#pragma once

#include "hal.hpp"
#include "gd30ad3340.hpp"
#include "OLED/ssd1306/0.91.hpp"
using system_status_led = HAL::gd32f4::GPIO<
	HAL::gd32f4::registers::GPIOE_ADDR, GPIO_PIN_3, GPIO_MODE_OUTPUT,
	GPIO_PUPD_PULLDOWN,
	HAL::gd32f4::OutputConfig<GPIO_OTYPE_PP, GPIO_OSPEED_2MHZ, SET> >;
using work_status_led = HAL::gd32f4::GPIO<
	HAL::gd32f4::registers::GPIOE_ADDR, GPIO_PIN_4, GPIO_MODE_OUTPUT,
	GPIO_PUPD_PULLDOWN,
	HAL::gd32f4::OutputConfig<GPIO_OTYPE_PP, GPIO_OSPEED_2MHZ, SET> >;

using ADC0_CONFIG = HAL::gd32f4::ADC_config<HAL::gd32f4::registers::ADC0_ADDR,
					    ADC_ADCCK_PCLK2_DIV6>;

using ADC0 = HAL::gd32f4::ADC<ADC0_CONFIG>;

using DAC0_CONFIG =
	HAL::gd32f4::DAC_config<HAL::gd32f4::registers::DAC0_ADDR>;

using DAC0 = HAL::gd32f4::DAC<DAC0_CONFIG>;

// ADC_CHANNEL_10 -> PC0, 必须配为 GPIO_MODE_ANALOG
// PULLDOWN 对齐 reference 2025030920/SysFunction/Src/main.c:26
using ADC0_GPIO =
	HAL::gd32f4::GPIO<HAL::gd32f4::registers::GPIOC_ADDR, GPIO_PIN_0,
			  GPIO_MODE_ANALOG, GPIO_PUPD_PULLDOWN,
			  HAL::gd32f4::AnalogConfig>;
using READ_BACK_DAC =
	HAL::gd32f4::GPIO<HAL::gd32f4::registers::GPIOC_ADDR, GPIO_PIN_1,
			  GPIO_MODE_ANALOG, GPIO_PUPD_PULLDOWN,
			  HAL::gd32f4::AnalogConfig>;

using KEY1_GPIO =
	HAL::gd32f4::GPIO<HAL::gd32f4::registers::GPIOB_ADDR, GPIO_PIN_11,
			  GPIO_MODE_INPUT, GPIO_PUPD_PULLUP, void>;
using KEY2_GPIO =
	HAL::gd32f4::GPIO<HAL::gd32f4::registers::GPIOE_ADDR, GPIO_PIN_15,
			  GPIO_MODE_INPUT, GPIO_PUPD_PULLUP, void>;
using KEY3_GPIO =
	HAL::gd32f4::GPIO<HAL::gd32f4::registers::GPIOE_ADDR, GPIO_PIN_13,
			  GPIO_MODE_INPUT, GPIO_PUPD_PULLUP, void>;
using KEY4_GPIO =
	HAL::gd32f4::GPIO<HAL::gd32f4::registers::GPIOE_ADDR, GPIO_PIN_11,
			  GPIO_MODE_INPUT, GPIO_PUPD_PULLUP, void>;
using KEY5_GPIO =
	HAL::gd32f4::GPIO<HAL::gd32f4::registers::GPIOE_ADDR, GPIO_PIN_9,
			  GPIO_MODE_INPUT, GPIO_PUPD_PULLUP, void>;
using KEY6_GPIO =
	HAL::gd32f4::GPIO<HAL::gd32f4::registers::GPIOE_ADDR, GPIO_PIN_7,
			  GPIO_MODE_INPUT, GPIO_PUPD_PULLUP, void>;

using DAC0_GPIO = HAL::gd32f4::GPIO<HAL::gd32f4::registers::GPIOA_ADDR,
				    GPIO_PIN_4, GPIO_MODE_ANALOG,
				    GPIO_PUPD_NONE, HAL::gd32f4::AnalogConfig>;

// ========== I2C 总线配置 ==========
// I2C0: SCL = PB8, SDA = PB9, AF4
using I2C0_SCL = HAL::gd32f4::GPIO<HAL::gd32f4::registers::GPIOB_ADDR,
				   GPIO_PIN_8, GPIO_MODE_AF, GPIO_PUPD_PULLUP,
				   HAL::gd32f4::AFConfig<GPIO_AF_4> >;
using I2C0_SDA = HAL::gd32f4::GPIO<HAL::gd32f4::registers::GPIOB_ADDR,
				   GPIO_PIN_9, GPIO_MODE_AF, GPIO_PUPD_PULLUP,
				   HAL::gd32f4::AFConfig<GPIO_AF_4> >;

using I2C0_BUS = HAL::gd32f4::I2C_bus<I2C0_SDA, I2C0_SCL,
				      HAL::gd32f4::registers::I2C0_ADDR,
				      400000>; // 400kHz 快速模式

// ========== GD30AD3340 设备 ==========
// 7-bit 地址 0x48 → 8-bit 写入地址 0x90 (ADDR 接 GND)
using ADC_I2C = HAL::gd32f4::I2C_device_addr<I2C0_BUS, 0x90>;
using gd30ad3340_on_i2c0 = GD30AD3340<ADC_I2C>;

#define OLED_ADDRESS 0x78
using OLED_I2C = HAL::gd32f4::I2C_device_addr<I2C0_BUS, OLED_ADDRESS>;
using Screen = OLED<OLED_I2C>;

using RTC = HAL::gd32f4::RTC_Device;

using WKUP = HAL::gd32f4::GPIO<HAL::gd32f4::registers::GPIOA_ADDR, GPIO_PIN_0,
			       GPIO_MODE_INPUT, GPIO_PUPD_NONE, void>;

using SPI1_MOSI = HAL::gd32f4::GPIO<HAL::gd32f4::registers::GPIOB_ADDR,
				    GPIO_PIN_15, GPIO_MODE_AF, GPIO_PUPD_NONE,
				    HAL::gd32f4::AFConfig<GPIO_AF_5> >;

using SPI1_MISO =
	HAL::gd32f4::GPIO<HAL::gd32f4::registers::GPIOB_ADDR, GPIO_PIN_14,
			  GPIO_MODE_AF, GPIO_PUPD_PULLDOWN,
			  HAL::gd32f4::AFConfig<GPIO_AF_5> >;

using SPI1_SCLK = HAL::gd32f4::GPIO<HAL::gd32f4::registers::GPIOB_ADDR,
				    GPIO_PIN_13, GPIO_MODE_AF, GPIO_PUPD_NONE,
				    HAL::gd32f4::AFConfig<GPIO_AF_5> >;

using SPI1_CONFIG = HAL::gd32f4::SPI_config<HAL::gd32f4::registers::SPI1_ADDR,
					    SPI_PSC_64, SPI_CK_PL_LOW_PH_1EDGE,
					    SPI_MASTER, SPI_NSS_SOFT>;

using SPI1_BUS = HAL::gd32f4::SPI<SPI1_MOSI, SPI1_MISO, SPI1_SCLK, SPI1_CONFIG>;

using SPI1_NSS_GPIO = HAL::gd32f4::GPIO<
	HAL::gd32f4::registers::GPIOB_ADDR, GPIO_PIN_12, GPIO_MODE_OUTPUT,
	GPIO_PUPD_PULLUP,
	HAL::gd32f4::OutputConfig<GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, SET> >;

using SPI1_FLASH = HAL::gd32f4::SPI_device<SPI1_BUS, SPI1_NSS_GPIO>;

using USART1_TX = HAL::gd32f4::GPIO<HAL::gd32f4::registers::GPIOA_ADDR,
				    GPIO_PIN_2, GPIO_MODE_AF, GPIO_PUPD_PULLUP,
				    HAL::gd32f4::AFConfig<GPIO_AF_7> >;

using USART1_RX = HAL::gd32f4::GPIO<HAL::gd32f4::registers::GPIOA_ADDR,
				    GPIO_PIN_3, GPIO_MODE_AF, GPIO_PUPD_NONE,
				    HAL::gd32f4::AFConfig<GPIO_AF_7> >;

using USART1 =
	HAL::gd32f4::USART_Device<USART1_TX, USART1_RX,
				  HAL::gd32f4::registers::USART1_ADDR, 19200,
				  USART_PM_NONE, USART_WL_8BIT, USART_STB_1BIT>;

using CS_485 = HAL::gd32f4::GPIO<
	HAL::gd32f4::registers::GPIOA_ADDR, GPIO_PIN_1, GPIO_MODE_OUTPUT,
	GPIO_PUPD_PULLDOWN,
	HAL::gd32f4::OutputConfig<GPIO_OTYPE_PP, GPIO_OSPEED_2MHZ, SET> >;
