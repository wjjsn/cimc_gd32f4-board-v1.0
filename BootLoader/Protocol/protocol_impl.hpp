#pragma once
// protocol_impl.hpp — Parser 模板方法实现
// 由 protocol.hpp 末尾 include

#include "hal.hpp" // CRC16_MODBUS
#include <cstring> // memmove

namespace Protocol
{

	// ======================== Parser 实现 ========================

	template <typename RingBuffer>
	Protocol::Status Parser<RingBuffer>::poll(Frame &out_frame)
	{
		uint8_t byte;

		// 从 ringbuffer 拉取所有可用字节
		while (rb_read(byte))
		{
			char c = static_cast<char>(byte);

			// 只接受 ASCII 十六进制可打印字符
			if (!is_hex(c))
			{
				// 非法字符: 丢弃当前帧, 重新同步
				ascii_pos_ = 0;
				state_	   = State::SEEKING_HEADER;
				continue;
			}

			if (!push_ascii(c))
			{
				// 缓冲区溢出, 重置
				ascii_pos_ = 0;
				state_	   = State::SEEKING_HEADER;
				continue;
			}

			switch (state_)
			{
				case State::SEEKING_HEADER:
					// 至少需要 4 个字符 "A5B6"
					if (ascii_pos_ >= 4)
					{
						// 检查末尾 4 字符是否为 "A5B6"
						if (ascii_buf_[ascii_pos_ - 4] == 'A' &&
							ascii_buf_[ascii_pos_ - 3] == '5' &&
							ascii_buf_[ascii_pos_ - 2] == 'B' &&
							ascii_buf_[ascii_pos_ - 1] == '6')
						{
							// 找到帧头, 将 "A5B6" 移到缓冲开头
							ascii_buf_[0] = 'A';
							ascii_buf_[1] = '5';
							ascii_buf_[2] = 'B';
							ascii_buf_[3] = '6';
							ascii_pos_	  = 4;
							state_		  = State::COLLECTING_TAIL;
						}
					}
					break;

				case State::COLLECTING_TAIL:
					// 至少需要 4 个字符才能检查帧尾 "B6A5"
					if (ascii_pos_ >= 4)
					{
						if (ascii_buf_[ascii_pos_ - 4] == 'B' &&
							ascii_buf_[ascii_pos_ - 3] == '6' &&
							ascii_buf_[ascii_pos_ - 2] == 'A' &&
							ascii_buf_[ascii_pos_ - 1] == '5')
						{
							// 找到帧尾, 尝试解码
							Status s   = decode_and_check(out_frame);
							ascii_pos_ = 0;
							state_	   = State::SEEKING_HEADER;
							if (s == Status::frame_ready)
							{
								return Status::frame_ready;
							}
							// CRC/length 错误也返回对应状态
							return s;
						}
					}
					// 帧太长 (ASCII 字符超过 2 * (256+10) ≈ 532)
					if (ascii_pos_ >= 600)
					{
						ascii_pos_ = 0;
						state_	   = State::SEEKING_HEADER;
						return Status::length_error;
					}
					break;
			}
		}

		return Status::idle;
	}

	template <typename RingBuffer>
	void Parser<RingBuffer>::frame_to_ascii(const uint8_t *binary, uint16_t binary_size,
											char *ascii_out, uint16_t &ascii_size)
	{
		static constexpr char hex_chars[] = "0123456789ABCDEF";
		ascii_size						  = 0;
		for (uint16_t i = 0; i < binary_size; ++i)
		{
			ascii_out[ascii_size++] = hex_chars[(binary[i] >> 4) & 0x0F];
			ascii_out[ascii_size++] = hex_chars[binary[i] & 0x0F];
		}
	}

	template <typename RingBuffer>
	bool Parser<RingBuffer>::is_hex(char c)
	{
		return (c >= '0' && c <= '9') ||
			   (c >= 'A' && c <= 'F') ||
			   (c >= 'a' && c <= 'f');
	}

	template <typename RingBuffer>
	bool Parser<RingBuffer>::hex_pair_to_byte(char high, char low, uint8_t &out)
	{
		auto nibble = [](char c) -> int
		{
			if (c >= '0' && c <= '9') return c - '0';
			if (c >= 'A' && c <= 'F') return c - 'A' + 10;
			if (c >= 'a' && c <= 'f') return c - 'a' + 10;
			return -1;
		};
		int h = nibble(high);
		int l = nibble(low);
		if (h < 0 || l < 0) return false;
		out = static_cast<uint8_t>((h << 4) | l);
		return true;
	}

	template <typename RingBuffer>
	bool Parser<RingBuffer>::push_ascii(char c)
	{
		if (ascii_pos_ >= sizeof(ascii_buf_)) return false;
		ascii_buf_[ascii_pos_++] = c;
		return true;
	}

	template <typename RingBuffer>
	Protocol::Status Parser<RingBuffer>::decode_and_check(Frame &out_frame)
	{
		// ascii_pos_ 必须是偶数 (两个 ASCII 字符 = 1 字节)
		uint16_t hex_len = ascii_pos_;
		if (hex_len & 1) return Status::invalid_hex;

		uint16_t binary_len = hex_len / 2;

		// 最小帧: 帧头(4) + 设备ID(4) + 帧类型(2) + 命令字(4) + 报文长度(2) + 协议版本(2) + CRC(4) + 帧尾(4) = 26 hex chars = 13 bytes
		if (binary_len < 13) return Status::length_error;

		// 转换为二进制
		uint8_t binary[256];
		for (uint16_t i = 0; i < binary_len; ++i)
		{
			if (!hex_pair_to_byte(ascii_buf_[i * 2], ascii_buf_[i * 2 + 1], binary[i]))
			{
				return Status::invalid_hex;
			}
		}

		// 解析各字段 (大端序)
		// [0..1] 起始标志  0xA5B6
		// [2..3] 设备 ID
		// [4]    帧类型
		// [5..6] 命令字
		// [7]    报文长度 (仅内容 N 字节)
		// [8]    协议版本
		// [9..9+N-1] 内容 (N 字节)
		// [9+N..10+N] CRC16 (大端)
		// [11+N..12+N] 结束标志 0xB6A5

		// 检查帧头帧尾
		uint16_t head = (static_cast<uint16_t>(binary[0]) << 8) | binary[1];
		uint16_t tail = (static_cast<uint16_t>(binary[binary_len - 2]) << 8) | binary[binary_len - 1];
		if (head != 0xA5B6 || tail != 0xB6A5)
		{
			return Status::header_not_found;
		}

		uint8_t content_len = binary[7]; // 报文长度 (N)
		(void)binary[8];				 // 协议版本 (暂存)

		// 验证总长度: 帧头(2) + ID(2) + 类型(1) + 命令字(2) + 长度(1) + 版本(1) + 内容(N) + CRC(2) + 帧尾(2)
		//           = 9 + N + 4 = 13 + N
		uint16_t expected_binary_len = 13 + content_len;
		if (binary_len != expected_binary_len)
		{
			return Status::length_error;
		}

		// CRC-16-Modbus 校验
		// 计算范围: 起始标志(含) 到 内容末尾(含) = binary[0..9+N-1]
		uint16_t crc_range_len	= 9 + content_len; // 帧头(2)+ID(2)+类型(1)+命令字(2)+长度(1)+版本(1)+内容(N)
		uint16_t calculated_crc = HAL::gd32f4::CRC16_MODBUS::calculate(binary, crc_range_len);

		// CRC 在 binary[9+N] 和 binary[10+N], 大端
		uint16_t received_crc = (static_cast<uint16_t>(binary[9 + content_len]) << 8) |
								binary[10 + content_len];

		if (calculated_crc != received_crc)
		{
			return Status::crc_error;
		}

		// 输出完整二进制帧
		out_frame.size = binary_len;
		for (uint16_t i = 0; i < binary_len; ++i)
		{
			out_frame.data[i] = binary[i];
		}

		return Status::frame_ready;
	}

} // namespace Protocol
