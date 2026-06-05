#include "protocol.hpp"
#include "hal.hpp" // CRC16_MODBUS

namespace Protocol
{
	namespace Response
	{

		// ======================== 内部辅助 ========================

		/// 写入 uint16 大端
		static void write_u16(uint8_t *buf, uint16_t val)
		{
			buf[0] = static_cast<uint8_t>(val >> 8);
			buf[1] = static_cast<uint8_t>(val & 0xFF);
		}

		/// 构建通用帧: 帧头(2) + ID(2) + 类型(1) + 命令字(2) + 长度(1) + 版本(1) + 内容(N) + CRC(2) + 帧尾(2)
		/// @return 二进制帧字节数
		static uint16_t build_generic(uint16_t device_id, uint8_t ftype, uint16_t cmd_word,
									  const uint8_t *content, uint8_t content_size,
									  uint8_t *out_data, uint16_t out_capacity)
		{
			// 总大小: 2+2+1+2+1+1+content_size+2+2 = 13 + content_size
			uint16_t total = 13 + content_size;
			if (total > out_capacity) return 0;

			uint8_t *p = out_data;

			// [0..1] 帧头
			write_u16(p, 0xA5B6);
			p += 2;

			// [2..3] 设备 ID
			write_u16(p, device_id);
			p += 2;

			// [4] 帧类型
			*p++ = ftype;

			// [5..6] 命令字
			write_u16(p, cmd_word);
			p += 2;

			// [7] 报文长度 (仅内容)
			*p++ = content_size;

			// [8] 协议版本
			*p++ = PROTOCOL_VERSION;

			// [9..9+N-1] 内容
			for (uint8_t i = 0; i < content_size; ++i)
			{
				*p++ = content[i];
			}

			// CRC-16-Modbus: 计算范围 [out_data[0] .. p-1] (即帧头到内容末尾)
			uint16_t crc_range = static_cast<uint16_t>(p - out_data);
			uint16_t crc	   = HAL::gd32f4::CRC16_MODBUS::calculate(out_data, crc_range);
			write_u16(p, crc);
			p += 2;

			// 帧尾
			write_u16(p, 0xB6A5);
			p += 2;

			return static_cast<uint16_t>(p - out_data);
		}

		// ======================== 公开接口 ========================

		uint16_t build_ok(uint16_t device_id, uint16_t cmd_word,
						  uint8_t *out_data, uint16_t out_capacity)
		{
			uint8_t ok = 0xFF;
			return build_generic(device_id, FTYPE_RSP, cmd_word, &ok, 1, out_data, out_capacity);
		}

		uint16_t build_error(uint16_t device_id,
							 uint8_t *out_data, uint16_t out_capacity)
		{
			// 错误帧: 类型 0xFF, 命令字 0xEEEE, 内容为空
			return build_generic(device_id, FTYPE_ERR, 0xEEEE, nullptr, 0, out_data, out_capacity);
		}

		uint16_t build_heartbeat(uint16_t device_id,
								 uint8_t *out_data, uint16_t out_capacity)
		{
			// 心跳帧: 类型 0x05, 命令字 0x8888, 内容为空
			return build_generic(device_id, FTYPE_HB, 0x8888, nullptr, 0, out_data, out_capacity);
		}

		uint16_t build_response(uint16_t device_id, uint16_t cmd_word,
								const uint8_t *content, uint8_t content_size,
								uint8_t *out_data, uint16_t out_capacity)
		{
			return build_generic(device_id, FTYPE_RSP, cmd_word, content, content_size, out_data, out_capacity);
		}

	} // namespace Response
} // namespace Protocol
