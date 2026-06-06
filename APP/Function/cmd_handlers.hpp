#pragma once
// 命令分发器 — 解析帧, 调用对应 handler, 组应答帧

#include "../Protocol/protocol.hpp"
#include "params.hpp"
#include "alarm.hpp"
#include "device_state.hpp"
#include "../Driver/serial_send.hpp"
#include "../Driver/dac_driver.hpp"
#include "hardware.hpp"
#include <cstdio>
#include <cmath>


// GD30AD3340 全局实例 (定义在 main.cpp)
extern ADC g_adc;

namespace CmdDispatch
{

	using namespace Protocol;
	using namespace DeviceState;

	// ==================== 内部辅助函数 ====================

	/// 发送二进制帧 (组帧 → hex ASCII 发送)
	inline void send_frame(const uint8_t *binary, uint16_t size)
	{
		send_with_485(binary, size);
	}

	/// 发送 OK 应答
	inline void send_ok(uint16_t device_id, uint16_t cmd_word)
	{
		uint8_t buf[64];
		uint16_t sz = Response::build_ok(device_id, cmd_word, buf, sizeof(buf));
		if (sz) send_frame(buf, sz);
	}

	/// 发送错误应答
	inline void send_error(uint16_t device_id)
	{
		uint8_t buf[64];
		uint16_t sz = Response::build_error(device_id, buf, sizeof(buf));
		if (sz) send_frame(buf, sz);
	}

	/// 发送含内容的应答
	inline void send_response(uint16_t device_id, uint16_t cmd_word,
							  const uint8_t *content, uint8_t content_size)
	{
		uint8_t buf[256];
		uint16_t sz = Response::build_response(device_id, cmd_word, content, content_size, buf, sizeof(buf));
		if (sz) send_frame(buf, sz);
	}

	/// 从帧数据中提取字段 (帧格式: head(2)+ID(2)+type(1)+cmd(2)+len(1)+ver(1)+content(N)+crc(2)+tail(2))
	inline uint16_t frame_cmd(const Frame &f)
	{
		return read_u16(&f.data[5]);
	}
	inline uint8_t frame_type(const Frame &f)
	{
		return f.data[4];
	}
	inline uint16_t frame_devid(const Frame &f)
	{
		return read_u16(&f.data[2]);
	}
	inline uint8_t frame_content_len(const Frame &f)
	{
		return f.data[7];
	}
	inline const uint8_t *frame_content(const Frame &f)
	{
		return &f.data[9];
	}

	/// 读取 CH0 原始 ADC 值并换算电压
	inline float read_ch0()
	{
		ADC0::set_channel(ADC_CHANNEL_10);
		uint16_t raw = ADC0::get_value();
		return raw * 3.3f / 4095.0f; // 12-bit, 3.3V 参考
	}

	/// 读取 CH1 (DAC 回读, PC1)
	inline float read_ch1()
	{
		ADC0::set_channel(ADC_CHANNEL_11);
		uint16_t raw = ADC0::get_value();
		return raw * 3.3f / 4095.0f;
	}

	/// 读取 CH2 (PT100 温度, 外部 ADC)
	inline float read_ch2()
	{
		int16_t raw = g_adc.read_raw();
		// GD30AD3340: PGA=±2.048V, 16-bit signed, 量程 ±2048mV
		if(raw<0){
			raw=0;
		}
		// 转换为电压, 再按 PT100 分压计算温度 (简化: 直接返回 raw 转电压)
		float voltage = ((float)raw / 32768.0f)* 2.048f;
		// PT100 温度近似: 假设分压电路 0°C=0V, 每°C ≈ 0.008V (简化)
		return voltage * voltage * -6.91f + 268.66f * voltage - 281.28f; // 二次线性；检测+-4度内
	}
        inline uint8_t bcd_to_dec(uint8_t val) {
          return ((val >> 4) * 10) + (val & 0x0F);
        }
        /// RTC 时间 → UTC 时间戳 (简化 mktime)
        inline uint32_t rtc_to_utc() {
          // 获取 RTC 硬件时间 (BCD 格式)
          auto t = RTC::get_time();

          // 1. 将 BCD 码转换为普通十进制
          uint16_t year = bcd_to_dec(t.year) + 2000; // 假设 26 转换为 2026
          uint8_t month = bcd_to_dec(t.month);
          uint8_t day = bcd_to_dec(t.date);
          uint8_t hour = bcd_to_dec(t.hour);
          uint8_t min = bcd_to_dec(t.minute);
          uint8_t sec = bcd_to_dec(t.second);

          // 2. 计算从 1970 年到当前年份前一年的总天数
          uint32_t total_days = 0;
          for (uint16_t y = 1970; y < year; y++) {
            bool is_leap = (y % 4 == 0 && (y % 100 != 0 || y % 400 == 0));
            total_days += is_leap ? 366 : 365;
          }

          // 3. 累加今年过去月份的天数
          bool current_is_leap =
              (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
          const uint8_t days_in_month[] = {
              31, (uint8_t)(current_is_leap ? 29 : 28),
              31, 30,
              31, 30,
              31, 31,
              30, 31,
              30, 31};

          // month 的范围是 1 ~ 12，累加到 month - 1 即可
          for (int m = 0; m < month - 1; m++) {
            total_days += days_in_month[m];
          }

          // 4. 累加当月的当前天数 (注意：day 是从 1 开始的，算过去的天数要减 1)
          total_days += (day - 1);

          // 5. 将天数转换为秒，并累加当天的时、分、秒
          uint32_t utc_timestamp = (total_days * 86400) +
                                   ((uint32_t)hour * 3600) +
                                   ((uint32_t)min * 60) + sec;

          return utc_timestamp;
        }

        inline uint8_t dec_to_bcd(uint8_t val) {
          return ((val / 10) << 4) | (val % 10);
        }
        inline auto utc_to_rtc_bcd(uint32_t utc) {
          // 1. 计算星期几 (1970年1月1日是星期四)
          // 星期通常不需要特殊 BCD 转换（0-6），但为了统一也可以过一遍
          uint8_t wday = (utc / 86400 + 4) % 7;

          // 2. 计算时、分、秒
          uint32_t time_of_day = utc % 86400;
          uint8_t hour = time_of_day / 3600;
          uint8_t minute = (time_of_day % 3600) / 60;
          uint8_t second = time_of_day % 60;

          // 3. 计算天数与年份
          uint32_t days = utc / 86400;
          uint16_t year = 1970;

          while (true) {
            bool is_leap =
                (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
            uint16_t days_in_year = is_leap ? 366 : 365;
            if (days >= days_in_year) {
              days -= days_in_year;
              year++;
            } else {
              break;
            }
          }

          // 4. 计算月份与日期
          bool is_leap =
              (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
          const uint8_t days_in_month[] = {31, (uint8_t)(is_leap ? 29 : 28),
                                           31, 30,
                                           31, 30,
                                           31, 31,
                                           30, 31,
                                           30, 31};

          uint8_t month = 0;
          while (days >= days_in_month[month]) {
            days -= days_in_month[month];
            month++;
          }

          uint8_t month_dec = month + 1; // 1-12 月
          uint8_t date_dec = days + 1;   // 1-31 日

          // RTC 硬件通常只取年份的后两位（如 2026 年取 26）
          uint8_t year_short = year % 100;

          // 5. 构造并返回 BCD 格式的 RTC_Time
          HAL::gd32f4::RTC_Time rtc;
          rtc.year = dec_to_bcd(year_short);
          rtc.month = dec_to_bcd(month_dec);
          rtc.date = dec_to_bcd(date_dec);
          rtc.week =
              dec_to_bcd(wday); // 注：部分 RTC 的星期范围是
                                // 1-7（1是周一或周日），如需调整请在此处修改
          rtc.hour = dec_to_bcd(hour);
          rtc.minute = dec_to_bcd(minute);
          rtc.second = dec_to_bcd(second);

          return rtc;
        }
        /// UTC 时间戳 → 设置 RTC
        inline void set_rtc_time(uint32_t utc) {
          auto time = utc_to_rtc_bcd(utc);
          RTC::set_time(time.year, time.month, time.date, time.week, time.hour,
                        time.minute, time.second);
        }

        // ==================== 命令处理函数 ====================

        // 0x0101 — 设备重启
	inline void cmd_0101_reboot(const Frame &f)
	{
		uint16_t devid = frame_devid(f);
		send_ok(devid, 0x0101);
		// 简单重启: 跳转到复位向量
		__disable_irq();
		NVIC_SystemReset();
	}

	// 0x0104 — 查询固件版本
	inline void cmd_0104_fw_version(const Frame &f)
	{
		uint16_t devid = frame_devid(f);
		uint8_t ver[4] = {0x02, 0x00, 0x01, 0x00}; // 2.0.1.0
		send_response(devid, 0x0104, ver, 4);
	}

	// 0x0105 — 设置设备时间
	inline void cmd_0105_set_time(const Frame &f)
	{
		uint16_t devid = frame_devid(f);
		uint8_t clen   = frame_content_len(f);
		if (clen >= 4)
		{
			const uint8_t *c = frame_content(f);
			uint32_t utc	 = (static_cast<uint32_t>(c[0]) << 24) |
						   (static_cast<uint32_t>(c[1]) << 16) |
						   (static_cast<uint32_t>(c[2]) << 8) |
						   c[3];
                        set_rtc_time(utc);
                        send_ok(devid, 0x0105);
		}
		else
			send_error(devid);
	}

	// 0x0106 — 查询设备时间
	inline void cmd_0106_get_time(const Frame &f)
	{
		uint16_t devid = frame_devid(f);
		uint32_t utc   = rtc_to_utc();
		uint8_t buf[4];
		buf[0] = (utc >> 24) & 0xFF;
		buf[1] = (utc >> 16) & 0xFF;
		buf[2] = (utc >> 8) & 0xFF;
		buf[3] = utc & 0xFF;
		send_response(devid, 0x0106, buf, 4);
	}

	// 0x01A1 — 设置设备 ID
	inline void cmd_01A1_set_id(const Frame &f)
	{
		uint16_t devid = frame_devid(f);
		uint8_t clen   = frame_content_len(f);
		if (clen >= 2)
		{
			const uint8_t *c = frame_content(f);
			uint16_t new_id	 = read_u16(c);
			if (new_id >= 0x0001 && new_id <= 0xFFFE)
			{
				Params::g_params.device_id = new_id;
				Params::save();
				// 注意: 应答使用新 ID
				send_ok(new_id, 0x01A1);
				return;
			}
		}
		send_error(devid);
	}

	// 0x01A2 — 设置波特率
	inline void cmd_01A2_set_baudrate(const Frame &f)
	{
		uint16_t devid = frame_devid(f);
		uint8_t clen   = frame_content_len(f);
		if (clen >= 1)
		{
			uint8_t code = frame_content(f)[0];
			if (code >= 0x11 && code <= 0x14)
			{
				Params::g_params.baudrate_code = code;
				Params::save();
				send_ok(devid, 0x01A2);
				// 应答后切换波特率
				for (int i = 0; i < 100000; ++i) __asm__ volatile("nop");
				set_485_baudrate(Params::code_to_baudrate(code));
				return;
			}
		}
		send_error(devid);
	}

	// 0x0111 — 查询设备 ID (广播)
	inline void cmd_0111_get_id(const Frame &f)
	{
		(void)f;
		uint16_t devid = Params::g_params.device_id;
		uint8_t buf[2];
		write_u16(buf, devid);
		send_response(devid, 0x0111, buf, 2);
	}

	// 0x0112 — 查询波特率
	inline void cmd_0112_get_baudrate(const Frame &f)
	{
		uint16_t devid = frame_devid(f);
		uint8_t code   = Params::g_params.baudrate_code;
		send_response(devid, 0x0112, &code, 1);
	}

	// 0x0201 — 查询 CH0 数据
	inline void cmd_0201_ch0(const Frame &f)
	{
		uint16_t devid = frame_devid(f);
		float raw	   = read_ch0();
		float value	   = raw * Params::g_params.ch0_ratio;
		uint8_t buf[4];
		float_to_bytes(value, buf);
		send_response(devid, 0x0201, buf, 4);
	}

	// 0x0202 — 查询 CH1 数据
	inline void cmd_0202_ch1(const Frame &f)
	{
		uint16_t devid = frame_devid(f);
		float raw	   = read_ch1();
		float value	   = raw * Params::g_params.ch1_ratio;
		uint8_t buf[4];
		float_to_bytes(value, buf);
		send_response(devid, 0x0202, buf, 4);
	}

	// 0x0221 — 查询 CH2 数据 (PT100)
	inline void cmd_0221_ch2(const Frame &f)
	{
		uint16_t devid = frame_devid(f);
		float value	   = read_ch2();
		uint8_t buf[4];
		float_to_bytes(value, buf);
		send_response(devid, 0x0221, buf, 4);
	}

	// 0x0241 — 设置 CH0 变比
	inline void cmd_0241_set_ch0_ratio(const Frame &f)
	{
		uint16_t devid = frame_devid(f);
		if (frame_content_len(f) >= 4)
		{
			Params::g_params.ch0_ratio = bytes_to_float(frame_content(f));
			Params::save();
			send_ok(devid, 0x0241);
		}
		else
			send_error(devid);
	}

	// 0x0242 — 设置 CH1 变比
	inline void cmd_0242_set_ch1_ratio(const Frame &f)
	{
		uint16_t devid = frame_devid(f);
		if (frame_content_len(f) >= 4)
		{
			Params::g_params.ch1_ratio = bytes_to_float(frame_content(f));
			Params::save();
			send_ok(devid, 0x0242);
		}
		else
			send_error(devid);
	}

	// 0x0261 — 设置上报间隔
	inline void cmd_0261_set_interval(const Frame &f)
	{
		uint16_t devid = frame_devid(f);
		if (frame_content_len(f) >= 1)
		{
			uint8_t code = frame_content(f)[0];
			if (code >= 1 && code <= 3)
			{
				Params::g_params.report_interval = code;
				Params::save();
				g_auto_report_interval_ms = (code == 1) ? 1000 : (code == 2) ? 3000
																			 : 5000;
				send_ok(devid, 0x0261);
			}
			else
				send_error(devid);
		}
		else
			send_error(devid);
	}

	// 0x0301 — 设置 DAC 电压
	inline void cmd_0301_set_dac(const Frame &f)
	{
		uint16_t devid = frame_devid(f);
		if (frame_content_len(f) >= 2)
		{
			const uint8_t *c = frame_content(f);
			uint16_t val	 = read_u16(c);
			if (val <= 4095)
			{
				dac_set(val);
				send_ok(devid, 0x0301);
			}
			else
				send_error(devid);
		}
		else
			send_error(devid);
	}

	// 0x0302 — 开始自动上报
	inline void cmd_0302_start_auto(const Frame &f)
	{
		uint16_t devid		 = frame_devid(f);
		g_auto_report_active = true;
		g_auto_report_next	 = 0; // 立即上报首帧
		g_oled_status		 = OLEDStatus::AUTO_SAMPLE;
		g_is_sampling		 = true;

		// 发送首帧: 12B = UTC(4) + CH0(4) + CH1(4)
		uint32_t utc = rtc_to_utc();
		float ch0	 = read_ch0() * Params::g_params.ch0_ratio;
		float ch1	 = read_ch1() * Params::g_params.ch1_ratio;
		uint8_t buf[12];
		buf[0] = (utc >> 24) & 0xFF;
		buf[1] = (utc >> 16) & 0xFF;
		buf[2] = (utc >> 8) & 0xFF;
		buf[3] = utc & 0xFF;
		float_to_bytes(ch0, buf + 4);
		float_to_bytes(ch1, buf + 8);
		send_response(devid, 0x0302, buf, 12);
	}

	// 0x0303 — 停止自动上报
	inline void cmd_0303_stop_auto(const Frame &f)
	{
		uint16_t devid		 = frame_devid(f);
		g_auto_report_active = false;
		g_oled_status		 = OLEDStatus::IDLE;
		g_is_sampling		 = false;
		send_ok(devid, 0x0303);
	}

	// 0x03AA — 进入睡眠模式
	inline void cmd_03AA_sleep(const Frame &f)
	{
		uint16_t devid = frame_devid(f);
		send_ok(devid, 0x03AA);
		for (int i = 0; i < 100000; ++i) __asm__ volatile("nop");

		// RTC 闹钟: 10 秒后唤醒, 仅匹配秒字段
		rtc_alarm_disable(RTC_ALARM0);
		rtc_alarm_struct alarm = {};
		auto t				   = RTC::get_time();
		uint8_t next_sec	   = (t.second + 10) % 60;
		alarm.alarm_mask	   = RTC_ALARM_DATE_MASK | RTC_ALARM_HOUR_MASK | RTC_ALARM_MINUTE_MASK;
		alarm.alarm_second	   = next_sec;
		alarm.am_pm			   = RTC_AM;
		rtc_alarm_config(RTC_ALARM0, &alarm);
		rtc_alarm_enable(RTC_ALARM0);
		rtc_interrupt_enable(RTC_INT_ALARM0);

		g_sleeping = true;
		pmu_to_deepsleepmode(PMU_LDO_NORMAL, PMU_LOWDRIVER_DISABLE, WFI_CMD);
		g_sleeping = false;
		send_with_485("instrument wakeup\r\n");
	}

	// 0x0400 — 读取阈值参数 (CH0+CH1)
	inline void cmd_0400_get_thresholds(const Frame &f)
	{
		uint16_t devid = frame_devid(f);
		uint8_t buf[8];
		float_to_bytes(Params::g_params.ch0_threshold, buf);
		float_to_bytes(Params::g_params.ch1_threshold, buf + 4);
		send_response(devid, 0x0400, buf, 8);
	}

	// 0x0401/0402/0403 — 读取单通道阈值
	inline void cmd_0401_get_ch0_thr(const Frame &f)
	{
		uint16_t devid = frame_devid(f);
		uint8_t buf[4];
		float_to_bytes(Params::g_params.ch0_threshold, buf);
		send_response(devid, 0x0401, buf, 4);
	}
	inline void cmd_0402_get_ch1_thr(const Frame &f)
	{
		uint16_t devid = frame_devid(f);
		uint8_t buf[4];
		float_to_bytes(Params::g_params.ch1_threshold, buf);
		send_response(devid, 0x0402, buf, 4);
	}
	inline void cmd_0403_get_ch2_thr(const Frame &f)
	{
		uint16_t devid = frame_devid(f);
		uint8_t buf[4];
		float_to_bytes(Params::g_params.ch2_threshold, buf);
		send_response(devid, 0x0403, buf, 4);
	}

	// 0x0411/0412/0413 — 写入单通道阈值
	inline void cmd_0411_set_ch0_thr(const Frame &f)
	{
		uint16_t devid = frame_devid(f);
		if (frame_content_len(f) >= 4)
		{
			Params::g_params.ch0_threshold = bytes_to_float(frame_content(f));
			Params::save();
			send_ok(devid, 0x0411);
		}
		else
			send_error(devid);
	}
	inline void cmd_0412_set_ch1_thr(const Frame &f)
	{
		uint16_t devid = frame_devid(f);
		if (frame_content_len(f) >= 4)
		{
			Params::g_params.ch1_threshold = bytes_to_float(frame_content(f));
			Params::save();
			send_ok(devid, 0x0412);
		}
		else
			send_error(devid);
	}
	inline void cmd_0413_set_ch2_thr(const Frame &f)
	{
		uint16_t devid = frame_devid(f);
		if (frame_content_len(f) >= 4)
		{
			Params::g_params.ch2_threshold = bytes_to_float(frame_content(f));
			Params::save();
			send_ok(devid, 0x0413);
		}
		else
			send_error(devid);
	}

// Bootloader 入口标志 (BKPSRAM 备份域, 复位后保留)
#define BOOTLOADER_FLAG_ADDR ((volatile uint32_t *)0x40024000)
#define BOOTLOADER_MAGIC	 0x424F4F54

	// 0x0501 — 升级请求 → 设标志并软重启
	inline void cmd_0501_upgrade_req(const Frame &f)
	{
		uint16_t devid = frame_devid(f);
		send_ok(devid, 0x0501);
		for (int i = 0; i < 100000; ++i) __asm__ volatile("nop");
		rcu_periph_clock_enable(RCU_BKPSRAM);
		pmu_backup_write_enable();
		*BOOTLOADER_FLAG_ADDR = BOOTLOADER_MAGIC;
		__disable_irq();
		NVIC_SystemReset();
	}

	// 0x0601 — 设置是否主动上报告警
	inline void cmd_0601_set_alarm_mode(const Frame &f)
	{
		uint16_t devid = frame_devid(f);
		if (frame_content_len(f) >= 1)
		{
			uint8_t mode = frame_content(f)[0];
			if (mode == 0x01 || mode == 0x02)
			{
				Params::g_params.alarm_mode = mode;
				Alarm::g_active				= (mode == 0x01);
				Params::save();
				send_ok(devid, 0x0601);
			}
			else
				send_error(devid);
		}
		else
			send_error(devid);
	}

	// 0x0602 — 查询告警记录
	inline void cmd_0602_get_alarms(const Frame &f)
	{
		(void)f;
		if (Alarm::g_record_count == 0)
		{
			send_with_485("empty\r\n");
			return;
		}
		char buf[128];
		for (int i = 0; i < Alarm::g_record_count; ++i)
		{
			if (Alarm::g_records[i].valid)
			{
				Alarm::format_record(Alarm::g_records[i], buf, sizeof(buf));
				send_with_485(buf);
			}
		}
	}

	// 0x0603 — 清除告警
	inline void cmd_0603_clear_alarms(const Frame &f)
	{
		uint16_t devid = frame_devid(f);
		Alarm::clear();
		send_ok(devid, 0x0603);
	}

	// ==================== 帧分发 ====================

	/// 协议帧分发: 返回 true 表示已处理 (含错误帧)
	inline bool dispatch(const Frame &frame)
	{
		uint16_t cmd  = frame_cmd(frame);
		uint8_t ftype = frame_type(frame);

		// 广播地址 0xFFFF 或 本机地址
		uint16_t devid = frame_devid(frame);
		uint16_t my_id = Params::g_params.device_id;
		if (devid != 0xFFFF && devid != my_id)
		{
			return true; // 不是发给我的, 静默丢弃
		}

		// 自动上报期间: 只响应停止命令, 其余静默丢弃
		if (g_auto_report_active && ftype == 0x01)
		{
			if (cmd != 0x0303)
			{
				return true;
			}
		}

		// 错误帧(FF EEEE) 由上层已处理, 此处只处理正常命令
		if (ftype == 0x01 || ftype == 0x05)
		{
			switch (cmd)
			{
				// 系统管理
				case 0x0101:
					cmd_0101_reboot(frame);
					return true;
				case 0x0104:
					cmd_0104_fw_version(frame);
					return true;
				case 0x0105:
					cmd_0105_set_time(frame);
					return true;
				case 0x0106:
					cmd_0106_get_time(frame);
					return true;
				case 0x01A1:
					cmd_01A1_set_id(frame);
					return true;
				case 0x01A2:
					cmd_01A2_set_baudrate(frame);
					return true;
				case 0x0111:
					cmd_0111_get_id(frame);
					return true;
				case 0x0112:
					cmd_0112_get_baudrate(frame);
					return true;
				// 数据类
				case 0x0201:
					cmd_0201_ch0(frame);
					return true;
				case 0x0202:
					cmd_0202_ch1(frame);
					return true;
				case 0x0221:
					cmd_0221_ch2(frame);
					return true;
				case 0x0241:
					cmd_0241_set_ch0_ratio(frame);
					return true;
				case 0x0242:
					cmd_0242_set_ch1_ratio(frame);
					return true;
				case 0x0261:
					cmd_0261_set_interval(frame);
					return true;
				// 控制类
				case 0x0301:
					cmd_0301_set_dac(frame);
					return true;
				case 0x0302:
					cmd_0302_start_auto(frame);
					return true;
				case 0x0303:
					cmd_0303_stop_auto(frame);
					return true;
				case 0x03AA:
					cmd_03AA_sleep(frame);
					return true;
				// 参数类
				case 0x0400:
					cmd_0400_get_thresholds(frame);
					return true;
				case 0x0401:
					cmd_0401_get_ch0_thr(frame);
					return true;
				case 0x0402:
					cmd_0402_get_ch1_thr(frame);
					return true;
				case 0x0403:
					cmd_0403_get_ch2_thr(frame);
					return true;
				case 0x0411:
					cmd_0411_set_ch0_thr(frame);
					return true;
				case 0x0412:
					cmd_0412_set_ch1_thr(frame);
					return true;
				case 0x0413:
					cmd_0413_set_ch2_thr(frame);
					return true;
				// 升级类
				case 0x0501:
					cmd_0501_upgrade_req(frame);
					return true;
				// 告警类
				case 0x0601:
					cmd_0601_set_alarm_mode(frame);
					return true;
				case 0x0602:
					cmd_0602_get_alarms(frame);
					return true;
				case 0x0603:
					cmd_0603_clear_alarms(frame);
					return true;
				// 心跳 / 广播
				case 0x8888:
					return true; // 接收到心跳 (忽略)
				case 0xFFFF:
				{
					// 广播寻址: 回复心跳
					uint8_t buf[64];
					uint16_t sz = Response::build_heartbeat(my_id, buf, sizeof(buf));
					if (sz) send_frame(buf, sz);
					return true;
				}
				default:
				{
					send_error(my_id);
					return true;
				}
			}
		}

		return false;
	}

} // namespace CmdDispatch
