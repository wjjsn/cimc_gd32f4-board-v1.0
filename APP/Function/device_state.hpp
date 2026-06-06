#pragma once
// 设备运行时状态 — OLED 显示、LED 指示灯、采样状态

#include "hardware.hpp"
#include "params.hpp"
#include "alarm.hpp"
#include "Driver/serial_send.hpp"
#include "Protocol/protocol.hpp"
#include <cstdint>
#include <cstdio>
#include <cmath>

// OLED 实例 (定义在 main.cpp)
extern Screen g_screen;

namespace DeviceState
{

	// ===================== OLED =====================
	constexpr char TEAM_ID[] = "CIMC2026";

	enum class OLEDStatus : uint8_t
	{
		BOOTLOADER	= 0,
		IDLE		= 1,
		AUTO_SAMPLE = 2,
	};

	inline OLEDStatus g_oled_status = OLEDStatus::IDLE;

	// 告警触发标志 (true 表示当前处于告警状态，用于上升沿触发)
	inline volatile bool g_ch0_alarm_active = false;
	inline volatile bool g_ch1_alarm_active = false;
	
	// ===================== LED =====================
	// 系统状态灯: 进入 APP 后 1s 闪烁
	// 采集工作灯: 自动采集时常亮, 否则熄灭
	inline bool g_led_on	   = false;
	inline bool g_is_sampling  = false; // 是否正在自动采集
	inline uint32_t g_led_tick = 0;

	// ===================== 自动上报 =====================
	inline bool g_auto_report_active		  = false;
	inline uint32_t g_auto_report_next		  = 0; // 下次上报的 tick
	inline uint32_t g_auto_report_interval_ms = 1000;


	/// 刷新 OLED 双行显示
	inline void oled_update()
	{
		static OLEDStatus last_status = static_cast<OLEDStatus>(255); // 强制首次刷新

		if (g_oled_status == last_status) return;
		last_status = g_oled_status;

		g_screen.chear();
		g_screen.printf(0, 0, "%s", TEAM_ID);
		const char *line2 = "IDLE";
		switch (g_oled_status)
		{
			case OLEDStatus::BOOTLOADER:
				line2 = "Bootloader";
				break;
			case OLEDStatus::IDLE:
				line2 = "IDLE";
				break;
			case OLEDStatus::AUTO_SAMPLE:
				line2 = "AutoSample";
				break;
			default:
				break;
		}
		g_screen.printf(2, 0, "%s", line2);
		g_screen.update_force();
	}

	/// 刷新 LED 指示灯
	inline void led_update()
	{
		// // 采集工作灯
		// if (g_is_sampling)
		// {
		// 	LED::set();
		// }
		// else
		// {
		// 	// 系统状态灯: 1s 闪烁 (500ms on, 500ms off)
		// 	// LED::set/clear 由外部 tick 控制
		// }
	}

	/// 系统 LED 闪烁 (每 500ms 调用一次)
	inline void led_toggle()
	{
		// if (g_is_sampling)
		// {
		// 	// 采集工作指示灯常亮
		// 	LED::set();
		// }
		// else
		// {
		// 	// 系统状态灯: 1s 闪烁 (500ms on, 500ms off)
		// 	g_led_on = !g_led_on;
		// 	if (g_led_on)
		// 		LED::set();
		// 	else
		// 		LED::clear();
		// }
	}

	// ===================== 采样数据 =====================
	inline float g_ch0_raw	= 0.0f; // ADC0 PC0 原始电压
	inline float g_ch1_raw	= 0.0f; // ADC0 PC1 DAC回读原始电压
	inline float g_ch2_temp = 0.0f; // PT100 温度

	// ===================== RTC UTC 时间戳 =====================
	inline uint32_t g_utc_timestamp = 0;

	/// 获取当前 UTC 时间戳 (由外部定期更新)
	inline uint32_t get_utc()
	{
		return g_utc_timestamp;
	}

	/// 设置 UTC 时间戳
	inline void set_utc(uint32_t ts)
	{
		g_utc_timestamp = ts;
	}

	// ===================== 睡眠状态 =====================
	inline bool g_sleeping = false;

	/// IEEE 754 float → 4 字节大端
	inline void float_to_bytes(float val, uint8_t *out)
	{
		uint32_t bits;
		std::memcpy(&bits, &val, 4);
		out[0] = (bits >> 24) & 0xFF;
		out[1] = (bits >> 16) & 0xFF;
		out[2] = (bits >> 8) & 0xFF;
		out[3] = bits & 0xFF;
	}

	/// 4 字节大端 → IEEE 754 float
	inline float bytes_to_float(const uint8_t *bytes)
	{
		uint32_t bits = (static_cast<uint32_t>(bytes[0]) << 24) |
						(static_cast<uint32_t>(bytes[1]) << 16) |
						(static_cast<uint32_t>(bytes[2]) << 8) |
						bytes[3];
		float val;
		std::memcpy(&val, &bits, 4);
		return val;
	}

	/// 读取 uint16 大端
	inline uint16_t read_u16(const uint8_t *buf)
	{
		return (static_cast<uint16_t>(buf[0]) << 8) | buf[1];
	}

	/// 写入 uint16 大端
	inline void write_u16(uint8_t *buf, uint16_t val)
	{
		buf[0] = (val >> 8) & 0xFF;
		buf[1] = val & 0xFF;
	}

} // namespace DeviceState
