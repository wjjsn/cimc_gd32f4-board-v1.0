#pragma once
// 设备参数管理 — 参数结构体、默认值、Flash 持久化

#include "Driver/flash_param.hpp"
#include <cstdint>
#include <cmath>
#include <cstring>

// ======================== 参数结构体 (16 字节对齐, 便于 Flash 存储) ========================
struct __attribute__((aligned(4))) DeviceParams
{
	uint32_t magic;		   // 魔数 0x5041524D ("PARM") 用于校验是否已初始化
	uint16_t device_id;	   // 设备 ID, 默认 0x0001
	uint8_t baudrate_code; // 11=4800, 12=9600, 13=19200, 14=115200
	uint8_t reserved0;
	float ch0_ratio;		 // CH0 变比, 默认 1.0
	float ch1_ratio;		 // CH1 变比, 默认 1.0
	float ch0_threshold;	 // CH0 阈值, 默认 100.0
	float ch1_threshold;	 // CH1 阈值, 默认 100.0
	float ch2_threshold;	 // CH2 阈值, 默认 100.0
	uint8_t alarm_mode;		 // 01=主动上报, 02=不主动上报
	uint8_t report_interval; // 01=1s, 02=3s, 03=5s
	uint8_t reserved1[2];
	uint32_t crc32; // 简单校验 (结构体末尾)
};

constexpr uint32_t PARAM_MAGIC = 0x5041524D; // "PARM"

namespace Params
{

	/// 全局参数实例
	inline DeviceParams g_params;

	/// 波特率 code → 实际值
	inline uint32_t code_to_baudrate(uint8_t code)
	{
		switch (code)
		{
			case 11:
				return 4800;
			case 12:
				return 9600;
			case 13:
				return 19200;
			case 14:
				return 115200;
			default:
				return 19200;
		}
	}

	/// 波特率实际值 → code
	inline uint8_t baudrate_to_code(uint32_t baud)
	{
		switch (baud)
		{
			case 4800:
				return 11;
			case 9600:
				return 12;
			case 19200:
				return 13;
			case 115200:
				return 14;
			default:
				return 13;
		}
	}

	/// 简单 CRC32 用于参数校验
	inline uint32_t crc32_calc(const uint8_t *data, uint32_t len)
	{
		uint32_t crc = 0xFFFFFFFF;
		for (uint32_t i = 0; i < len; ++i)
		{
			crc ^= data[i];
			for (int j = 0; j < 8; ++j)
				crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
		}
		return ~crc;
	}

	/// 加载默认参数
	inline void set_defaults()
	{
		g_params.magic			 = PARAM_MAGIC;
		g_params.device_id		 = 0x0001;
		g_params.baudrate_code	 = 13; // 19200
		g_params.reserved0		 = 0;
		g_params.ch0_ratio		 = 1.0f;
		g_params.ch1_ratio		 = 1.0f;
		g_params.ch0_threshold	 = 100.0f;
		g_params.ch1_threshold	 = 100.0f;
		g_params.ch2_threshold	 = 100.0f;
		g_params.alarm_mode		 = 0x02; // 不主动上报
		g_params.report_interval = 0x01; // 1s
		g_params.reserved1[0]	 = 0;
		g_params.reserved1[1]	 = 0;
		g_params.crc32			 = 0;
	}

	/// 保存参数到 Flash
	inline void save()
	{
		g_params.crc32 = crc32_calc(reinterpret_cast<const uint8_t *>(&g_params),
									sizeof(DeviceParams) - sizeof(uint32_t));
		FlashParam::save(g_params);
	}

	/// 从 Flash 加载参数, 如果无效则恢复默认
	inline void load()
	{
		FlashParam::load(g_params);
		// 校验 magic 和 crc
		if (g_params.magic != PARAM_MAGIC)
		{
			set_defaults();
			save();
			return;
		}
		uint32_t calc = crc32_calc(reinterpret_cast<const uint8_t *>(&g_params),
								   sizeof(DeviceParams) - sizeof(uint32_t));
		if (calc != g_params.crc32)
		{
			set_defaults();
			save();
		}
	}

} // namespace Params
