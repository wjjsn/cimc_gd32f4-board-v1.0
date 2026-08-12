#pragma once
// 设备核心对象 — 状态容器, 调度器任务入口, 参数持久化

#include "OLED/ssd1306/0.91.hpp"
#include "hardware.hpp"
#include "params.hpp"
#include "alarm_manager.hpp"
#include "command_handler.hpp"
#include "../Protocol/protocol_parser.hpp"
#include "../Driver/serial_send.hpp"
#include "SEGGER_RTT.h"
#include "modbus_app.hpp"
#include <cmath>

extern Screen g_screen;

constexpr uint32_t DEFAULT_BAUDRATE = 19200;

template <typename RingBuffer> class Device {
    public:
	DeviceParams params_{};
	AlarmManager alarms_;
	ProtocolParser<RingBuffer> parser_;

	OLEDStatus oled_status_ = OLEDStatus::IDLE;
	bool ch0_alarm_active_ = false;
	bool ch1_alarm_active_ = false;
	bool auto_report_active_ = false;
	uint32_t auto_report_next_ = 0;
	uint32_t auto_report_interval_ms_ = 1000;
	bool is_sampling_ = false;
	bool sleeping_ = false;
	bool heartbeat_sent_ = false;
	uint16_t dac_raw_ = 0;
	float latest_ch0_ = 0.0f;
	float latest_ch1_ = 0.0f;
	float latest_ch2_ = 0.0f;

	// 命令处理器 — init() 中 placement new 构造, 无动态内存
	CommandHandler *cmd_handler_ = nullptr;
	alignas(CommandHandler) char cmd_handler_storage_[sizeof(CommandHandler)];

	void init()
	{
		load_params();
		parser_.init();
		alarms_.init();
		alarms_.load_from_flash();
		alarms_.active_ = (params_.alarm_mode == 0x01);

		auto_report_interval_ms_ =
			(params_.report_interval == 1) ? 1000U :
			(params_.report_interval == 2) ? 3000U : 5000U;

		oled_status_ = OLEDStatus::IDLE;
		oled_update();

		cmd_handler_ = new (cmd_handler_storage_) CommandHandler(
			params_, alarms_, oled_status_,
			auto_report_active_, auto_report_next_,
			auto_report_interval_ms_, is_sampling_, sleeping_, dac_raw_);

		// if ( //Power reset generated
		// 	RESET != rcu_flag_get(RCU_FLAG_PORRST) ||
		// 	//External PIN reset generated
		// 	(RESET != rcu_flag_get(RCU_FLAG_EPRST) &&
		// 	 RESET == rcu_flag_get(RCU_FLAG_PORRST))) {
		// 	usart_baudrate_set(HAL::gd32f4::registers::USART1_ADDR,
		// 			   DEFAULT_BAUDRATE);
		// 	usart_enable(HAL::gd32f4::registers::USART1_ADDR);
		// }
		// //Software reset generated
		// else if (RESET != rcu_flag_get(RCU_FLAG_SWRST)) {
			usart_baudrate_set(
				HAL::gd32f4::registers::USART1_ADDR,
				baudrate_code_to_hz(params_.baudrate_code));
			usart_enable(HAL::gd32f4::registers::USART1_ADDR);
		// }
		// rcu_all_reset_flag_clear();

		SEGGER_RTT_WriteString(0, "=== CIMC APP v2.0.1.0 ===\r\n");
	}

	void poll_frame()
	{
		ProtocolFrame frame;
		ProtocolStatus s = parser_.poll(frame);

		switch (s) {
		case ProtocolStatus::frame_ready:
			cmd_handler_->handle_frame(frame);
			break;
		case ProtocolStatus::crc_error:
		case ProtocolStatus::length_error:
		case ProtocolStatus::invalid_hex:
			if (frame.size >= 4) {
				uint16_t devid = read_u16(&frame.data[2]);
				if (devid != params_.device_id)
					break;
				send_error_frame(params_.device_id);
			}
			break;
		default:
			break;
		}
	}

	void alarm_scan()
	{
		latest_ch0_ = read_ch0() * params_.ch0_ratio;
		latest_ch1_ = read_ch1() * params_.ch1_ratio;
		float ch0 = latest_ch0_;
		float ch1 = latest_ch1_;
		float thr0 = params_.ch0_threshold;
		float thr1 = params_.ch1_threshold;
		uint32_t utc = rtc_to_utc();

		bool ch0_over = (ch0 > thr0);
		if (ch0_over && !ch0_alarm_active_) {
			alarms_.add(utc, 0, thr0, ch0);
			alarms_.save_to_flash();
			if (alarms_.active_) {
				char buf[128];
				int last = alarms_.record_count_ - 1;
				if (last >= 0) {
					AlarmManager::format_record(alarms_.records_[last], buf, sizeof(buf));
					send_with_485(buf);
				}
			}
			ch0_alarm_active_ = true;
		} else if (!ch0_over && ch0_alarm_active_) {
			ch0_alarm_active_ = false;
		}

		bool ch1_over = (ch1 > thr1);
		if (ch1_over && !ch1_alarm_active_) {
			alarms_.add(utc, 1, thr1, ch1);
			alarms_.save_to_flash();
			if (alarms_.active_) {
				char buf[128];
				int last = alarms_.record_count_ - 1;
				if (last >= 0) {
					AlarmManager::format_record(alarms_.records_[last], buf, sizeof(buf));
					send_with_485(buf);
				}
			}
			ch1_alarm_active_ = true;
		} else if (!ch1_over && ch1_alarm_active_) {
			ch1_alarm_active_ = false;
		}
	}

	void auto_report_tick(uint32_t systick_ms)
	{
		if (!auto_report_active_) {
			work_status_led::clear();
			return;
		}
		work_status_led::set();

		if (systick_ms >= auto_report_next_) {
			auto_report_next_ = systick_ms + auto_report_interval_ms_;

			uint32_t utc = rtc_to_utc();
			latest_ch0_ = read_ch0() * params_.ch0_ratio;
			latest_ch1_ = read_ch1() * params_.ch1_ratio;
			float ch0 = latest_ch0_;
			float ch1 = latest_ch1_;
			uint8_t buf[12];
			buf[0] = (utc >> 24) & 0xFF;
			buf[1] = (utc >> 16) & 0xFF;
			buf[2] = (utc >> 8) & 0xFF;
			buf[3] = utc & 0xFF;
			float_to_bytes(ch0, buf + 4);
			float_to_bytes(ch1, buf + 8);
			send_with_485_response(params_.device_id, 0x0302, buf, 12);
		}
	}

	void oled_update()
	{
		static OLEDStatus last = static_cast<OLEDStatus>(255);
		if (oled_status_ == last) return;
		last = oled_status_;

		g_screen.chear();
		g_screen.printf(0, 0, "%s", "2026523446");
		const char *line2;
		switch (oled_status_) {
		case OLEDStatus::BOOTLOADER: line2 = "Bootloader";  break;
		case OLEDStatus::IDLE:       line2 = "IDLE";        break;
		case OLEDStatus::AUTO_SAMPLE:line2 = "AutoSample";  break;
		default:                     line2 = "IDLE";        break;
		}
		g_screen.printf(2, 0, "%s", line2);
		g_screen.update_force();
	}

	void try_heartbeat(uint32_t systick_ms)
	{
		if (!heartbeat_sent_ && systick_ms > 100) {
			heartbeat_sent_ = true;
			uint8_t buf[64];
			uint16_t sz = ResponseBuilder::build_heartbeat(params_.device_id, buf, sizeof(buf));
			if (sz) send_with_485(buf, sz);
		}
	}

	void alarm_send_tick(uint32_t systick_ms)
	{
		if (cmd_handler_) cmd_handler_->alarm_send_tick(systick_ms);
	}

	void sleep_tick(uint32_t systick_ms)
	{
		if (cmd_handler_) cmd_handler_->sleep_tick(systick_ms);
	}

	void modbus_get_snapshot(ModbusAppSnapshot &snapshot)
	{
		/*
		 * Modbus回调只读这份快照，不直接碰慢速I2C ADC。这样主站读寄存器时不会因为
		 * 某个外设等待而卡住整个协议栈。新增只读业务字段时在这里填当前缓存值。
		 */
		snapshot.ch0 = latest_ch0_;
		snapshot.ch1 = latest_ch1_;
		snapshot.ch2 = latest_ch2_;
		snapshot.utc = rtc_to_utc();
		snapshot.device_id = params_.device_id;
		snapshot.legacy_baudrate_code = params_.baudrate_code;
		snapshot.ch0_ratio = params_.ch0_ratio;
		snapshot.ch1_ratio = params_.ch1_ratio;
		snapshot.ch0_threshold = params_.ch0_threshold;
		snapshot.ch1_threshold = params_.ch1_threshold;
		snapshot.ch2_threshold = params_.ch2_threshold;
		snapshot.report_interval = params_.report_interval;
		snapshot.alarm_mode = params_.alarm_mode;
		snapshot.dac_raw = dac_raw_;
		snapshot.alarm_count = static_cast<uint16_t>(alarms_.record_count_);
		snapshot.ch0_alarm = ch0_alarm_active_;
		snapshot.ch1_alarm = ch1_alarm_active_;
		snapshot.auto_report = auto_report_active_;
		snapshot.sleeping = sleeping_;
		snapshot.work_led = work_status_led::read();
	}

	bool modbus_apply_snapshot(const ModbusAppSnapshot &snapshot,
				   uint32_t changes)
	{
		/*
		 * 先把整次请求全部校验完，再真正修改设备，避免FC16前半段已经生效、后半段却
		 * 因非法值失败。返回false会让Modbus主站收到非法数据值异常（通常是异常码03）。
		 */
		if ((changes & MODBUS_CHANGE_DEVICE_ID) &&
		    (snapshot.device_id == 0U || snapshot.device_id == 0xFFFFU))
			return false;
		// 这是旧协议USART1的波特率代码，不是FreeModbus USART0的19200配置。
		if ((changes & MODBUS_CHANGE_LEGACY_BAUDRATE) &&
		    (snapshot.legacy_baudrate_code < 0x11U ||
		     snapshot.legacy_baudrate_code > 0x14U))
			return false;
		// NaN/Inf写进参数后会让比较和告警逻辑失去意义，因此统一拒绝。
		if ((changes & (MODBUS_CHANGE_CH0_RATIO | MODBUS_CHANGE_CH1_RATIO |
				MODBUS_CHANGE_CH0_THRESHOLD | MODBUS_CHANGE_CH1_THRESHOLD |
				MODBUS_CHANGE_CH2_THRESHOLD)) &&
		    (!std::isfinite(snapshot.ch0_ratio) ||
		     !std::isfinite(snapshot.ch1_ratio) ||
		     !std::isfinite(snapshot.ch0_threshold) ||
		     !std::isfinite(snapshot.ch1_threshold) ||
		     !std::isfinite(snapshot.ch2_threshold)))
			return false;
		if ((changes & MODBUS_CHANGE_REPORT_INTERVAL) &&
		    (snapshot.report_interval < 1U || snapshot.report_interval > 3U))
			return false;
		if ((changes & MODBUS_CHANGE_ALARM_MODE) &&
		    snapshot.alarm_mode != 1U && snapshot.alarm_mode != 2U)
			return false;
		if ((changes & MODBUS_CHANGE_DAC) && snapshot.dac_raw > 4095U)
			return false;

		if (changes & MODBUS_CHANGE_DEVICE_ID)
			params_.device_id = snapshot.device_id;
		if (changes & MODBUS_CHANGE_LEGACY_BAUDRATE)
			params_.baudrate_code = snapshot.legacy_baudrate_code;
		if (changes & MODBUS_CHANGE_CH0_RATIO)
			params_.ch0_ratio = snapshot.ch0_ratio;
		if (changes & MODBUS_CHANGE_CH1_RATIO)
			params_.ch1_ratio = snapshot.ch1_ratio;
		if (changes & MODBUS_CHANGE_CH0_THRESHOLD)
			params_.ch0_threshold = snapshot.ch0_threshold;
		if (changes & MODBUS_CHANGE_CH1_THRESHOLD)
			params_.ch1_threshold = snapshot.ch1_threshold;
		if (changes & MODBUS_CHANGE_CH2_THRESHOLD)
			params_.ch2_threshold = snapshot.ch2_threshold;
		if (changes & MODBUS_CHANGE_REPORT_INTERVAL) {
			params_.report_interval = snapshot.report_interval;
			auto_report_interval_ms_ =
				snapshot.report_interval == 1U ? 1000U :
				snapshot.report_interval == 2U ? 3000U : 5000U;
		}
		if (changes & MODBUS_CHANGE_ALARM_MODE) {
			params_.alarm_mode = snapshot.alarm_mode;
			alarms_.active_ = snapshot.alarm_mode == 1U;
		}
		if (changes & MODBUS_CHANGE_DAC) {
			// DAC写入立即作用。当前dac_raw_不在DeviceParams中，因此这里不单独持久化。
			dac_raw_ = snapshot.dac_raw;
			DAC0::set(dac_raw_);
			DAC0::trigger();
		}

		// 除DAC外，现有保持寄存器参数都属于DeviceParams，统一保存到外部Flash。
		if (changes & ~MODBUS_CHANGE_DAC)
			params_save();
		if (changes & MODBUS_CHANGE_LEGACY_BAUDRATE) {
			usart_baudrate_set(HAL::gd32f4::registers::USART1_ADDR,
					   baudrate_code_to_hz(params_.baudrate_code));
			usart_enable(HAL::gd32f4::registers::USART1_ADDR);
		}
		return true;
	}

	void modbus_set_auto_report(bool enabled)
	{
		// 00001是运行控制，不写入Flash；重启后按默认运行状态重新开始。
		auto_report_active_ = enabled;
		is_sampling_ = enabled;
		oled_status_ = enabled ? OLEDStatus::AUTO_SAMPLE : OLEDStatus::IDLE;
		if (enabled) {
			auto_report_next_ = 0;
			work_status_led::set();
		} else {
			work_status_led::clear();
		}
	}

	void modbus_set_alarm_report(bool enabled)
	{
		// 00002同时改变持久化alarm_mode，所以这里立即保存参数。
		params_.alarm_mode = enabled ? 1U : 2U;
		alarms_.active_ = enabled;
		params_save();
	}

	void modbus_set_work_led(bool enabled)
	{
		// 00003只控制当前GPIO状态，不保存。
		if (enabled)
			work_status_led::set();
		else
			work_status_led::clear();
	}

    private:
	void params_set_defaults()
	{
		params_.magic = PARAM_MAGIC;
		params_.device_id = 0x0001;
		params_.baudrate_code = 0x13;
		params_.reserved0 = 0;
		params_.ch0_ratio = 1.0f; params_.ch1_ratio = 1.0f;
		params_.ch0_threshold = 100.0f; params_.ch1_threshold = 100.0f; params_.ch2_threshold = 100.0f;
		params_.alarm_mode = 0x02; params_.report_interval = 0x01;
		params_.reserved1[0] = 0; params_.reserved1[1] = 0;
		params_.crc32 = 0;
	}
	void params_save()
	{
		params_.crc32 = params_crc32_calc(reinterpret_cast<const uint8_t*>(&params_), sizeof(DeviceParams) - 4);
		FlashParam::save(params_);
		// 恢复告警数据 (params_save 擦除了整个扇区)
		alarms_.save_to_flash();
	}
	void load_params()
	{
		FlashParam::load(params_);
		if (params_.magic != PARAM_MAGIC) { params_set_defaults(); params_save(); return; }
		uint32_t calc = params_crc32_calc(reinterpret_cast<const uint8_t*>(&params_), sizeof(DeviceParams) - 4);
		if (calc != params_.crc32) { params_set_defaults(); params_save(); }
	}

	void send_error_frame(uint16_t devid)
	{
		uint8_t buf[64];
		uint16_t sz = ResponseBuilder::build_error(devid, buf, sizeof(buf));
		if (sz) send_with_485(buf, sz);
	}

	void send_with_485_response(uint16_t devid, uint16_t cmd, const uint8_t* content, uint8_t len)
	{
		uint8_t buf[256];
		uint16_t sz = ResponseBuilder::build_response(devid, cmd, content, len, buf, sizeof(buf));
		if (sz) send_with_485(buf, sz);
	}
};
