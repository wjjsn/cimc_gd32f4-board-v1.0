#pragma once
// 设备全局初始化 — 统一入口

#include "../Function/hardware.hpp"
#include "serial_send.hpp"
#include "dac_driver.hpp"

// 前向声明 (实例定义在 cmd_handlers.hpp / device_state.hpp)
extern ADC g_adc;
extern Screen g_screen;

/// 初始化所有外设, 注意: GPIO 引脚由模板构造时自动初始化
inline void device_init_all()
{
	// LED
	LED::init();

	// I2C + OLED + 外部ADC
	I2C0_SCL::init();
	I2C0_SDA::init();
	I2C0_BUS::init();

	g_screen.init();
	g_adc.init(ADC::MUX_AIN0_GND,
			   ADC::PGA_2048,
			   ADC::MODE_CONTINUOUS,
			   ADC::DR_100,
			   ADC::COMP_QUE_DIS);

	// ADC0 (CH0 电位器: PC0=CH10, CH1 DAC回读: PC1=CH11)
	ADC0_GPIO::init();
	ADC0::init();

	// RTC
	RTC::init();

	// SPI Flash (预留)
	SPI1_FLASH::init();

	// USART1 + RS485
	USART1::init();
	CS_485::init();

	// DAC
	DAC0_GPIO::init();
	dac_init();
}
