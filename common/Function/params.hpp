#pragma once
// 设备参数 — 结构体定义与工具函数 (无命名空间, 无全局状态)

#include "flash_param.hpp"
#include <cstdint>
#include <cstring>

// ======================== 参数结构体 ========================
struct __attribute__((aligned(4))) DeviceParams {
	uint32_t magic; // 魔数 0x5041524D ("PARM") 用于校验是否已初始化
	uint16_t device_id; // 设备 ID, 默认 0x0001
	uint8_t baudrate_code; // 11=4800, 12=9600, 13=19200, 14=115200
	uint8_t reserved0;
	float ch0_ratio; // CH0 变比, 默认 1.0
	float ch1_ratio; // CH1 变比, 默认 1.0
	float ch0_threshold; // CH0 阈值, 默认 100.0
	float ch1_threshold; // CH1 阈值, 默认 100.0
	float ch2_threshold; // CH2 阈值, 默认 100.0
	bool use_factory_mode;
	uint8_t alarm_mode; // 01=主动上报, 02=不主动上报
	uint8_t report_interval; // 01=1s, 02=3s, 03=5s
	uint8_t reserved1[2];
	uint32_t crc32; // 简单校验
};

constexpr uint32_t PARAM_MAGIC = 0x5041524D; // "PARM"

// ======================== 工具函数 ========================

/// 波特率 code → 实际值
inline uint32_t baudrate_code_to_hz(uint8_t code)
{
	switch (code) {
	case 0x11:
		return 4800;
	case 0x12:
		return 9600;
	case 0x13:
		return 19200;
	case 0x14:
		return 115200;
	default:
		return 19200;
	}
}

/// 波特率实际值 → code
inline uint8_t baudrate_hz_to_code(uint32_t baud)
{
	switch (baud) {
	case 4800:
		return 0x11;
	case 9600:
		return 0x12;
	case 19200:
		return 0x13;
	case 115200:
		return 0x14;
	default:
		return 0x13;
	}
}

/// 简单 CRC32 用于参数校验
inline uint32_t params_crc32_calc(const uint8_t *data, uint32_t len)
{
	uint32_t crc = 0xFFFFFFFF;
	for (uint32_t i = 0; i < len; ++i) {
		crc ^= data[i];
		for (int j = 0; j < 8; ++j)
			crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
	}
	return ~crc;
}
