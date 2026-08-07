#pragma once
// 串口 ASCII 十六进制发送 — RS485 半双工

#include "hardware.hpp"
#include <cstdint>
#include <cstring>

/// 以 ASCII 十六进制形式发送二进制数据
inline void send_with_485(const uint8_t *binary, uint16_t len)
{
	static constexpr char hex_chars[] = "0123456789ABCDEF";
	uint8_t hex_buf[256];
	for (uint16_t i = 0; i < len; ++i) {
		hex_buf[i * 2] = hex_chars[(binary[i] >> 4) & 0x0F];
		hex_buf[i * 2 + 1] = hex_chars[binary[i] & 0x0F];
	}
	CS_485::set();
	USART1::transmit(hex_buf, len * 2);
	CS_485::clear();
}

/// 发送 ASCII 字符串
inline void send_with_485(const char *str)
{
	CS_485::set();
	USART1::transmit(reinterpret_cast<const uint8_t *>(str),
			 std::strlen(str));
	CS_485::clear();
}

/// 发送指定长度的原始 ASCII 数据 (不依赖 strlen)
inline void send_raw_485(const uint8_t *data, uint16_t len)
{
	CS_485::set();
	USART1::transmit(data, len);
	CS_485::clear();
}

/// 运行时切换波特率 (HAL 无运行时方法, 直接调 SPL)
inline void set_485_baudrate(uint32_t baudrate)
{
	usart_baudrate_set(HAL::gd32f4::registers::USART1_ADDR, baudrate);
}
