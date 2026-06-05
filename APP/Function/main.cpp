#include "hardware.hpp"

#include "chry_ringbuffer.hpp"
#include "SEGGER_RTT.h"
#include "schedule.hpp"

// 协议层
#include "../Protocol/protocol.hpp"

// Driver 层
#include "../Driver/serial_send.hpp"
#include "../Driver/device_init.hpp"

// Function 层
#include "params.hpp"
#include "alarm.hpp"
#include "device_state.hpp"
#include "cmd_handlers.hpp"

// ===================== 环形缓冲区 =====================
chry_ringbuffer_t ctx_uart1_buffer;
using uart1_buffer = Cherry_RingBuffer<&ctx_uart1_buffer, 128>;
template <>
std::array<uint8_t, 128> Cherry_RingBuffer<&ctx_uart1_buffer, 128>::pool_{};

// ===================== 协议解析器 =====================
Protocol::Parser<uart1_buffer> g_proto;

// ===================== 全局外设实例 =====================
ADC g_adc;
Screen g_screen;

// ===================== SysTick =====================
static volatile uint32_t systick_tick_ms = 0;

uint64_t systick_get_ms()
{
	return systick_tick_ms;
}

// ===================== 心跳已发送标记 =====================
static bool g_heartbeat_sent = false;

// ===================== 调度器 =====================
using Scheduler = StaticTimerManager<
	systick_get_ms,

	// 5ms: 协议轮询与命令分发
	TaskConfig{5, []
			   {
				   Protocol::Frame frame;
				   Protocol::Status s = g_proto.poll(frame);

				   switch (s)
				   {
					   case Protocol::Status::frame_ready:
					   {
						   CmdDispatch::dispatch(frame);
						   break;
					   }
					   case Protocol::Status::crc_error:
					   case Protocol::Status::length_error:
					   case Protocol::Status::invalid_hex:
					   {
						   if (frame.size >= 4)
						   {
							   uint16_t devid = DeviceState::read_u16(&frame.data[2]);
							   if (devid != 0xFFFF && devid != Params::g_params.device_id)
								   break;
							   CmdDispatch::send_error(Params::g_params.device_id);
						   }
						   break;
					   }
					   default:
						   break;
				   }
			   }},

	// 10ms: 告警扫描 (始终运行, 不依赖自动上报)
	TaskConfig{10, []
			   {
				   float ch0	= CmdDispatch::read_ch0() * Params::g_params.ch0_ratio;
				   float ch1	= CmdDispatch::read_ch1() * Params::g_params.ch1_ratio;
				   float thr0	= Params::g_params.ch0_threshold;
				   float thr1	= Params::g_params.ch1_threshold;
				   uint32_t utc = CmdDispatch::rtc_to_utc();

				   if (!(ch0 <= thr0))
				   {
					   Alarm::add(utc, 0, thr0, ch0);
					   Alarm::save_to_flash();
					   if (Alarm::g_active)
					   {
						   char buf[128];
						   Alarm::format_record(Alarm::g_records[0], buf, sizeof(buf));
						   send_with_485(buf);
					   }
				   }
				   if (!(ch1 <= thr1))
				   {
					   Alarm::add(utc, 1, thr1, ch1);
					   Alarm::save_to_flash();
					   if (Alarm::g_active)
					   {
						   char buf[128];
						   Alarm::format_record(Alarm::g_records[0], buf, sizeof(buf));
						   send_with_485(buf);
					   }
				   }
			   }},

	// 100ms: 自动上报 + 间隔判断
	TaskConfig{100, []
			   {
				   if (!DeviceState::g_auto_report_active) return;

				   uint32_t now = systick_tick_ms;
				   if (now >= DeviceState::g_auto_report_next)
				   {
					   DeviceState::g_auto_report_next = now + DeviceState::g_auto_report_interval_ms;

					   uint32_t utc = CmdDispatch::rtc_to_utc();
					   float ch0	= CmdDispatch::read_ch0() * Params::g_params.ch0_ratio;
					   float ch1	= CmdDispatch::read_ch1() * Params::g_params.ch1_ratio;
					   uint8_t buf[12];
					   buf[0] = (utc >> 24) & 0xFF;
					   buf[1] = (utc >> 16) & 0xFF;
					   buf[2] = (utc >> 8) & 0xFF;
					   buf[3] = utc & 0xFF;
					   DeviceState::float_to_bytes(ch0, buf + 4);
					   DeviceState::float_to_bytes(ch1, buf + 8);
					   CmdDispatch::send_response(Params::g_params.device_id, 0x0302, buf, 12);
				   }
			   }},

	// 500ms: LED 闪烁 + OLED 刷新
	TaskConfig{500, []
			   {
				   DeviceState::led_toggle();
				   DeviceState::oled_update();
			   }},

	// 1000ms: 心跳
	TaskConfig{1000, []
			   {
				   uint32_t now = systick_tick_ms;
				   if (!g_heartbeat_sent && (now > 100))
				   {
					   g_heartbeat_sent = true;
					   uint8_t buf[64];
					   uint16_t sz = Protocol::Response::build_heartbeat(
						   Params::g_params.device_id, buf, sizeof(buf));
					   if (sz) send_with_485(buf, sz);
				   }
			   }}>;

// ===================== main 入口 =====================
extern "C"
{
	int main(void)
	{
		// 初始化所有外设
		device_init_all();

		// 初始化环形缓冲区
		uart1_buffer::init();

		// 使能串口中断接收
		USART1::enable_it(0, 0);

		if (SysTick_Config(SystemCoreClock / 1000U))
		{
			while (1) {
				SEGGER_RTT_WriteString(0, "SysTick config failed!\r\n");
			}
		}
		NVIC_SetPriority(SysTick_IRQn, 0x00U);
		// 加载参数
		Params::load();

		// 初始化协议解析器
		g_proto.init();

		// 初始化告警
		Alarm::init();
		Alarm::load_from_flash();
		Alarm::g_active = (Params::g_params.alarm_mode == 0x01);

		// 上报间隔
		DeviceState::g_auto_report_interval_ms =
			(Params::g_params.report_interval == 1) ? 1000 : (Params::g_params.report_interval == 2) ? 3000
																									 : 5000;

		// OLED
		DeviceState::g_oled_status = DeviceState::OLEDStatus::IDLE;
		DeviceState::oled_update();

		SEGGER_RTT_WriteString(0, "=== CIMC APP v2.0.1.0 ===\r\n");

		while (1)
		{
			Scheduler::poll();
		}
	}

	void USART1_IRQHandler()
	{
		if (usart_interrupt_flag_get(HAL::gd32f4::registers::USART1_ADDR, USART_INT_FLAG_RBNE) == SET)
		{
			const uint8_t byte = usart_data_receive(HAL::gd32f4::registers::USART1_ADDR);
			uart1_buffer::write_byte(byte);
		}
	}

	void SysTick_Handler(void)
	{
		systick_tick_ms = systick_tick_ms + 1U;
	}

	// RTC 闹钟唤醒 ISR (睡眠 10s 后唤醒)
	void RTC_Alarm_IRQHandler()
	{
		if (rtc_flag_get(RTC_FLAG_ALRM0) == SET)
		{
			rtc_flag_clear(RTC_FLAG_ALRM0);
			rtc_alarm_disable(RTC_ALARM0);
			exti_flag_clear(EXTI_17);
		}
	}

	// picolibc 需要 _exit 桩
	void _exit(int)
	{
		while (1) __asm__ volatile("nop");
	}
}
