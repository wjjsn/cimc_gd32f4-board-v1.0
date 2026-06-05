#pragma once

#ifdef __cplusplus
#include <cstdint>
#include <concepts>
#include <cmath>
extern "C"
{
#include "gd32f4xx.h"
#include "gd32f4xx_gpio.h"
#include "gd32f4xx_rcu.h"
#include "gd32f4xx_usart.h"
#include "gd32f4xx_i2c.h"
#include "gd32f4xx_spi.h"
#include "gd32f4xx_timer.h"
#include "gd32f4xx_misc.h"
}
namespace HAL
{
	namespace gd32f4
	{
		// Save peripheral base addresses before undefining macros
		// These constants are used as template parameters
		namespace registers {
			constexpr uint32_t GPIOA_ADDR = GPIOA;
			constexpr uint32_t GPIOB_ADDR = GPIOB;
			constexpr uint32_t GPIOC_ADDR = GPIOC;
			constexpr uint32_t GPIOD_ADDR = GPIOD;
			constexpr uint32_t GPIOE_ADDR = GPIOE;
			constexpr uint32_t GPIOF_ADDR = GPIOF;
			constexpr uint32_t GPIOG_ADDR = GPIOG;
			constexpr uint32_t GPIOH_ADDR = GPIOH;
			constexpr uint32_t GPIOI_ADDR = GPIOI;
			constexpr uint32_t TIMER0_ADDR = TIMER0;
			constexpr uint32_t TIMER1_ADDR = TIMER1;
			constexpr uint32_t TIMER2_ADDR = TIMER2;
			constexpr uint32_t TIMER3_ADDR = TIMER3;
			constexpr uint32_t TIMER4_ADDR = TIMER4;
			constexpr uint32_t TIMER5_ADDR = TIMER5;
			constexpr uint32_t TIMER6_ADDR = TIMER6;
			constexpr uint32_t TIMER7_ADDR = TIMER7;
			constexpr uint32_t TIMER8_ADDR = TIMER8;
			constexpr uint32_t TIMER9_ADDR = TIMER9;
			constexpr uint32_t TIMER10_ADDR = TIMER10;
			constexpr uint32_t TIMER11_ADDR = TIMER11;
			constexpr uint32_t TIMER12_ADDR = TIMER12;
			constexpr uint32_t TIMER13_ADDR = TIMER13;
			constexpr uint32_t USART0_ADDR = USART0;
			constexpr uint32_t USART1_ADDR = USART1;
			constexpr uint32_t USART2_ADDR = USART2;
			constexpr uint32_t USART5_ADDR = USART5;
			constexpr uint32_t UART3_ADDR = UART3;
			constexpr uint32_t UART4_ADDR = UART4;
			constexpr uint32_t UART6_ADDR = UART6;
			constexpr uint32_t UART7_ADDR = UART7;
			constexpr uint32_t I2C0_ADDR = I2C0;
			constexpr uint32_t I2C1_ADDR = I2C1;
			constexpr uint32_t I2C2_ADDR = I2C2;
			constexpr uint32_t SPI0_ADDR = SPI0;
			constexpr uint32_t SPI1_ADDR = SPI1;
			constexpr uint32_t SPI2_ADDR = SPI2;
			constexpr uint32_t SPI3_ADDR = SPI3;
			constexpr uint32_t SPI4_ADDR = SPI4;
			constexpr uint32_t SPI5_ADDR = SPI5;
			constexpr uint32_t ADC0_ADDR = ADC0;
			constexpr uint32_t ADC1_ADDR = ADC1;
			constexpr uint32_t ADC2_ADDR = ADC2;
		}

		// Undef macros that conflict with C++ identifiers or template parameters
		#undef SPI_BASE
		#undef RTC_BASE
		#undef USART_BASE
		#undef SPI0
		#undef SPI1
		#undef SPI2
		#undef SPI3
		#undef SPI4
		#undef SPI5
		#undef TIMER0
		#undef TIMER1
		#undef TIMER2
		#undef TIMER3
		#undef TIMER4
		#undef TIMER5
		#undef TIMER6
		#undef TIMER7
		#undef TIMER8
		#undef TIMER9
		#undef TIMER10
		#undef TIMER11
		#undef TIMER12
		#undef TIMER13
		#undef USART0
		#undef USART1
		#undef USART2
		#undef USART5
		#undef UART3
		#undef UART4
		#undef UART6
		#undef UART7
		#undef ADC0
		#undef ADC1
		#undef ADC2
		#undef I2C0
		#undef I2C1
		#undef I2C2
		#undef GPIOA
		#undef GPIOB
		#undef GPIOC
		#undef GPIOD
		#undef GPIOE
		#undef GPIOF
		#undef GPIOG
		#undef GPIOH
		#undef GPIOI

		template <uint32_t PERIPH>
		struct RCU_periph
		{
			static constexpr rcu_periph_enum periph = []
			{
				if constexpr (PERIPH == registers::GPIOA_ADDR)
					return RCU_GPIOA;
				else if constexpr (PERIPH == registers::GPIOB_ADDR)
					return RCU_GPIOB;
				else if constexpr (PERIPH == registers::GPIOC_ADDR)
					return RCU_GPIOC;
				else if constexpr (PERIPH == registers::GPIOD_ADDR)
					return RCU_GPIOD;
				else if constexpr (PERIPH == registers::GPIOE_ADDR)
					return RCU_GPIOE;
				else if constexpr (PERIPH == registers::GPIOF_ADDR)
					return RCU_GPIOF;
				else if constexpr (PERIPH == registers::GPIOG_ADDR)
					return RCU_GPIOG;
				else if constexpr (PERIPH == registers::GPIOH_ADDR)
					return RCU_GPIOH;
				else if constexpr (PERIPH == registers::GPIOI_ADDR)
					return RCU_GPIOI;
				else if constexpr (PERIPH == registers::TIMER0_ADDR)
					return RCU_TIMER0;
				else if constexpr (PERIPH == registers::TIMER1_ADDR)
					return RCU_TIMER1;
				else if constexpr (PERIPH == registers::TIMER2_ADDR)
					return RCU_TIMER2;
				else if constexpr (PERIPH == registers::TIMER3_ADDR)
					return RCU_TIMER3;
				else if constexpr (PERIPH == registers::TIMER4_ADDR)
					return RCU_TIMER4;
				else if constexpr (PERIPH == registers::TIMER5_ADDR)
					return RCU_TIMER5;
				else if constexpr (PERIPH == registers::TIMER6_ADDR)
					return RCU_TIMER6;
				else if constexpr (PERIPH == registers::TIMER7_ADDR)
					return RCU_TIMER7;
				else if constexpr (PERIPH == registers::TIMER8_ADDR)
					return RCU_TIMER8;
				else if constexpr (PERIPH == registers::TIMER9_ADDR)
					return RCU_TIMER9;
				else if constexpr (PERIPH == registers::TIMER10_ADDR)
					return RCU_TIMER10;
				else if constexpr (PERIPH == registers::TIMER11_ADDR)
					return RCU_TIMER11;
				else if constexpr (PERIPH == registers::TIMER12_ADDR)
					return RCU_TIMER12;
				else if constexpr (PERIPH == registers::TIMER13_ADDR)
					return RCU_TIMER13;
				else if constexpr (PERIPH == registers::USART0_ADDR)
					return RCU_USART0;
				else if constexpr (PERIPH == registers::USART1_ADDR)
					return RCU_USART1;
				else if constexpr (PERIPH == registers::USART2_ADDR)
					return RCU_USART2;
				else if constexpr (PERIPH == registers::USART5_ADDR)
					return RCU_USART5;
				else if constexpr (PERIPH == registers::UART3_ADDR)
					return RCU_UART3;
				else if constexpr (PERIPH == registers::UART4_ADDR)
					return RCU_UART4;
				else if constexpr (PERIPH == registers::UART6_ADDR)
					return RCU_UART6;
				else if constexpr (PERIPH == registers::UART7_ADDR)
					return RCU_UART7;
				else if constexpr (PERIPH == registers::I2C0_ADDR)
					return RCU_I2C0;
				else if constexpr (PERIPH == registers::I2C1_ADDR)
					return RCU_I2C1;
				else if constexpr (PERIPH == registers::I2C2_ADDR)
					return RCU_I2C2;
				else if constexpr (PERIPH == registers::SPI0_ADDR)
					return RCU_SPI0;
				else if constexpr (PERIPH == registers::SPI1_ADDR)
					return RCU_SPI1;
				else if constexpr (PERIPH == registers::SPI2_ADDR)
					return RCU_SPI2;
				else if constexpr (PERIPH == registers::SPI3_ADDR)
					return RCU_SPI3;
				else if constexpr (PERIPH == registers::SPI4_ADDR)
					return RCU_SPI4;
				else if constexpr (PERIPH == registers::SPI5_ADDR)
					return RCU_SPI5;
				else if constexpr (PERIPH == registers::ADC0_ADDR)
					return RCU_ADC0;
				else if constexpr (PERIPH == registers::ADC1_ADDR)
					return RCU_ADC1;
				else if constexpr (PERIPH == registers::ADC2_ADDR)
					return RCU_ADC2;
				else
					return []<bool b = false>()
					{ static_assert(b, "Unsupported PERIPH"); return 0; }();
			}();
		};

		/**
		* @brief  GPIO输出模式配置参数
		* @param  OTYPE: 输出类型 (GPIO_OTYPE_PP:推挽/GPIO_OTYPE_OD:开漏)
		* @param  SPEED: 输出速度 (GPIO_OSPEED_2MHZ/25MHZ/50MHZ/MAX)
		* @param  VAL: 初始值 (RESET/SET)
		*/
		template <uint8_t OTYPE, uint32_t SPEED, bit_status VAL>
		struct OutputConfig
		{
			static constexpr uint8_t otype	= OTYPE;
			static constexpr uint32_t speed = SPEED;
			static constexpr bit_status val = VAL;
		};

		/**
		* @brief  GPIO复用功能模式配置参数
		* @param  AF_NUM: 复用功能号 (GPIO_AF_0-GPIO_AF_15)
		*/
		template <uint32_t AF_NUM>
		struct AFConfig
		{
			static constexpr uint32_t af_num = AF_NUM;
		};

		/**
		* @brief  GPIO模拟模式配置参数 (无配置, 仅作 tag)
		* @note   用于 ADC 输入 / DAC 输出引脚, 配为 GPIO_MODE_ANALOG
		*/
		struct AnalogConfig
		{
		};

		/**
		* @brief  GPIO通用输入输出
		* @note   支持四种模式: OutputConfig(输出), AFConfig(复用功能), void(输入), AnalogConfig(模拟)
		* @param  GPIOx: GPIO端口 (GPIOA-GPIOI)
		* @param  Pin: GPIO引脚 (GPIO_PIN_0-GPIO_PIN_15)
		* @param  Mode: GPIO模式 (GPIO_MODE_INPUT/GPIO_MODE_OUTPUT/GPIO_MODE_AF/GPIO_MODE_ANALOG)
		* @param  PULL: 上下拉配置 (GPIO_PUPD_NONE/GPIO_PUPD_PULLUP/GPIO_PUPD_PULLDOWN)
		* @param  config: OutputConfig/AFConfig/AnalogConfig/void
		*/
		template <uint32_t GPIOx, uint16_t Pin, uint32_t Mode, uint32_t PULL, typename config>
		struct GPIO
		{
			static_assert((false), "fallback");
		};

		/**
		* @brief  GPIO输出模式配置
		* @param  OTYPE: 输出类型 (GPIO_OTYPE_PP:推挽/GPIO_OTYPE_OD:开漏)
		* @param  SPEED: 输出速度 (GPIO_OSPEED_2MHZ/25MHZ/50MHZ/MAX)
		* @param  VAL: 初始值 (RESET/SET)
		*/
		template <uint32_t GPIOx, uint16_t Pin, uint32_t PULL, uint8_t OTYPE, uint32_t SPEED, bit_status VAL>
		struct GPIO<GPIOx, Pin, GPIO_MODE_OUTPUT, PULL, OutputConfig<OTYPE, SPEED, VAL>>
		{
			static void init()
			{
				rcu_periph_clock_enable(RCU_periph<GPIOx>::periph);
				gpio_mode_set(GPIOx, GPIO_MODE_OUTPUT, PULL, Pin);
				gpio_output_options_set(GPIOx, OTYPE, SPEED, Pin);
				gpio_bit_write(GPIOx, Pin, VAL);
			}
			static void set()
			{
				gpio_bit_write(GPIOx, Pin, SET);
			}
			static void clear()
			{
				gpio_bit_reset(GPIOx, Pin);
			}
			static void toggle()
			{
				gpio_bit_toggle(GPIOx, Pin);
			}
			static bool read()
			{
				return gpio_input_bit_get(GPIOx, Pin) == SET;
			}
		};

		/**
		* @brief  GPIO复用功能模式配置
		* @param  AF_NUM: 复用功能号
		* @arg    GPIO_AF_0: SYSTEM
		* @arg    GPIO_AF_1: TIMER0, TIMER1
		* @arg    GPIO_AF_2: TIMER2, TIMER3, TIMER4
		* @arg    GPIO_AF_3: TIMER7, TIMER8, TIMER9, TIMER10
		* @arg    GPIO_AF_4: I2C0, I2C1, I2C2
		* @arg    GPIO_AF_5: SPI0, SPI1, SPI2, SPI3, SPI4, SPI5
		* @arg    GPIO_AF_6: SPI2, SPI3, SPI4
		* @arg    GPIO_AF_7: USART0, USART1, USART2, SPI1, SPI2
		* @arg    GPIO_AF_8: UART3, UART4, USART5, UART6, UART7
		* @arg    GPIO_AF_9: CAN0, CAN1, TLI, TIMER11, TIMER12, TIMER13, I2C1, I2C2, CTC
		* @arg    GPIO_AF_10: USB_FS, USB_HS
		* @arg    GPIO_AF_11: ENET
		* @arg    GPIO_AF_12: EXMC, SDIO, USB_HS
		* @arg    GPIO_AF_13: DCI
		* @arg    GPIO_AF_14: TLI
		* @arg    GPIO_AF_15: EVENTOUT
		*/
		template <uint32_t GPIOx, uint16_t Pin, uint32_t PULL, uint32_t AF_NUM>
		struct GPIO<GPIOx, Pin, GPIO_MODE_AF, PULL, AFConfig<AF_NUM>>
		{
			using af_config = AFConfig<AF_NUM>;

			static void init()
			{
				rcu_periph_clock_enable(RCU_periph<GPIOx>::periph);
				gpio_mode_set(GPIOx, GPIO_MODE_AF, PULL, Pin);
				gpio_af_set(GPIOx, AF_NUM, Pin);
			}
			static void set()
			{
				gpio_bit_write(GPIOx, Pin, SET);
			}
			static void clear()
			{
				gpio_bit_reset(GPIOx, Pin);
			}
			static void toggle()
			{
				gpio_bit_toggle(GPIOx, Pin);
			}
			static bool read()
			{
				return gpio_input_bit_get(GPIOx, Pin) == SET;
			}
		};

		/**
		* @brief  GPIO输入模式
		* @param  PULL: 上下拉配置 (GPIO_PUPD_NONE/GPIO_PUPD_PULLUP/GPIO_PUPD_PULLDOWN)
		*/
		template <uint32_t GPIOx, uint16_t Pin, uint32_t PULL>
		struct GPIO<GPIOx, Pin, GPIO_MODE_INPUT, PULL, void>
		{
			static void init()
			{
				rcu_periph_clock_enable(RCU_periph<GPIOx>::periph);
				gpio_mode_set(GPIOx, GPIO_MODE_INPUT, PULL, Pin);
			}
			static bool read()
			{
				return gpio_input_bit_get(GPIOx, Pin) == SET;
			}
		};

		/**
		* @brief  GPIO模拟模式 (用于 ADC 输入 / DAC 输出)
		* @note   仅暴露 init(), 不提供 set/clear/toggle/read (模拟模式无意义)
		* @param  PULL: 上下拉配置 (GPIO_PUPD_NONE/GPIO_PUPD_PULLUP/GPIO_PUPD_PULLDOWN)
		*/
		template <uint32_t GPIOx, uint16_t Pin, uint32_t PULL>
		struct GPIO<GPIOx, Pin, GPIO_MODE_ANALOG, PULL, AnalogConfig>
		{
			static void init()
			{
				rcu_periph_clock_enable(RCU_periph<GPIOx>::periph);
				gpio_mode_set(GPIOx, GPIO_MODE_ANALOG, PULL, Pin);
			}
		};

		/**
		 * @brief  I2C总线层
		 * @note   SDA/SCL必须配置为GPIO_AF_4
		 * @param  GPIO_SDA: SDA引脚 (AFConfig<GPIO_AF_4>)
		 * @param  GPIO_SCL: SCL引脚 (AFConfig<GPIO_AF_4>)
		 * @param  I2Cx: I2C外设 (I2C0/I2C1/I2C2)
		 * @param  clkspeed: 时钟速度 (Hz, 推荐100000/400000)
		 * @param  own_address7: 本机7位地址 (默认0x00)
		 * @note   transmit/receive 的 slave_addr 由调用方传入 (7-bit 地址)
		 */
		template <typename GPIO_SDA, typename GPIO_SCL, uint32_t I2Cx, uint32_t clkspeed, uint8_t own_address7 = 0x00>
			requires(
				GPIO_SDA::af_config::af_num == GPIO_AF_4 &&
				GPIO_SCL::af_config::af_num == GPIO_AF_4)
		struct I2C_bus
		{
			static constexpr uint32_t periph = I2Cx;

			static void init()
			{
				GPIO_SDA::init();
				GPIO_SCL::init();
				i2c_deinit(periph);
				rcu_periph_clock_enable(RCU_periph<I2Cx>::periph);

				i2c_clock_config(I2Cx, clkspeed, I2C_DTCY_2);
				i2c_mode_addr_config(I2Cx, I2C_I2CMODE_ENABLE, I2C_ADDFORMAT_7BITS, own_address7);
				i2c_enable(I2Cx);
				i2c_ack_config(I2Cx, I2C_ACK_ENABLE);
			}

			static void transmit(uint8_t slave_addr, uint8_t *p, uint16_t n, uint32_t t)
			{
				(void)t;
				while (i2c_flag_get(I2Cx, I2C_FLAG_I2CBSY));
				i2c_start_on_bus(I2Cx);
				while (!i2c_flag_get(I2Cx, I2C_FLAG_SBSEND));
				i2c_master_addressing(I2Cx, slave_addr, I2C_TRANSMITTER);
				while (!i2c_flag_get(I2Cx, I2C_FLAG_ADDSEND));
				i2c_flag_clear(I2Cx, I2C_FLAG_ADDSEND);
				for (uint16_t i = 0; i < n; i++)
				{
					while (!i2c_flag_get(I2Cx, I2C_FLAG_TBE));
					i2c_data_transmit(I2Cx, *(p + i));
				}
				while (!i2c_flag_get(I2Cx, I2C_FLAG_BTC));
				i2c_stop_on_bus(I2Cx);
				while (I2C_CTL0(I2Cx) & I2C_CTL0_STOP);
			}

			static void receive(uint8_t slave_addr, uint8_t *p, uint16_t n, uint32_t t)
			{
				(void)t;
				while (i2c_flag_get(I2Cx, I2C_FLAG_I2CBSY));
				i2c_start_on_bus(I2Cx);
				while (!i2c_flag_get(I2Cx, I2C_FLAG_SBSEND));
				i2c_master_addressing(I2Cx, slave_addr, I2C_RECEIVER);
				while (!i2c_flag_get(I2Cx, I2C_FLAG_ADDSEND));
				i2c_flag_clear(I2Cx, I2C_FLAG_ADDSEND);
				if (n == 1)
				{
					i2c_ack_config(I2Cx, I2C_ACK_DISABLE);
					while (!i2c_flag_get(I2Cx, I2C_FLAG_RBNE));
					*p = i2c_data_receive(I2Cx);
					i2c_stop_on_bus(I2Cx);
				}
				else
				{
					while (!i2c_flag_get(I2Cx, I2C_FLAG_RBNE));
					*p = i2c_data_receive(I2Cx);
					for (uint16_t i = 1; i < n - 1; i++)
					{
						i2c_ack_config(I2Cx, I2C_ACK_ENABLE);
						while (!i2c_flag_get(I2Cx, I2C_FLAG_RBNE));
						*(p + i) = i2c_data_receive(I2Cx);
					}
					i2c_ack_config(I2Cx, I2C_ACK_DISABLE);
					while (!i2c_flag_get(I2Cx, I2C_FLAG_RBNE));
					*(p + n - 1) = i2c_data_receive(I2Cx);
					i2c_stop_on_bus(I2Cx);
				}
				while (I2C_CTL0(I2Cx) & I2C_CTL0_STOP);
			}
		};

		/**
		 * @brief  I2C设备层 (7-bit 地址)
		 * @param  bus_t: I2C_bus 类型
		 * @param  slave_address: 从设备7位地址
		 */
		template <typename bus_t, uint8_t slave_address>
		struct I2C_device_addr
		{
			static void init()
			{
				bus_t::init();
			}

			static void transmit(uint8_t *p, uint16_t n, uint32_t t)
			{
				bus_t::transmit(slave_address, p, n, t);
			}

			static void receive(uint8_t *p, uint16_t n, uint32_t t)
			{
				bus_t::receive(slave_address, p, n, t);
			}

			static void mem_write(uint16_t reg, uint8_t *p, uint16_t n, uint32_t t)
			{
				(void)t;
				const uint32_t I2Cx = bus_t::periph;
				while (i2c_flag_get(I2Cx, I2C_FLAG_I2CBSY));
				i2c_start_on_bus(I2Cx);
				while (!i2c_flag_get(I2Cx, I2C_FLAG_SBSEND));
				i2c_master_addressing(I2Cx, slave_address, I2C_TRANSMITTER);
				while (!i2c_flag_get(I2Cx, I2C_FLAG_ADDSEND));
				i2c_flag_clear(I2Cx, I2C_FLAG_ADDSEND);
				while (!i2c_flag_get(I2Cx, I2C_FLAG_TBE));
				i2c_data_transmit(I2Cx, reg);
				while (!i2c_flag_get(I2Cx, I2C_FLAG_BTC));
				for (uint16_t i = 0; i < n; i++)
				{
					while (!i2c_flag_get(I2Cx, I2C_FLAG_TBE));
					i2c_data_transmit(I2Cx, *(p + i));
				}
				while (!i2c_flag_get(I2Cx, I2C_FLAG_BTC));
				i2c_stop_on_bus(I2Cx);
				while (I2C_CTL0(I2Cx) & I2C_CTL0_STOP);
			}

			static void mem_read(uint16_t reg, uint8_t *p, uint16_t n, uint32_t t)
			{
				(void)t;
				const uint32_t I2Cx = bus_t::periph;
				while (i2c_flag_get(I2Cx, I2C_FLAG_I2CBSY));
				i2c_start_on_bus(I2Cx);
				while (!i2c_flag_get(I2Cx, I2C_FLAG_SBSEND));
				i2c_master_addressing(I2Cx, slave_address, I2C_TRANSMITTER);
				while (!i2c_flag_get(I2Cx, I2C_FLAG_ADDSEND));
				i2c_flag_clear(I2Cx, I2C_FLAG_ADDSEND);
				while (!i2c_flag_get(I2Cx, I2C_FLAG_TBE));
				i2c_data_transmit(I2Cx, reg);
				while (!i2c_flag_get(I2Cx, I2C_FLAG_BTC));

				i2c_start_on_bus(I2Cx);
				while (!i2c_flag_get(I2Cx, I2C_FLAG_SBSEND));
				i2c_master_addressing(I2Cx, slave_address, I2C_RECEIVER);
				while (!i2c_flag_get(I2Cx, I2C_FLAG_ADDSEND));
				i2c_flag_clear(I2Cx, I2C_FLAG_ADDSEND);

				if (n == 1)
				{
					i2c_ack_config(I2Cx, I2C_ACK_DISABLE);
					while (!i2c_flag_get(I2Cx, I2C_FLAG_RBNE));
					*p = i2c_data_receive(I2Cx);
					i2c_stop_on_bus(I2Cx);
				}
				else
				{
					while (!i2c_flag_get(I2Cx, I2C_FLAG_RBNE));
					*p = i2c_data_receive(I2Cx);
					i2c_ack_config(I2Cx, I2C_ACK_ENABLE);
					for (uint16_t i = 1; i < n - 1; i++)
					{
						while (!i2c_flag_get(I2Cx, I2C_FLAG_RBNE));
						*(p + i) = i2c_data_receive(I2Cx);
					}
					i2c_ack_config(I2Cx, I2C_ACK_DISABLE);
					// while (!i2c_flag_get(I2Cx, I2C_FLAG_RBNE));
					*(p + n - 1) = i2c_data_receive(I2Cx);
					i2c_stop_on_bus(I2Cx);
				}
				while (I2C_CTL0(I2Cx) & I2C_CTL0_STOP);
			}
		};

		/**
		* @brief  USART/UART串口通信
		* @param  GPIO_TX: TX引脚 (AF7或AF8)
		* @param  GPIO_RX: RX引脚 (AF7或AF8)
		* @param  USARTx: USART外设 (USART0/USART1/USART2/USART5/UART3/UART4/UART6/UART7)
		* @param  baudval: 波特率 (如115200)
		* @param  paritycfg: 校验配置 (USART_PM_NONE/USART_PM_EVEN/USART_PM_ODD)
		* @param  wlen: 字长 (USART_WL_8BIT/USART_WL_9BIT)
		* @param  stblen: 停止位 (USART_STB_1BIT/USART_STB_0_5BIT/USART_STB_2BIT/USART_STB_1_5BIT)
		*/
		template <typename GPIO_TX, typename GPIO_RX, uint32_t USARTx, uint32_t baudval,
				  uint32_t paritycfg = USART_PM_NONE, uint32_t wlen = USART_WL_8BIT, uint32_t stblen = USART_STB_1BIT>
			requires(
				(GPIO_TX::af_config::af_num == GPIO_AF_7 || GPIO_TX::af_config::af_num == GPIO_AF_8) &&
				(GPIO_RX::af_config::af_num == GPIO_AF_7 || GPIO_RX::af_config::af_num == GPIO_AF_8))
		struct USART_Device
		{
			static constexpr IRQn_Type get_irqn()
			{
				if constexpr (USARTx == registers::USART0_ADDR)
					return USART0_IRQn;
				else if constexpr (USARTx == registers::USART1_ADDR)
					return USART1_IRQn;
				else if constexpr (USARTx == registers::USART2_ADDR)
					return USART2_IRQn;
				else if constexpr (USARTx == registers::USART5_ADDR)
					return USART5_IRQn;
				else if constexpr (USARTx == registers::UART3_ADDR)
					return UART3_IRQn;
				else if constexpr (USARTx == registers::UART4_ADDR)
					return UART4_IRQn;
				else if constexpr (USARTx == registers::UART6_ADDR)
					return UART6_IRQn;
				else if constexpr (USARTx == registers::UART7_ADDR)
					return UART7_IRQn;
				else
					return USART0_IRQn;
			}

			static void init()
			{
				GPIO_TX::init();
				GPIO_RX::init();

				rcu_periph_clock_enable(RCU_periph<USARTx>::periph);

				usart_deinit(USARTx);
				usart_baudrate_set(USARTx, baudval);
				usart_parity_config(USARTx, paritycfg);
				usart_word_length_set(USARTx, wlen);
				usart_stop_bit_set(USARTx, stblen);
				usart_transmit_config(USARTx, USART_TRANSMIT_ENABLE);
				usart_receive_config(USARTx, USART_RECEIVE_ENABLE);
				usart_enable(USARTx);
			}

			static void transmit(const uint8_t *pData, uint16_t Size, uint32_t Timeout=0)
			{
				(void)Timeout;
				for (uint16_t i = 0; i < Size; i++)
				{
					while (usart_flag_get(USARTx, USART_FLAG_TBE) == RESET);
					usart_data_transmit(USARTx, *(pData + i));
				}
				while (usart_flag_get(USARTx, USART_FLAG_TC) == RESET);
			}

			static void enable_it(uint8_t priority, uint8_t sub_priority)
			{
				nvic_irq_enable(get_irqn(), priority, sub_priority);
				usart_interrupt_enable(USARTx, USART_INT_RBNE);
			}
		};

		// SPI 配置参数
		/**
		* @brief  SPI配置参数
		* @param  SPIx: SPI外设 (SPI0-SPI5)
		* @param  prescaler: 预分频 (SPI_PSC_2/4/8/16/32/64/128/256)
		* @param  clock_polarity_phase: 时钟极性和相位 (SPI_CK_PL_LOW_PH_1EDGE/SPI_CK_PL_LOW_PH_2EDGE等)
		* @param  device_mode: 主从模式 (SPI_MASTER/SPI_SLAVE)
		* @param  nss: 片选模式 (SPI_NSS_SOFT/SPI_NSS_HARD)
		* @param  transmode: 传输模式 (SPI_TRANSMODE_FULLDUPLEX/SPI_TRANSMODE_RECEIVEONLY等)
		* @param  framesize: 数据帧长度 (SPI_FRAMESIZE_8BIT/16BIT)
		* @param  endian: 字节序 (SPI_ENDIAN_MSB/SPI_ENDIAN_LSB)
		*/
		template <uint32_t SPIx, uint32_t prescaler = SPI_PSC_64,
				  uint32_t clock_polarity_phase = SPI_CK_PL_LOW_PH_2EDGE,
				  uint32_t device_mode = SPI_MASTER, uint32_t nss = SPI_NSS_SOFT,
				  uint32_t transmode = SPI_TRANSMODE_FULLDUPLEX,
				  uint32_t framesize = SPI_FRAMESIZE_8BIT,
				  uint32_t endian = SPI_ENDIAN_MSB>
		struct SPI_config
		{
			static constexpr uint32_t spi_periph = SPIx;
			static constexpr uint32_t prescaler_val = prescaler;
			static constexpr uint32_t ckp = clock_polarity_phase;
			static constexpr uint32_t mode = device_mode;
			static constexpr uint32_t nss_mode = nss;
			static constexpr uint32_t transmode_val = transmode;
			static constexpr uint32_t framesize_val = framesize;
			static constexpr uint32_t endian_val = endian;
		};

		/**
		* @brief  SPI主设备
		* @note   MOSI/MISO/SCLK必须配置为GPIO_AF_5
		* @param  GPIO_MOSI: MOSI引脚 (AFConfig<GPIO_AF_5>)
		* @param  GPIO_MISO: MISO引脚 (AFConfig<GPIO_AF_5>)
		* @param  GPIO_SCLK: SCLK引脚 (AFConfig<GPIO_AF_5>)
		* @param  SPI_CONFIG: SPI_config配置模板
		*/
		template <typename GPIO_MOSI, typename GPIO_MISO, typename GPIO_SCLK,
				  typename SPI_CONFIG>
			requires(
				GPIO_MOSI::af_config::af_num == GPIO_AF_5 &&
				GPIO_MISO::af_config::af_num == GPIO_AF_5 &&
				GPIO_SCLK::af_config::af_num == GPIO_AF_5)
		struct SPI
		{
			static constexpr uint32_t periph = SPI_CONFIG::spi_periph;

			static void init()
			{
				GPIO_MOSI::init();
				GPIO_MISO::init();
				GPIO_SCLK::init();

				rcu_periph_clock_enable(RCU_periph<SPI_CONFIG::spi_periph>::periph);

				spi_parameter_struct spi_init_struct = {
					SPI_CONFIG::mode,
					SPI_CONFIG::transmode_val,
					SPI_CONFIG::framesize_val,
					SPI_CONFIG::nss_mode,
					SPI_CONFIG::endian_val,
					SPI_CONFIG::ckp,
					SPI_CONFIG::prescaler_val};
				spi_init(SPI_CONFIG::spi_periph, &spi_init_struct);
				spi_enable(SPI_CONFIG::spi_periph);
			}

			// 全双工 (不控 CS)
			static void transfer(uint8_t *p, uint16_t n, uint32_t t)
			{
				(void)t;
				for (uint16_t i = 0; i < n; i++)
				{
					while (spi_i2s_flag_get(periph, SPI_FLAG_TBE) == RESET);
					spi_i2s_data_transmit(periph, *(p + i));
					while (spi_i2s_flag_get(periph, SPI_FLAG_RBNE) == RESET);
					*(p + i) = spi_i2s_data_receive(periph);
				}
			}

			// 主发送 (不控 CS): 委托给 transfer, 逐字节发送并丢弃 RX (与 C 版 spi_send_rec_byte 一致)
			static void transmit(uint8_t *p, uint16_t n, uint32_t t)
			{
				for (uint16_t i = 0; i < n; i++)
				{
					uint8_t tx = p[i];
					uint8_t rx;
					uint8_t buf[1] = {tx};
					transfer(buf, 1, t);
					rx = buf[0];
					(void)rx; // 故意消费 RX, 防止 FIFO 满
				}
			}

			// 主接收 (不控 CS): 委托给 transfer, 用 0xFF 作 dummy 逐字节收
			static void receive(uint8_t *p, uint16_t n, uint32_t t)
			{
				for (uint16_t i = 0; i < n; i++)
				{
					uint8_t buf[1] = {0xFF};
					transfer(buf, 1, t);
					p[i] = buf[0];
				}
			}
		};

		/**
		 * @brief  SPI设备层 (控 CS)
		 * @param  bus_t: SPI 主设备类型 (SPI<...>)
		 * @param  GPIO_CS: 片选引脚 (OutputConfig, 默认拉高)
		 */
		template <typename bus_t, typename GPIO_CS>
		struct SPI_device
		{
			static void init()
			{
				bus_t::init();
				GPIO_CS::init();
				// CS 默认高 (deselect)
				GPIO_CS::set();
			}

			static void select()
			{
				GPIO_CS::clear();
			}

			static void deselect()
			{
				GPIO_CS::set();
			}

			static void transmit(uint8_t *p, uint16_t n, uint32_t t = 0)
			{
				select();
				bus_t::transmit(p, n, t);
				deselect();
			}

			static void receive(uint8_t *p, uint16_t n, uint32_t t = 0)
			{
				select();
				bus_t::receive(p, n, t);
				deselect();
			}

			static void transfer(uint8_t *p, uint16_t n, uint32_t t = 0)
			{
				select();
				bus_t::transfer(p, n, t);
				deselect();
			}

			static void transmit_without_ctl_select(uint8_t *p, uint16_t n, uint32_t t = 0)
			{
				bus_t::transmit(p, n, t);
			}

			static void receive_without_ctl_select(uint8_t *p, uint16_t n, uint32_t t = 0)
			{
				bus_t::receive(p, n, t);
			}

			static void transfer_without_ctl_select(uint8_t *p, uint16_t n, uint32_t t = 0)
			{
				bus_t::transfer(p, n, t);
			}
		};

		// ADC 配置参数
		/**
		* @brief  ADC配置参数
		* @param  ADCx: ADC外设 (ADC0/ADC1/ADC2)
		* @param  prescaler: 时钟预分频 (ADC_ADCCK_PCLK2_DIV2/4/6/8)
		* @param  resolution: 分辨率 (ADC_RESOLUTION_12B/10B/8B/6B)
		* @param  channel_num: 通道数量
		* @param  trigger: 外部触发模式 (EXTERNAL_TRIGGER_DISABLE/T10_CH0/T10_CH1等)
		*/
		template <uint32_t ADCx, uint32_t prescaler = ADC_ADCCK_PCLK2_DIV4,
				  uint32_t resolution = ADC_RESOLUTION_12B,
				  uint32_t channel_num = 1,
				  uint32_t trigger = EXTERNAL_TRIGGER_DISABLE>
		struct ADC_config
		{
			static constexpr uint32_t adc_periph = ADCx;
			static constexpr uint32_t prescaler_val = prescaler;
			static constexpr uint32_t resolution_val = resolution;
			static constexpr uint32_t channel_num_val = channel_num;
			static constexpr uint32_t trigger_val = trigger;
		};

		/**
		* @brief  ADC模数转换器
		* @param  ADC_CONFIG: ADC_config配置模板
		* @note   ADC频率不能超过40MHz
		*/
		template <typename ADC_CONFIG>
		struct ADC
		{
			static void init()
			{
				rcu_periph_clock_enable(RCU_periph<ADC_CONFIG::adc_periph>::periph);
				adc_clock_config(ADC_CONFIG::prescaler_val);
				adc_sync_mode_config(ADC_SYNC_MODE_INDEPENDENT);
				adc_special_function_config(ADC_CONFIG::prescaler_val, ADC_SCAN_MODE, ENABLE);
				adc_data_alignment_config(ADC_CONFIG::adc_periph, ADC_DATAALIGN_RIGHT);
				adc_resolution_config(ADC_CONFIG::adc_periph, ADC_CONFIG::resolution_val);
				adc_channel_length_config(ADC_CONFIG::adc_periph, ADC_ROUTINE_CHANNEL, ADC_CONFIG::channel_num_val);
				adc_external_trigger_config(ADC_CONFIG::adc_periph, ADC_ROUTINE_CHANNEL, ADC_CONFIG::trigger_val);
				adc_enable(ADC_CONFIG::adc_periph);
				adc_calibration_enable(ADC_CONFIG::adc_periph);
			}

			static void set_channel(uint8_t channel, uint8_t sample_time = ADC_SAMPLETIME_144)
			{
				// rank = 0: 写入 RSQ2[4:0] (第 0 次转换的通道号).
				// 用户手册 14.7.10: "单次运行模式下, ADC_RSQ2寄存器的RSQ0[4:0]位规定了ADC的转换通道".
				adc_routine_channel_config(ADC_CONFIG::adc_periph, 0, channel, sample_time);
			}

			static uint16_t get_value()
			{
				adc_software_trigger_enable(ADC_CONFIG::adc_periph, ADC_ROUTINE_CHANNEL);
				while (!adc_flag_get(ADC_CONFIG::adc_periph, ADC_FLAG_EOC));
				uint16_t value = adc_routine_data_read(ADC_CONFIG::adc_periph);
				adc_flag_clear(ADC_CONFIG::adc_periph, ADC_FLAG_EOC); // 清标志, 防止下次 while 立即通过读到旧值
				return value;
			}
		};

		// CRC MODBUS 计算器
		/**
		* @brief  CRC16 MODBUS校验计算器
		* @note   使用MODBUS标准CRC16查表算法
		* @note   初始值: 0xFFFF, 多项式: 0x8005
		* @param  data: 数据指针
		* @param  length: 数据长度
		* @retval CRC16校验值
		*/
		struct CRC16_MODBUS
		{
			static constexpr uint16_t initial_value = 0xFFFF;

			static uint16_t calculate(const uint8_t *data, uint16_t length)
			{
				uint16_t crc = initial_value;
				while (length--)
				{
					crc = (crc >> 8) ^ crc16_table[(crc ^ *data++) & 0xFF];
				}
				return crc;
			}

		private:
			static constexpr uint16_t crc16_table[256] = {
				0x0000, 0xC0C1, 0xC181, 0x0140, 0xC301, 0x03C0, 0x0280, 0xC241,
				0xC601, 0x06C0, 0x0780, 0xC741, 0x0500, 0xC5C1, 0xC481, 0x0440,
				0xCC01, 0x0CC0, 0x0D80, 0xCD41, 0x0F00, 0xCFC1, 0xCE81, 0x0E40,
				0x0A00, 0xCAC1, 0xCB81, 0x0B40, 0xC901, 0x09C0, 0x0880, 0xC841,
				0xD801, 0x18C0, 0x1980, 0xD941, 0x1B00, 0xDBC1, 0xDA81, 0x1A40,
				0x1E00, 0xDEC1, 0xDF81, 0x1F40, 0xDD01, 0x1DC0, 0x1C80, 0xDC41,
				0x1400, 0xD4C1, 0xD581, 0x1540, 0xD701, 0x17C0, 0x1680, 0xD641,
				0xD201, 0x12C0, 0x1380, 0xD341, 0x1100, 0xD1C1, 0xD081, 0x1040,
				0xF001, 0x30C0, 0x3180, 0xF141, 0x3300, 0xF3C1, 0xF281, 0x3240,
				0x3600, 0xF6C1, 0xF781, 0x3740, 0xF501, 0x35C0, 0x3480, 0xF441,
				0x3C00, 0xFCC1, 0xFD81, 0x3D40, 0xFF01, 0x3FC0, 0x3E80, 0xFE41,
				0xFA01, 0x3AC0, 0x3B80, 0xFB41, 0x3900, 0xF9C1, 0xF881, 0x3840,
				0x2800, 0xE8C1, 0xE981, 0x2940, 0xEB01, 0x2BC0, 0x2A80, 0xEA41,
				0xEE01, 0x2EC0, 0x2F80, 0xEF41, 0x2D00, 0xEDC1, 0xEC81, 0x2C40,
				0xE401, 0x24C0, 0x2580, 0xE541, 0x2700, 0xE7C1, 0xE681, 0x2640,
				0x2200, 0xE2C1, 0xE381, 0x2340, 0xE101, 0x21C0, 0x2080, 0xE041,
				0xA001, 0x60C0, 0x6180, 0xA141, 0x6300, 0xA3C1, 0xA281, 0x6240,
				0x6600, 0xA6C1, 0xA781, 0x6740, 0xA501, 0x65C0, 0x6480, 0xA441,
				0x6C00, 0xACC1, 0xAD81, 0x6D40, 0xAF01, 0x6FC0, 0x6E80, 0xAE41,
				0xAA01, 0x6AC0, 0x6B80, 0xAB41, 0x6900, 0xA9C1, 0xA881, 0x6840,
				0x7800, 0xB8C1, 0xB981, 0x7940, 0xBB01, 0x7BC0, 0x7A80, 0xBA41,
				0xBE01, 0x7EC0, 0x7F80, 0xBF41, 0x7D00, 0xBDC1, 0xBC81, 0x7C40,
				0xB401, 0x74C0, 0x7580, 0xB541, 0x7700, 0xB7C1, 0xB681, 0x7640,
				0x7200, 0xB2C1, 0xB381, 0x7340, 0xB101, 0x71C0, 0x7080, 0xB041,
				0x5000, 0x90C1, 0x9181, 0x5140, 0x9301, 0x53C0, 0x5280, 0x9241,
				0x9601, 0x56C0, 0x5780, 0x9741, 0x5500, 0x95C1, 0x9481, 0x5440,
				0x9C01, 0x5CC0, 0x5D80, 0x9D41, 0x5F00, 0x9FC1, 0x9E81, 0x5E40,
				0x5A00, 0x9AC1, 0x9B81, 0x5B40, 0x9901, 0x59C0, 0x5880, 0x9841,
				0x8801, 0x48C0, 0x4980, 0x8941, 0x4B00, 0x8BC1, 0x8A81, 0x4A40,
				0x4E00, 0x8EC1, 0x8F81, 0x4F40, 0x8D01, 0x4DC0, 0x4C80, 0x8C41,
				0x4400, 0x84C1, 0x8581, 0x4540, 0x8701, 0x47C0, 0x4680, 0x8641,
				0x8201, 0x42C0, 0x4380, 0x8341, 0x4100, 0x81C1, 0x8081, 0x4040};
		};

		// RTC 时间数据结构
		struct RTC_Time
		{
			uint8_t year;
			uint8_t month;
			uint8_t date;
			uint8_t week;
			uint8_t hour;
			uint8_t minute;
			uint8_t second;
		};

		// RTC 实时时钟
		/**
		* @brief  RTC实时时钟
		* @note   使用LXTAL 32.768KHz晶振
		* @note   factor_asyn=0x7F, factor_syn=0xFF -> 128Hz计数时钟
		*/
		struct RTC_Device
		{
			static void init()
			{
				rcu_periph_clock_enable(RCU_PMU);
				pmu_backup_write_enable();
				rcu_osci_on(RCU_LXTAL);
				rcu_osci_stab_wait(RCU_LXTAL);
				rcu_rtc_clock_config(RCU_RTCSRC_LXTAL);
				rcu_periph_clock_enable(RCU_RTC);
				rtc_register_sync_wait();
			}

			static void set_time(uint8_t year, uint8_t month, uint8_t date, uint8_t week,
								 uint8_t hour, uint8_t minute, uint8_t second)
			{
				// 字段顺序必须与 rtc_parameter_struct 完全一致:
				//   year, month, date, day_of_week, hour, minute, second,
				//   factor_asyn, factor_syn, am_pm, display_format
				rtc_parameter_struct rtc_initpara;
				rtc_initpara.year         = year;
				rtc_initpara.month        = month;
				rtc_initpara.date         = date;
				rtc_initpara.day_of_week  = week;
				rtc_initpara.hour         = hour;
				rtc_initpara.minute       = minute;
				rtc_initpara.second       = second;
				rtc_initpara.factor_asyn  = 0x7F;  // 32768Hz / (127+1) = 256Hz
				rtc_initpara.factor_syn   = 0xFF;  // 256Hz   / (255+1) = 1Hz (秒脉冲)
				rtc_initpara.am_pm        = RTC_PM;
				rtc_initpara.display_format = RTC_24HOUR;
				rtc_init_mode_enter();
				rtc_init(&rtc_initpara);
				rtc_register_sync_wait();
				rtc_init_mode_exit();
				pmu_backup_write_enable();
			}

			/**
			* @brief  获取当前RTC时间
			* @param  none
			* @retval RTC_Time结构体，包含year,month,date,week,hour,minute,second
			*/
			static RTC_Time get_time()
			{
				rtc_parameter_struct rtc_initpara;
				rtc_current_time_get(&rtc_initpara);
				return RTC_Time{
					rtc_initpara.year,
					rtc_initpara.month,
					rtc_initpara.date,
					rtc_initpara.day_of_week,
					rtc_initpara.hour,
					rtc_initpara.minute,
					rtc_initpara.second};
			}
		};

		// SysTick 延时定时器 (通过 TIMx 模板参数指定定时器)
		/**
		* @brief  SysTick延时定时器
		* @param  TIMx: 定时器外设 (如TIMER5)
		* @note   通过TIMx实现微秒和毫秒延时
		* @note   预分频设置为100ns分辨率
		*/
		template <uint32_t TIMx>
		struct SysTick_Delay
		{
			static void init()
			{
				rcu_periph_clock_enable(RCU_periph<TIMx>::periph);
				uint32_t clk_freq = rcu_clock_freq_get(CK_APB1);
				timer_parameter_struct timer_param = {
					(clk_freq / 10000000) - 1, // prescaler: 100ns resolution
					TIMER_COUNTER_EDGE,
					TIMER_COUNTER_UP,
					TIMER_CKDIV_DIV1,
					65535U,
					0U};
				timer_init(TIMx, &timer_param);
				timer_enable(TIMx);
			}

			static void delay_100ns(uint32_t ns)
			{
				timer_counter_value_config(TIMx, 0);
				while (timer_counter_read(TIMx) < ns * 2)
					;
			}

			static void delay_1us(uint32_t us)
			{
				delay_100ns(10 * us);
			}

			static void delay_1ms(uint32_t ms)
			{
				while (ms--)
				{
					delay_1us(1000);
				}
			}
		};

		/**
		* @brief  通用定时器
		* @param  TIMx: 定时器外设 (TIMER0-TIMER13)
		* @param  clock_frequency: 时钟频率
		* @param  psc_mul: 预分频倍增 (RCU_TIMER_PSC_MUL4)
		*/
		template <uint32_t TIMx, uint32_t clock_frequency, uint32_t psc_mul = RCU_TIMER_PSC_MUL4>
		struct TIM
		{
			static constexpr uint32_t tim_base = TIMx;

			static void init(uint16_t prescaler, uint32_t autoreload)
			{
				rcu_periph_clock_enable(RCU_periph<TIMx>::periph);
				rcu_timer_clock_prescaler_config(psc_mul);

				timer_parameter_struct param = {
					prescaler,
					TIMER_COUNTER_EDGE,
					TIMER_COUNTER_UP,
					TIMER_CKDIV_DIV1,
					autoreload,
					0};
				timer_init(TIMx, &param);
				timer_enable(TIMx);
			}

			static uint32_t get_handle()
			{
				return TIMx;
			}
			static uint32_t get_clock_frequency()
			{
				return clock_frequency;
			}
			static uint32_t get_counter()
			{
				return timer_counter_read(TIMx);
			}
			static void set_counter(uint32_t counter)
			{
				timer_counter_value_config(TIMx, counter);
			}
			static void start()
			{
				timer_enable(TIMx);
			}
			static void stop()
			{
				timer_disable(TIMx);
			}
		};

		/**
		* @brief  PWM脉冲宽度调制
		* @param  TIMtype: 定时器类型 (TIM模板)
		* @param  PWM_CONFIG: PWM_config配置模板
		* @note   频率约束: 1Hz <= frequency_ <= 1000000Hz
		*/
		template <typename TIMtype, typename PWM_CONFIG>
		struct PWM
		{
			static void init()
			{
				TIMtype::init(
					TIMtype::get_clock_frequency() / 1000000.0f - 1,
					1000000.0f / PWM_CONFIG::frequency_val - 1);

				timer_oc_parameter_struct oc_param = {
					TIMER_CCX_ENABLE,
					TIMER_CCXN_DISABLE,
					PWM_CONFIG::polarity_val,
					PWM_CONFIG::npolity_val,
					PWM_CONFIG::idle_state_val,
					PWM_CONFIG::nidle_state_val};
				timer_channel_output_config(TIMtype::get_handle(), PWM_CONFIG::channel_val, &oc_param);
				timer_channel_output_mode_config(TIMtype::get_handle(), PWM_CONFIG::channel_val, PWM_CONFIG::mode_val);
				timer_channel_output_shadow_config(TIMtype::get_handle(), PWM_CONFIG::channel_val, PWM_CONFIG::shadow_val);
			}

			static void set_compare(uint32_t compare)
			{
				timer_channel_output_pulse_value_config(TIMtype::get_handle(), PWM_CONFIG::channel_val, compare);
			}

			static void start()
			{
				timer_channel_output_state_config(TIMtype::get_handle(), PWM_CONFIG::channel_val, TIMER_CCX_ENABLE);
			}

			static void stop()
			{
				timer_channel_output_state_config(TIMtype::get_handle(), PWM_CONFIG::channel_val, 0);
			}
		};

		// PWM 配置参数
		/**
		* @brief  PWM配置参数
		* @param  channel: 通道号 (TIMER_CH_0/TIMER_CH_1/TIMER_CH_2/TIMER_CH_3)
		* @param  frequency: PWM频率 (1-1000000 Hz)
		* @param  polarity: 输出极性 (TIMER_OC_POLARITY_HIGH/TIMLER_OC_POLARITY_LOW)
		* @param  npolarity: 互补通道极性
		* @param  idle_state: 空闲状态
		* @param  nidle_state: 互补通道空闲状态
		* @param  mode: PWM模式 (TIMER_OC_MODE_PWM0/TIMER_OC_MODE_PWM1等)
		* @param  shadow: 输出影子寄存器 (TIMER_OC_SHADOW_DISABLE/ENABLE)
		*/
		template <uint32_t channel, uint32_t frequency,
				  uint32_t polarity = TIMER_OC_POLARITY_HIGH,
				  uint32_t npolarity = TIMER_OCN_POLARITY_HIGH,
				  uint32_t idle_state = TIMER_OC_IDLE_STATE_LOW,
				  uint32_t nidle_state = TIMER_OCN_IDLE_STATE_LOW,
				  uint32_t mode = TIMER_OC_MODE_PWM0,
				  uint32_t shadow = TIMER_OC_SHADOW_DISABLE>
		struct PWM_config
		{
			static constexpr uint32_t channel_val = channel;
			static constexpr uint32_t frequency_val = frequency;
			static constexpr uint32_t polarity_val = polarity;
			static constexpr uint32_t npolarity_val = npolarity;
			static constexpr uint32_t idle_state_val = idle_state;
			static constexpr uint32_t nidle_state_val = nidle_state;
			static constexpr uint32_t mode_val = mode;
			static constexpr uint32_t shadow_val = shadow;
		};
	}
}
#endif // __cplusplus
