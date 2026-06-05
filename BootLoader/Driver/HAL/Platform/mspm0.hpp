#pragma once
#if 1
#ifdef __cplusplus
#include <stdint.h>

#include "ti_msp_dl_config.h"

namespace HAL
{
	namespace mspm0
	{
		template <uint32_t GPIOx, uint32_t GPIO_Pin>
		struct GPIO
		/*todo:
		 *代替HAL库实现更好性能
		 *进行编译时检查
		 */
		{
			static void set()
			{
				DL_GPIO_setPins((GPIO_Regs *)GPIOx, GPIO_Pin);
			}
			static void clear()
			{
				DL_GPIO_clearPins((GPIO_Regs *)GPIOx, GPIO_Pin);
			}
			static void toggle()
			{
				DL_GPIO_togglePins((GPIO_Regs *)GPIOx, GPIO_Pin);
			}
			static bool read()
			{
				return (bool)(DL_GPIO_readPins((GPIO_Regs *)GPIOx, GPIO_Pin));
			}
		};
#ifdef GPIOA_BASE
		template <uint32_t GPIO_Pin>
		using PA = GPIO<GPIOA_BASE, GPIO_Pin>;
#endif // GPIOA_BASE
#ifdef GPIOB_BASE
		template <uint32_t GPIO_Pin>
		using PB = GPIO<GPIOB_BASE, GPIO_Pin>;
#endif // GPIOB_BASE

		template <uint32_t tim_base, uint32_t clock_frequency>
		struct TIM
		{
			static uint32_t get_handle()
			{
				return tim_base;
			}
			static uint32_t get_clock_frequency()
			{
				return clock_frequency;
			}
			static uint32_t get_autoreload()
			{
				return DL_Timer_getLoadValue((GPTIMER_Regs *)tim_base);
			}
			static uint32_t get_counter()
			{
				return DL_Timer_getTimerCount((GPTIMER_Regs *)tim_base);
			}
			static void set_prescaler(uint32_t prescaler)
			{
				((GPTIMER_Regs *)tim_base)->COMMONREGS.CPS = prescaler;
			}
			static void set_autoreload(uint32_t autoreload)
			{
				DL_Timer_setLoadValue((GPTIMER_Regs *)tim_base, autoreload);
			}
			static void set_counter(uint32_t counter)
			{
				DL_Timer_setTimerCount((GPTIMER_Regs *)tim_base, counter);
			}
			static void start()
			{
				DL_Timer_startCounter((GPTIMER_Regs *)tim_base);
			}
			static void start_it()
			{
				// HAL_TIM_Base_Start_IT(htim);
			}
		};

		template <typename TIMtype, DL_TIMER_CC_INDEX channel_, uint32_t frequency_>
		struct PWM
		{
			static void set_compare(uint32_t compare)
			{
				// __HAL_TIM_SET_COMPARE(TIMtype::get_handle(), channel_, compare);
				DL_Timer_setCaptureCompareValue((GPTIMER_Regs *)TIMtype::get_handle(), compare, channel_);
			}
			static void set_frequency(uint32_t frequency)
			{
				if (frequency > 0 && frequency <= 1000000)
				{
					/*注意启用预装载*/
					// __HAL_TIM_SET_PRESCALER(TIMtype::get_handle(), (TIMtype::get_clock_frequency() / 1000000.0f) - 1);
					// __HAL_TIM_SET_AUTORELOAD(TIMtype::get_handle(), 1000000.0f / frequency - 1);
					TIMtype::set_prescaler((TIMtype::get_clock_frequency() / 1000000.0f) - 1);
					TIMtype::set_autoreload(1000000.0f / frequency - 1);
				}
			}
			static void start()
			{
				// HAL_TIM_PWM_Start(TIMtype::get_handle(), channel_);
			}
			static void init()
			{
				static_assert(!(frequency_ == 0 || frequency_ > 1000000), "PWM frequency must be in range 1-1000000 Hz");
				TIMtype::set_prescaler(TIMtype::get_clock_frequency() / 1000000.0f - 1);
				TIMtype::set_autoreload(1000000.0f / frequency_ - 1);
				TIMtype::set_counter(0);
				set_compare(0);
				start();
			}
			static DL_TIMER_CC_INDEX get_channel()
			{
				return channel_;
			}
			static uint32_t get_autoreload()
			{
				return TIMtype::get_autoreload();
			}
		};
	}
}
#endif

#endif