#pragma once
// 协议帧解析器 — 模板类 (header-only)

#include "protocol_types.hpp"
#include "hal.hpp" // CRC16_MODBUS
#include <cstdint>
#include <cstring>

/**
 * @brief 协议帧解析器 (模板参数化 ringbuffer 类型)
 *
 * @tparam RingBuffer  需提供 static uint32_t get_used() / static bool read_byte(uint8_t*)
 *
 * 职责:
 *  1. 从 ringbuffer 拉取 ASCII 十六进制字符流
 *  2. 识别帧边界 A5B6 ... B6A5
 *  3. ASCII hex → 二进制转换
 *  4. 长度字段校验
 *  5. CRC-16-Modbus 校验
 *
 * 通信规约: 所有协议帧按十六进制结构组帧后,以 ASCII 字符串形式在串口收发。
 * 例如帧头 0xA5B6 实际发送字符 'A','5','B','6' (0x41,0x35,0x42,0x36)。
 */
template <typename RingBuffer> class ProtocolParser {
    public:
	void init()
	{
		state_ = State::SEEKING_HEADER;
		ascii_pos_ = 0;
	}

	ProtocolStatus poll(ProtocolFrame &out_frame)
	{
		uint8_t byte;

		while (rb_read(byte)) {
			char c = static_cast<char>(byte);

			if (!is_hex(c)) {
				ascii_pos_ = 0;
				state_ = State::SEEKING_HEADER;
				continue;
			}

			if (!push_ascii(c)) {
				ascii_pos_ = 0;
				state_ = State::SEEKING_HEADER;
				continue;
			}

			switch (state_) {
			case State::SEEKING_HEADER:
				if (ascii_pos_ >= 4) {
					if (ascii_buf_[ascii_pos_ - 4] == 'A' &&
					    ascii_buf_[ascii_pos_ - 3] == '5' &&
					    ascii_buf_[ascii_pos_ - 2] == 'B' &&
					    ascii_buf_[ascii_pos_ - 1] == '6') {
						ascii_buf_[0] = 'A';
						ascii_buf_[1] = '5';
						ascii_buf_[2] = 'B';
						ascii_buf_[3] = '6';
						ascii_pos_ = 4;
						state_ = State::COLLECTING_TAIL;
					}
				}
				break;

			case State::COLLECTING_TAIL:
				if (ascii_pos_ >= 4) {
					if (ascii_buf_[ascii_pos_ - 4] == 'B' &&
					    ascii_buf_[ascii_pos_ - 3] == '6' &&
					    ascii_buf_[ascii_pos_ - 2] == 'A' &&
					    ascii_buf_[ascii_pos_ - 1] == '5') {
						ProtocolStatus s =
							decode_and_check(
								out_frame);
						ascii_pos_ = 0;
						state_ = State::SEEKING_HEADER;
						if (s ==
						    ProtocolStatus::frame_ready)
							return ProtocolStatus::
								frame_ready;
						return s;
					}
				}
				if (ascii_pos_ >= 600) {
					ascii_pos_ = 0;
					state_ = State::SEEKING_HEADER;
					return ProtocolStatus::length_error;
				}
				break;
			}
		}

		return ProtocolStatus::idle;
	}

	static void frame_to_ascii(const uint8_t *binary, uint16_t binary_size,
				   char *ascii_out, uint16_t &ascii_size)
	{
		static constexpr char hex_chars[] = "0123456789ABCDEF";
		ascii_size = 0;
		for (uint16_t i = 0; i < binary_size; ++i) {
			ascii_out[ascii_size++] =
				hex_chars[(binary[i] >> 4) & 0x0F];
			ascii_out[ascii_size++] = hex_chars[binary[i] & 0x0F];
		}
	}

    private:
	enum State : uint8_t {
		SEEKING_HEADER,
		COLLECTING_TAIL,
	};

	State state_ = State::SEEKING_HEADER;
	char ascii_buf_[1024]{};
	uint16_t ascii_pos_ = 0;

	static bool rb_read(uint8_t &byte)
	{
		if (RingBuffer::get_used() == 0)
			return false;
		return RingBuffer::read_byte(&byte);
	}

	static bool is_hex(char c)
	{
		return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') ||
		       (c >= 'a' && c <= 'f');
	}

	static bool hex_pair_to_byte(char high, char low, uint8_t &out)
	{
		auto nibble = [](char c) -> int {
			if (c >= '0' && c <= '9')
				return c - '0';
			if (c >= 'A' && c <= 'F')
				return c - 'A' + 10;
			if (c >= 'a' && c <= 'f')
				return c - 'a' + 10;
			return -1;
		};
		int h = nibble(high), l = nibble(low);
		if (h < 0 || l < 0)
			return false;
		out = static_cast<uint8_t>((h << 4) | l);
		return true;
	}

	bool push_ascii(char c)
	{
		if (ascii_pos_ >= sizeof(ascii_buf_))
			return false;
		ascii_buf_[ascii_pos_++] = c;
		return true;
	}

	ProtocolStatus decode_and_check(ProtocolFrame &out_frame)
	{
		uint16_t hex_len = ascii_pos_;
		if (hex_len & 1)
			return ProtocolStatus::invalid_hex;

		uint16_t binary_len = hex_len / 2;
		if (binary_len < 13)
			return ProtocolStatus::length_error;

		uint8_t binary[256];
		for (uint16_t i = 0; i < binary_len; ++i) {
			if (!hex_pair_to_byte(ascii_buf_[i * 2],
					      ascii_buf_[i * 2 + 1], binary[i]))
				return ProtocolStatus::invalid_hex;
		}

		uint16_t head = (static_cast<uint16_t>(binary[0]) << 8) |
				binary[1];
		uint16_t tail =
			(static_cast<uint16_t>(binary[binary_len - 2]) << 8) |
			binary[binary_len - 1];
		if (head != 0xA5B6 || tail != 0xB6A5)
			return ProtocolStatus::header_not_found;

		uint8_t content_len = binary[7];
		uint16_t expected = 13 + content_len;
		if (binary_len != expected)
			return ProtocolStatus::length_error;

		uint16_t crc_range = 9 + content_len;
		uint16_t calc_crc =
			HAL::gd32f4::CRC16_MODBUS::calculate(binary, crc_range);
		uint16_t recv_crc =
			(static_cast<uint16_t>(binary[9 + content_len]) << 8) |
			binary[10 + content_len];
		if (calc_crc != recv_crc)
			return ProtocolStatus::crc_error;

		out_frame.size = binary_len;
		for (uint16_t i = 0; i < binary_len; ++i)
			out_frame.data[i] = binary[i];

		return ProtocolStatus::frame_ready;
	}
};
