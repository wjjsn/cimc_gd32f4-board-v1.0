#include "hardware.hpp"

#include "chry_ringbuffer.hpp"
#include "SEGGER_RTT.h"
#include "schedule.hpp"

// 设备核心对象
#include "device.hpp"

// Driver 层
#include "../Driver/serial_send.hpp"
#include "../Driver/device_init.hpp"

#include "core_cm4.h"

// ===================== 环形缓冲区 =====================
chry_ringbuffer_t ctx_uart1_buffer;
using Uart1RB = Cherry_RingBuffer<&ctx_uart1_buffer, 128>;

// ===================== 全局外设实例 =====================
gd30ad3340_on_i2c0 g_adc;
Screen g_screen;

// ===================== 全局设备对象 =====================
Device<Uart1RB> g_device;

// ===================== SysTick =====================
static volatile uint32_t systick_tick_ms = 0;

uint64_t systick_get_ms()
{
	return systick_tick_ms;
}

// ===================== 调度器 =====================
using Scheduler = StaticTimerManager<
	systick_get_ms,

	// 5ms: 协议轮询与命令分发
	TaskConfig{ 5, [] { g_device.poll_frame(); } },

	// 10ms: 告警扫描
	TaskConfig{ 10, [] { g_device.alarm_scan(); } },

	// 100ms: 自动上报
	TaskConfig{ 100, [] { g_device.auto_report_tick(systick_tick_ms); } },

	// 50ms: 告警分批发送驱动 (每条记录间隔 50ms)
	TaskConfig{ 50, [] { g_device.alarm_send_tick(systick_tick_ms); } },

	// 500ms: OLED 刷新
	TaskConfig{ 500, [] { g_device.oled_update(); } },

	// 1000ms: 系统状态指示灯闪烁
	TaskConfig{ 1000, [] { system_status_led::toggle(); } },

	// 1000ms: 心跳
	TaskConfig{ 1000, [] { g_device.try_heartbeat(systick_tick_ms); } }>;

// ===================== main 入口 =====================
extern "C" {
int main(void)
{
	// 初始化所有外设
	device_init_all();

	// 初始化环形缓冲区
	Uart1RB::init();

	// 使能串口中断接收
	USART1::enable_it(0, 0);

	// 初始化设备对象 (参数加载 + 协议 + 告警 + OLED)
	g_device.init();

	if (SysTick_Config(SystemCoreClock / 1000U)) {
		while (1) {
			SEGGER_RTT_WriteString(0, "SysTick config failed!\r\n");
			__asm volatile("BKPT #0");
		}
	}
	NVIC_SetPriority(SysTick_IRQn, 0x00U);
	while (1) {
		Scheduler::poll();
	}
}

void USART1_IRQHandler()
{
	if (usart_interrupt_flag_get(HAL::gd32f4::registers::USART1_ADDR,
				     USART_INT_FLAG_RBNE) == SET) {
		const uint8_t byte =
			usart_data_receive(HAL::gd32f4::registers::USART1_ADDR);
		Uart1RB::write_byte(byte);
	}
}

void SysTick_Handler()
{
	systick_tick_ms = systick_tick_ms + 1;
}

// RTC 闹钟中断 — 清除标志位以使深度睡眠能被正常唤醒
void RTC_Alarm_IRQHandler()
{
	if (RESET != rtc_flag_get(RTC_FLAG_ALRM0)) {
		rtc_flag_clear(RTC_FLAG_ALRM0);
		exti_flag_clear(EXTI_17);
	}
}

void _exit(int)
{
	while (1)
		__asm volatile("nop");
}
}
