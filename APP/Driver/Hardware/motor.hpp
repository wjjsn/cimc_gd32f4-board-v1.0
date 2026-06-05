#pragma once
#ifdef __cplusplus

template <typename PWM>
struct motor
{
	static void init()
	{
		PWM::init();
	}
	static void set_speed(float speed)
	{
		if (speed < 0 || speed > 100)
			return;
		PWM::set_compare(speed * PWM::get_autoreload() / 100.0f);
	}
};

#endif