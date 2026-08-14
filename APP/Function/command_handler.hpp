#pragma once
// 命令处理器 — 帧分发 + 全部协议命令响应 (独立于 Device)

#include "hardware.hpp"
#include "params.hpp"
#include "alarm_manager.hpp"
#include "../Protocol/protocol_types.hpp"
#include "../Protocol/response_builder.hpp"
#include "../Driver/serial_send.hpp"
#include "../Driver/flash_param.hpp"
#include "modbus_slave.hpp"
#include <cstdio>
#include <cstring>
#include <ctime>
extern "C" void SystemInit();
// 外部全局外设 (定义在 main.cpp)
extern gd30ad3344_on_spi1 g_adc;

// ======================== OLED 状态枚举 ========================
enum class OLEDStatus : uint8_t {
	BOOTLOADER = 0,
	IDLE = 1,
	AUTO_SAMPLE = 2,
};

// ======================== 共享工具函数 ========================

// — 大端读写 —
inline uint16_t read_u16(const uint8_t *p)
{
	return (static_cast<uint16_t>(p[0]) << 8) | p[1];
}
inline void write_u16(uint8_t *buf, uint16_t val)
{
	buf[0] = static_cast<uint8_t>(val >> 8);
	buf[1] = static_cast<uint8_t>(val & 0xFF);
}

// — IEEE 754 转换 —
inline void float_to_bytes(float val, uint8_t *out)
{
	uint32_t bits;
	std::memcpy(&bits, &val, 4);
	out[0] = bits >> 24;
	out[1] = bits >> 16;
	out[2] = bits >> 8;
	out[3] = bits;
}
inline float bytes_to_float(const uint8_t *b)
{
	uint32_t bits = (static_cast<uint32_t>(b[0]) << 24) |
			(static_cast<uint32_t>(b[1]) << 16) |
			(static_cast<uint32_t>(b[2]) << 8) | b[3];
	float v;
	std::memcpy(&v, &bits, 4);
	return v;
}

// — 帧字段提取 —
inline uint16_t frame_cmd(const ProtocolFrame &f)   { return read_u16(&f.data[5]); }
inline uint8_t  frame_type(const ProtocolFrame &f)  { return f.data[4]; }
inline uint16_t frame_devid(const ProtocolFrame &f) { return read_u16(&f.data[2]); }
inline uint8_t  frame_content_len(const ProtocolFrame &f) { return f.data[7]; }
inline const uint8_t *frame_content(const ProtocolFrame &f) { return &f.data[9]; }

// — ADC 采样 —
inline float read_ch0()
{
	ADC0::set_channel(ADC_CHANNEL_10);
	ADC0::get_value(); // 空读消除通道切换后的采样电容残余电压
	return ADC0::get_value() * 3.3f / 4095.0f;
}
inline float read_ch1()
{
	ADC0::set_channel(ADC_CHANNEL_11);
	ADC0::get_value(); // 空读消除通道切换后的采样电容残余电压
	return ADC0::get_value() * 3.3f / 4095.0f;
}
inline float read_ch2()
{
	int16_t raw = g_adc.read_raw();
	if (raw < 0) raw = 0;
	float v = static_cast<float>(raw) / 32768.0f * 2.048f;
	return v * v * -6.91f + 268.66f * v - 281.28f;
}

// — RTC / UTC 转换 —
inline uint8_t bcd_to_dec(uint8_t v) { return ((v >> 4) * 10) + (v & 0x0F); }
inline uint8_t dec_to_bcd(uint8_t v) { return ((v / 10) << 4) | (v % 10); }

inline uint32_t rtc_to_utc()
{
	auto t = RTC::get_time();
	uint16_t yr = bcd_to_dec(t.year) + 2000;
	uint8_t mo = bcd_to_dec(t.month), da = bcd_to_dec(t.date);
	uint8_t hr = bcd_to_dec(t.hour), mi = bcd_to_dec(t.minute), se = bcd_to_dec(t.second);
	uint32_t days = 0;
	for (uint16_t y = 1970; y < yr; ++y) {
		bool lp = (y % 4 == 0 && (y % 100 != 0 || y % 400 == 0));
		days += lp ? 366 : 365;
	}
	bool lp = (yr % 4 == 0 && (yr % 100 != 0 || yr % 400 == 0));
	const uint8_t dm[] = {31, (uint8_t)(lp ? 29 : 28), 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
	for (int m = 0; m < mo - 1; ++m) days += dm[m];
	days += da - 1;
	return (days * 86400) + (static_cast<uint32_t>(hr) * 3600) + (static_cast<uint32_t>(mi) * 60) + se;
}

inline void set_rtc_time(uint32_t utc)
{
	uint8_t wd = (utc / 86400 + 4) % 7;
	uint32_t tod = utc % 86400;
	uint8_t hr = tod / 3600, mi = (tod % 3600) / 60, se = tod % 60;
	uint32_t days = utc / 86400;
	uint16_t yr = 1970;
	while (true) {
		bool lp = (yr % 4 == 0 && (yr % 100 != 0 || yr % 400 == 0));
		uint16_t diy = lp ? 366 : 365;
		if (days >= diy) { days -= diy; ++yr; } else break;
	}
	bool lp = (yr % 4 == 0 && (yr % 100 != 0 || yr % 400 == 0));
	const uint8_t dm[] = {31, (uint8_t)(lp ? 29 : 28), 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
	uint8_t mo = 0;
	while (days >= dm[mo]) { days -= dm[mo]; ++mo; }
	HAL::gd32f4::RTC_Time rt;
	rt.year = dec_to_bcd(yr % 100);
	rt.month = dec_to_bcd(mo + 1);
	rt.date = dec_to_bcd(days + 1);
	rt.week = dec_to_bcd(wd);
	rt.hour = dec_to_bcd(hr);
	rt.minute = dec_to_bcd(mi);
	rt.second = dec_to_bcd(se);
	RTC::set_time(rt.year, rt.month, rt.date, rt.week, rt.hour, rt.minute, rt.second);
}

// ======================== CommandHandler 类 ========================

class CommandHandler {
    public:
	CommandHandler(DeviceParams &params, AlarmManager &alarms,
		       OLEDStatus &oled_status,
		       bool &auto_report_active, uint32_t &auto_report_next,
		       uint32_t &auto_report_interval_ms,
		       bool &is_sampling, bool &sleeping, uint16_t &dac_raw)
		: params_(params), alarms_(alarms),
		  oled_status_(oled_status),
		  auto_report_active_(auto_report_active),
		  auto_report_next_(auto_report_next),
		  auto_report_interval_ms_(auto_report_interval_ms),
		  is_sampling_(is_sampling), sleeping_(sleeping), dac_raw_(dac_raw)
	{}

	void handle_frame(const ProtocolFrame &frame)
	{
		uint16_t cmd = frame_cmd(frame);
		uint8_t ftype = frame_type(frame);
		uint16_t devid = frame_devid(frame);

		// 非广播且设备 ID 不匹配 — 静默丢弃 (协议规约 4.5.7(2))
		if (devid != 0xFFFF && devid != params_.device_id) return;

		// 自动上报期间: 只放行 0x0303(停止上报) , 其余静默丢弃
		if (auto_report_active_ && ftype == 0x01 && !(cmd == 0x0303)) return;

		// 仅处理命令下发帧(0x01)和心跳/广播帧(0x05)
		if (ftype != 0x01 && ftype != 0x05) return;

		switch (cmd) {
		// ——— 系统管理 ———
		case 0x0101: cmd_reboot(frame);           break;
		case 0x0104: cmd_fw_version(frame);       break;
		case 0x0105: cmd_set_time(frame);         break;
		case 0x0106: cmd_get_time(frame);         break;
		case 0x01A1: cmd_set_id(frame);           break;
		case 0x01A2: cmd_set_baudrate(frame);     break;
		case 0x0111: cmd_get_id(frame);           break;
		case 0x0112: cmd_get_baudrate(frame);     break;
		// ——— 数据类 ———
		case 0x0201: cmd_ch0(frame);              break;
		case 0x0202: cmd_ch1(frame);              break;
		case 0x0221: cmd_ch2(frame);              break;
		case 0x0241: cmd_set_ch0_ratio(frame);    break;
		case 0x0242: cmd_set_ch1_ratio(frame);    break;
		case 0x0261: cmd_set_interval(frame);     break;
		// ——— 控制类 ———
		case 0x0301: cmd_set_dac(frame);          break;
		case 0x0302: cmd_start_auto(frame);       break;
		case 0x0303: cmd_stop_auto(frame);        break;
		case 0x03AA: cmd_sleep(frame);            break;
		// ——— 参数配置 ———
		case 0x0400: cmd_get_thresholds(frame);   break;
		case 0x0401: cmd_get_ch0_thr(frame);      break;
		case 0x0402: cmd_get_ch1_thr(frame);      break;
		case 0x0403: cmd_get_ch2_thr(frame);      break;
		case 0x0411: cmd_set_ch0_thr(frame);      break;
		case 0x0412: cmd_set_ch1_thr(frame);      break;
		case 0x0413: cmd_set_ch2_thr(frame);      break;
		// ——— 系统升级 ———
		case 0x0501: cmd_upgrade_req(frame);      break;
		// ——— 告警与日志 ———
		case 0x0601: cmd_set_alarm_mode(frame);   break;
		case 0x0602: cmd_get_alarms(frame);       break;
		case 0x0603: cmd_clear_alarms(frame);     break;
		// ——— 特殊帧 ———
		case 0x8888: break;  // 心跳帧, 忽略
		case 0xFFFF: {       // 广播寻址 -> 回复心跳
			uint8_t buf[64];
			uint16_t sz = ResponseBuilder::build_heartbeat(params_.device_id, buf, sizeof(buf));
			if (sz) send_frame(buf, sz);
			break;
		}
		default:
			// 协议 4.5.7(2): 未定义命令字 → 回复错误应答帧
			if (devid != 0xFFFF)
				send_error(devid);
			break;
		}
	}

    private:
	// ——— 状态引用 ———
	DeviceParams &params_;
	AlarmManager &alarms_;
	OLEDStatus &oled_status_;
	bool &auto_report_active_;
	uint32_t &auto_report_next_;
	uint32_t &auto_report_interval_ms_;
	bool &is_sampling_;
	bool &sleeping_;
	uint16_t &dac_raw_;

	// ——— 告警分批发送状态机 ———
	bool alarm_send_active_ = false;
	int alarm_send_index_ = 0;
	uint32_t alarm_send_next_ms_ = 0;

	// ——— 睡眠状态机 ———
	enum class SleepPhase : uint8_t { IDLE, WAIT_TX };
	SleepPhase sleep_phase_ = SleepPhase::IDLE;

	// ——— 参数持久化 ———
	void params_save()
	{
		params_.crc32 = params_crc32_calc(reinterpret_cast<const uint8_t *>(&params_), sizeof(DeviceParams) - 4);
		FlashParam::save(params_);
		alarms_.save_to_flash();
	}

	// ——— 帧发送 ———
	void send_frame(const uint8_t *binary, uint16_t size) { send_with_485(binary, size); }
	void send_ok(uint16_t devid, uint16_t cmd) {
		uint8_t buf[64];
		uint16_t sz = ResponseBuilder::build_ok(devid, cmd, buf, sizeof(buf));
		if (sz) send_frame(buf, sz);
	}
	void send_error(uint16_t devid) {
		uint8_t buf[64];
		uint16_t sz = ResponseBuilder::build_error(devid, buf, sizeof(buf));
		if (sz) send_frame(buf, sz);
	}
	void send_response(uint16_t devid, uint16_t cmd, const uint8_t *content, uint8_t len) {
		uint8_t buf[256];
		uint16_t sz = ResponseBuilder::build_response(devid, cmd, content, len, buf, sizeof(buf));
		if (sz) send_frame(buf, sz);
	}

	// ——— 命令处理器 ———

	void cmd_reboot(const ProtocolFrame &f) { send_ok(frame_devid(f), 0x0101); __disable_irq(); NVIC_SystemReset(); }

	void cmd_fw_version(const ProtocolFrame &f) {
		uint8_t ver[4] = {0x02, 0x00, 0x01, 0x00};
		send_response(frame_devid(f), 0x0104, ver, 4);
	}

	void cmd_set_time(const ProtocolFrame &f) {
		if (frame_content_len(f) >= 4) {
			auto c = frame_content(f);
			set_rtc_time((static_cast<uint32_t>(c[0]) << 24) | (static_cast<uint32_t>(c[1]) << 16) |
				     (static_cast<uint32_t>(c[2]) << 8) | c[3]);
			send_ok(frame_devid(f), 0x0105);
		} else send_error(frame_devid(f));
	}

	void cmd_get_time(const ProtocolFrame &f) {
		uint32_t utc = rtc_to_utc();
		uint8_t buf[4]; buf[0] = utc >> 24; buf[1] = utc >> 16; buf[2] = utc >> 8; buf[3] = utc;
		send_response(frame_devid(f), 0x0106, buf, 4);
	}

	void cmd_set_id(const ProtocolFrame &f) {
		if (frame_content_len(f) >= 2) {
			uint16_t nid = read_u16(frame_content(f));
			if (nid >= 0x0001 && nid <= 0xFFFE) {
				params_.device_id = nid; params_save();
				send_ok(nid, 0x01A1); return;
			}
		}
		send_error(frame_devid(f));
	}

	void cmd_set_baudrate(const ProtocolFrame &f) {
		if (frame_content_len(f) >= 1) {
			uint8_t code = frame_content(f)[0];
			if (code >= 0x11 && code <= 0x14) {
				params_.baudrate_code = code; params_save();
				send_ok(frame_devid(f), 0x01A2);
				for (int i = 0; i < 100000; ++i) __asm__ volatile("nop");
				set_485_baudrate(baudrate_code_to_hz(code));
				return;
			}
		}
		send_error(frame_devid(f));
	}

	void cmd_get_id(const ProtocolFrame &f) {
		(void)f;
		uint8_t buf[2]; write_u16(buf, params_.device_id);
		send_response(params_.device_id, 0x0111, buf, 2);
	}

	void cmd_get_baudrate(const ProtocolFrame &f) {
		send_response(frame_devid(f), 0x0112, &params_.baudrate_code, 1);
	}

	void cmd_ch0(const ProtocolFrame &f) {
		float v = read_ch0() * params_.ch0_ratio;
		uint8_t buf[4]; float_to_bytes(v, buf);
		send_response(frame_devid(f), 0x0201, buf, 4);
	}

	void cmd_ch1(const ProtocolFrame &f) {
		float v = read_ch1() * params_.ch1_ratio;
		uint8_t buf[4]; float_to_bytes(v, buf);
		send_response(frame_devid(f), 0x0202, buf, 4);
	}

	void cmd_ch2(const ProtocolFrame &f) {
		float v = read_ch2();
		uint8_t buf[4]; float_to_bytes(v, buf);
		send_response(frame_devid(f), 0x0221, buf, 4);
	}

	void cmd_set_ch0_ratio(const ProtocolFrame &f) {
		if (frame_content_len(f) >= 4) {
			params_.ch0_ratio = bytes_to_float(frame_content(f)); params_save();
			send_ok(frame_devid(f), 0x0241);
		} else send_error(frame_devid(f));
	}

	void cmd_set_ch1_ratio(const ProtocolFrame &f) {
		if (frame_content_len(f) >= 4) {
			params_.ch1_ratio = bytes_to_float(frame_content(f)); params_save();
			send_ok(frame_devid(f), 0x0242);
		} else send_error(frame_devid(f));
	}

	void cmd_set_interval(const ProtocolFrame &f) {
		if (frame_content_len(f) >= 1) {
			uint8_t code = frame_content(f)[0];
			if (code >= 1 && code <= 3) {
				params_.report_interval = code; params_save();
				auto_report_interval_ms_ = (code == 1) ? 1000U : (code == 2) ? 3000U : 5000U;
				send_ok(frame_devid(f), 0x0261);
			} else send_error(frame_devid(f));
		} else send_error(frame_devid(f));
	}

	void cmd_set_dac(const ProtocolFrame &f) {
		if (frame_content_len(f) >= 2) {
			uint16_t val = read_u16(frame_content(f));
			if (val <= 4095) {
				dac_raw_ = val;
				DAC0::set(val);
				DAC0::trigger();
				send_ok(frame_devid(f), 0x0301);
			} else
				send_error(frame_devid(f));
		} else send_error(frame_devid(f));
	}

	void cmd_start_auto(const ProtocolFrame &f) {
		auto_report_active_ = true; auto_report_next_ = 0;
		oled_status_ = OLEDStatus::AUTO_SAMPLE; is_sampling_ = true;
		work_status_led::set();
		uint32_t utc = rtc_to_utc();
		float ch0 = read_ch0() * params_.ch0_ratio, ch1 = read_ch1() * params_.ch1_ratio;
		uint8_t buf[12];
		buf[0] = utc >> 24; buf[1] = utc >> 16; buf[2] = utc >> 8; buf[3] = utc;
		float_to_bytes(ch0, buf + 4); float_to_bytes(ch1, buf + 8);
		send_response(frame_devid(f), 0x0302, buf, 12);
	}

	void cmd_stop_auto(const ProtocolFrame &f) {
		auto_report_active_ = false; oled_status_ = OLEDStatus::IDLE; is_sampling_ = false;
		work_status_led::clear();
		send_ok(frame_devid(f), 0x0303);
	}

	void cmd_sleep(const ProtocolFrame &f) {
		// 阶段1: 回复 OK 后立即返回, 睡眠入口交给调度器的 sleep_tick 延迟执行
		send_ok(frame_devid(f), 0x03AA);
		sleep_phase_ = SleepPhase::WAIT_TX;
	}

	/// 睡眠入口驱动 (由调度器周期性调用, 传入当前 systick)
	public:
	void sleep_tick(uint32_t systick_ms)
	{
		if (sleep_phase_ != SleepPhase::WAIT_TX) return;
		sleep_phase_ = SleepPhase::IDLE;

		// ——— 阶段2: 进入深度睡眠 (OK 帧已由 50ms 调度延迟充分发完) ———

		// 1. 清除残留标志
		rtc_alarm_disable(RTC_ALARM0);
		rtc_flag_clear(RTC_FLAG_ALRM0);
		exti_flag_clear(EXTI_17);

		// 2. 配置 EXTI line 17
		exti_deinit();
		exti_init(EXTI_17, EXTI_INTERRUPT, EXTI_TRIG_RISING);
		rtc_flag_clear(RTC_FLAG_ALRM0);
		exti_interrupt_flag_clear(EXTI_17);
		exti_interrupt_enable(EXTI_17);

		// 3. 使能 NVIC RTC 闹钟中断
		nvic_irq_enable(RTC_Alarm_IRQn, 2U, 1U);

		// 4. 配置 RTC 闹钟
		rtc_alarm_struct alarm = {};
		auto t = RTC::get_time();
		alarm.alarm_mask = RTC_ALARM_DATE_MASK | RTC_ALARM_HOUR_MASK | RTC_ALARM_MINUTE_MASK;
		alarm.weekday_or_date = RTC_ALARM_DATE_SELECTED;
		alarm.alarm_day = 0x31;
		alarm.alarm_hour = 0x00;
		alarm.alarm_minute = 0x00;
		alarm.alarm_second = dec_to_bcd((bcd_to_dec(t.second) + 10) % 60);
		alarm.am_pm = RTC_AM;
		rtc_alarm_config(RTC_ALARM0, &alarm);
		rtc_interrupt_enable(RTC_INT_ALARM0);
		rtc_alarm_enable(RTC_ALARM0);

		sleeping_ = true;
		modbus_slave_suspend();

		// 5. HCLK 降频序列
		{
			auto soft_delay = [](uint32_t time) {
				__IO uint32_t ii;
				for (ii = 0; ii < time * 10; ii = ii + 1) {}
			};
			rcu_ahb_clock_config(RCU_AHB_CKSYS_DIV2);
			soft_delay(0x50);
			rcu_ahb_clock_config(RCU_AHB_CKSYS_DIV4);
			soft_delay(0x50);
			rcu_ahb_clock_config(RCU_AHB_CKSYS_DIV8);
			soft_delay(0x50);
			rcu_ahb_clock_config(RCU_AHB_CKSYS_DIV16);
			soft_delay(0x50);
			rcu_system_clock_source_config(RCU_CKSYSSRC_IRC16M);
			soft_delay(200);
			rcu_ahb_clock_config(RCU_AHB_CKSYS_DIV1);
		}

		pmu_to_deepsleepmode(PMU_LDO_LOWPOWER, PMU_LOWDRIVER_ENABLE,
				     WFI_CMD);
		sleeping_ = false;

		// 6. 唤醒后恢复系统时钟
		SystemInit();
		usart_baudrate_set(HAL::gd32f4::registers::USART1_ADDR,
				   baudrate_code_to_hz(params_.baudrate_code));
		usart_enable(HAL::gd32f4::registers::USART1_ADDR);
		if (!modbus_slave_resume())
			SEGGER_RTT_WriteString(0, "FreeModbus resume failed!\r\n");

		// 唤醒后 UART 电平稳定延时 (深度睡眠后调度器未恢复, 用 nop)
		for (int i = 0; i < 1000000; ++i) __asm__ volatile("nop");

		send_with_485("instrument wakeup");
	}

	void cmd_get_thresholds(const ProtocolFrame &f) {
		uint8_t buf[8];
		float_to_bytes(params_.ch0_threshold, buf);
		float_to_bytes(params_.ch1_threshold, buf + 4);
		send_response(frame_devid(f), 0x0400, buf, 8);
	}
	void cmd_get_ch0_thr(const ProtocolFrame &f) { uint8_t b[4]; float_to_bytes(params_.ch0_threshold, b); send_response(frame_devid(f), 0x0401, b, 4); }
	void cmd_get_ch1_thr(const ProtocolFrame &f) { uint8_t b[4]; float_to_bytes(params_.ch1_threshold, b); send_response(frame_devid(f), 0x0402, b, 4); }
	void cmd_get_ch2_thr(const ProtocolFrame &f) { uint8_t b[4]; float_to_bytes(params_.ch2_threshold, b); send_response(frame_devid(f), 0x0403, b, 4); }

	void cmd_set_ch0_thr(const ProtocolFrame &f) {
		if (frame_content_len(f) >= 4) { params_.ch0_threshold = bytes_to_float(frame_content(f)); params_save(); send_ok(frame_devid(f), 0x0411); }
		else send_error(frame_devid(f));
	}
	void cmd_set_ch1_thr(const ProtocolFrame &f) {
		if (frame_content_len(f) >= 4) { params_.ch1_threshold = bytes_to_float(frame_content(f)); params_save(); send_ok(frame_devid(f), 0x0412); }
		else send_error(frame_devid(f));
	}
	void cmd_set_ch2_thr(const ProtocolFrame &f) {
		if (frame_content_len(f) >= 4) { params_.ch2_threshold = bytes_to_float(frame_content(f)); params_save(); send_ok(frame_devid(f), 0x0413); }
		else send_error(frame_devid(f));
	}

	void cmd_upgrade_req(const ProtocolFrame &f) {
		send_ok(frame_devid(f), 0x0501);
		for (int i = 0; i < 100000; ++i) __asm__ volatile("nop");
		rcu_periph_clock_enable(RCU_BKPSRAM);
		pmu_backup_write_enable();
		*(volatile uint32_t *)0x40024000 = 0x424F4F54;
		__disable_irq();
		NVIC_SystemReset();
	}

	void cmd_set_alarm_mode(const ProtocolFrame &f) {
		if (frame_content_len(f) >= 1) {
			uint8_t mode = frame_content(f)[0];
			if (mode == 0x01 || mode == 0x02) {
				params_.alarm_mode = mode; alarms_.active_ = (mode == 0x01); params_save();
				send_ok(frame_devid(f), 0x0601);
			} else send_error(frame_devid(f));
		} else send_error(frame_devid(f));
	}

	void cmd_get_alarms(const ProtocolFrame &f) {
		(void)f;
		if (alarms_.record_count_ == 0) {
			// 微秒级缓冲: 防止自动化脚本连续收发导致 RS485 总线冲突
			for (int i = 0; i < 50000; ++i) __asm__ volatile("nop");
			send_with_485("empty \r\n");
			return;
		}
		// 启动分批发送状态机: 每条记录独立发送, 间隔由 alarm_send_tick 控制
		alarm_send_index_ = 0;
		alarm_send_active_ = true;
		alarm_send_next_ms_ = 0;
		// 不在这里发送, 由下一次 alarm_send_tick 驱动第一条
	}

	/// 告警分批发送驱动 (由调度器周期性调用, 传入当前 systick)
	public:
	void alarm_send_tick(uint32_t systick_ms)
	{
		if (!alarm_send_active_) return;
		if (systick_ms < alarm_send_next_ms_) return;

		// 跳过无效记录
		while (alarm_send_index_ < alarms_.record_count_ &&
		       !alarms_.records_[alarm_send_index_].valid) {
			++alarm_send_index_;
		}

		if (alarm_send_index_ >= alarms_.record_count_) {
			alarm_send_active_ = false;
			return;
		}

		char buf[80];
		AlarmManager::format_record(
			alarms_.records_[alarm_send_index_], buf, sizeof(buf));
		send_with_485(buf);

		++alarm_send_index_;
		// 下一条间隔 50ms, 给接收端留出处理时间
		alarm_send_next_ms_ = systick_ms + 50;
	}

	void cmd_clear_alarms(const ProtocolFrame &f) {
		alarms_.clear();
		alarms_.save_to_flash();
		send_ok(frame_devid(f), 0x0603);
	}
};
