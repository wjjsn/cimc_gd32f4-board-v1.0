#pragma once
// 应答帧构建器 — 纯静态方法类 (header-only)

#include <cstdint>
#include "hal.hpp" // CRC16_MODBUS

/// 提供常用协议的组帧方法 (全静态)
class ResponseBuilder {
    public:
	/// 帧类型常量
	static constexpr uint8_t FTYPE_CMD = 0x01; // 上位机 → 设备
	static constexpr uint8_t FTYPE_RSP = 0x02; // 设备 → 上位机 (应答)
	static constexpr uint8_t FTYPE_HB = 0x05; // 心跳
	static constexpr uint8_t FTYPE_ERR = 0xFF; // 错误应答

	/// 协议版本
	static constexpr uint8_t PROTOCOL_VERSION = 0x02;

	/// 帧头帧尾
	static constexpr uint16_t FRAME_HEAD = 0xA5B6;
	static constexpr uint16_t FRAME_TAIL = 0xB6A5;

	// ========== 公开接口 (inline 实现) ==========

	static inline uint16_t build_ok(uint16_t device_id, uint16_t cmd_word,
					uint8_t *out_data,
					uint16_t out_capacity)
	{
		uint8_t ok = 0xFF;
		return build_generic(device_id, FTYPE_RSP, cmd_word, &ok, 1,
				     out_data, out_capacity);
	}

	static inline uint16_t build_error(uint16_t device_id,
					   uint8_t *out_data,
					   uint16_t out_capacity)
	{
		return build_generic(device_id, FTYPE_ERR, 0xEEEE, nullptr, 0,
				     out_data, out_capacity);
	}

	static inline uint16_t build_heartbeat(uint16_t device_id,
					       uint8_t *out_data,
					       uint16_t out_capacity)
	{
		return build_generic(device_id, FTYPE_HB, 0x8888, nullptr, 0,
				     out_data, out_capacity);
	}

	static inline uint16_t
	build_response(uint16_t device_id, uint16_t cmd_word,
		       const uint8_t *content, uint8_t content_size,
		       uint8_t *out_data, uint16_t out_capacity)
	{
		return build_generic(device_id, FTYPE_RSP, cmd_word, content,
				     content_size, out_data, out_capacity);
	}

    private:
	static inline void write_u16(uint8_t *buf, uint16_t val)
	{
		buf[0] = static_cast<uint8_t>(val >> 8);
		buf[1] = static_cast<uint8_t>(val & 0xFF);
	}

	static inline uint16_t
	build_generic(uint16_t device_id, uint8_t ftype, uint16_t cmd_word,
		      const uint8_t *content, uint8_t content_size,
		      uint8_t *out_data, uint16_t out_capacity)
	{
		uint16_t total = 13 + content_size;
		if (total > out_capacity)
			return 0;

		uint8_t *p = out_data;
		write_u16(p, 0xA5B6);
		p += 2;
		write_u16(p, device_id);
		p += 2;
		*p++ = ftype;
		write_u16(p, cmd_word);
		p += 2;
		*p++ = content_size;
		*p++ = PROTOCOL_VERSION;
		for (uint8_t i = 0; i < content_size; ++i)
			*p++ = content[i];

		uint16_t crc_range = static_cast<uint16_t>(p - out_data);
		uint16_t crc = HAL::gd32f4::CRC16_MODBUS::calculate(out_data,
								    crc_range);
		write_u16(p, crc);
		p += 2;
		write_u16(p, 0xB6A5);
		p += 2;

		return static_cast<uint16_t>(p - out_data);
	}
};
