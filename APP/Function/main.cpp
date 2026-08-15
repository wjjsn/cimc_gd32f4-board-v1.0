#include "hardware.hpp"

#include "chry_ringbuffer.hpp"
#include "SEGGER_RTT.h"
#include "schedule.hpp"
#include "modbus_app.hpp"
#include "modbus_config.hpp"
#include "modbus_slave.hpp"
#include "sd_storage.h"

#include "button_task.hpp"

extern "C" {
#include "mb.h"
}

// 设备核心对象
#include "device.hpp"

// Driver 层
#include "Driver/device_init.hpp"


#include "core_cm4.h"

// ===================== 环形缓冲区 =====================
chry_ringbuffer_t ctx_uart1_buffer;
using Uart1RB = Cherry_RingBuffer<&ctx_uart1_buffer, 128>;

// ===================== 全局外设实例 =====================
gd30ad3344_on_spi1 g_adc;
Screen g_screen;

// ===================== 全局设备对象 =====================
Device<Uart1RB> g_device;

void modbus_app_get_snapshot(ModbusAppSnapshot &snapshot)
{
	g_device.modbus_get_snapshot(snapshot);
}

bool modbus_app_apply_snapshot(const ModbusAppSnapshot &snapshot,
			       uint32_t changes)
{
	return g_device.modbus_apply_snapshot(snapshot, changes);
}

void modbus_app_set_auto_report(bool enabled)
{
	g_device.modbus_set_auto_report(enabled);
}

void modbus_app_set_alarm_report(bool enabled)
{
	g_device.modbus_set_alarm_report(enabled);
}

void modbus_app_set_work_led(bool enabled)
{
	g_device.modbus_set_work_led(enabled);
}

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

	// 50ms: 睡眠状态机驱动 (OK 帧发送后延迟 ≥20ms 再入眠)
	TaskConfig{ 50, [] { g_device.sleep_tick(systick_tick_ms); } },

	// 500ms: OLED 刷新
	TaskConfig{ 500, [] { g_device.oled_update(); } },

	// 1000ms: 系统状态指示灯闪烁
	TaskConfig{ 1000, [] { system_status_led::toggle(); } },

	// 1000ms: 心跳
	TaskConfig{ 1000, [] { g_device.try_heartbeat(systick_tick_ms); } },
	TaskConfig{ 20,
		    [] {
			    key1::detect_key_click();
			    key2::detect_key_click();
			    key3::detect_key_click();
		    } },
	TaskConfig{ 200, [] {
			   key1::cope_click_data();
			   key2::cope_click_data();
			   key3::cope_click_data();
		   } }>;

// ===================== main 入口 =====================
extern "C" {
int main(void)
{
	device_init_all();
	// 挂载 SD 卡；成功后执行会写卡的完整自检。生产环境若不希望每次启动写卡，
	// 保留 sd_storage_init()，删除或改为按命令触发 sd_storage_self_test()。
	sd_storage_init();

	// 初始化环形缓冲区
	Uart1RB::init();

	// 使能串口中断接收
	USART1::enable_it(2, 0);

	// 初始化设备对象 (参数加载 + 协议 + 告警 + OLED)
	g_device.init();

	if (!modbus_slave_init()) {
		SEGGER_RTT_WriteString(0, "FreeModbus init failed!\r\n");
		while (1)
			__asm volatile("BKPT #0");
	}
	SEGGER_RTT_WriteString(
		0, ModbusConfig::mode == ModbusSerialMode::ascii ?
			   "FreeModbus ASCII ready\r\n" :
			   "FreeModbus RTU ready\r\n");

	if (SysTick_Config(SystemCoreClock / 1000U)) {
		while (1) {
			SEGGER_RTT_WriteString(0, "SysTick config failed!\r\n");
			__asm volatile("BKPT #0");
		}
	}
	while (1) {
		modbus_slave_poll();
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
