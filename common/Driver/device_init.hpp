#pragma once
// 设备全局初始化 — 统一入口 (APP / BootLoader 公用)

#include "hardware.hpp"
#include "serial_send.hpp"

// 前向声明 (实例定义在各工程 main.cpp)
extern gd30ad3344_on_spi1 g_adc;
extern Screen g_screen;

/// 初始化所有外设, 注意: GPIO 引脚由模板构造时自动初始化
inline void device_init_all()
{
	// LED
	system_status_led::init();
	work_status_led::init();

	// I2C + OLED
	I2C0_SCL::init();
	I2C0_SDA::init();
	I2C0_BUS::init();

	g_screen.init();

	// ADC0 (CH0 电位器: PC0=CH10, CH1 DAC回读: PC1=CH11)
	ADC0_GPIO::init();
	READ_BACK_DAC::init();
	ADC0::init();

	// RTC
	RTC::init();

	// SPI Flash (预留)
	SPI1_FLASH::init();

	// SPI ADC: GD30AD3344, CS=PD8, 内置参考源
	g_adc.init();

	// USART1 + RS485
	USART1::init();
	CS_485::init();

	// DAC
	DAC0_GPIO::init();
	DAC0::init();

	// 启用全局中断
	__enable_irq();
}
