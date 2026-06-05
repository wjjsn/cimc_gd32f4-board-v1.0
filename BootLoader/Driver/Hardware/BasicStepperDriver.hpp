#pragma once
#ifdef __cplusplus

#include <stdint.h>
#include <stdlib.h>

template <typename TIMtype_, typename Dirction_, typename Step_, uint16_t Motor_Steps_, uint8_t RPM_, uint8_t microsteps_>
struct BasicStepper
{
	uint16_t remain_Steps_;
	static void init()
	{
		uint16_t frequency = (uint16_t)((float)Motor_Steps_ * (float)RPM_ / 60.0f * 2.0f);
		TIMtype_::set_autoreload(10000.0f / frequency);
		TIMtype_::set_prescaler((float)TIMtype_::get_clock_frequency() / 1000000.0f * 100.0f - 1.0f);
		TIMtype_::set_counter(0);
		TIMtype_::start_it();
	}
	void move(int32_t steps)
	{
		if (steps == 0)
			return;
		steps > 0 ? Dirction_::set() : Dirction_::clear();
		remain_Steps_ += 2 * (uint16_t)labs(steps);
	}
	void IRQ_Handle()
	{
		if (remain_Steps_ > 0)
		{
			Step_::toggle();
			remain_Steps_--;
		}
	}
};
extern "C"
{
#endif

#ifdef __cplusplus
}
#endif