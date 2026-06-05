#pragma once
#ifdef __cplusplus

template <typename motor, typename IN1, typename IN2>
struct TB6612
{
	static void init()
	{
		motor::init();
	}
	static void forward(float speed)
	{
		IN1::set();
		IN2::set();
		motor::set_speed(speed);
	}
	static void backward(float speed)
	{
		IN1::set();
		IN2::clear();
		motor::set_speed(speed);
	}
	static void stop()
	{
		IN1::clear();
		IN2::clear();
		motor::set_speed(0);
	}
	static void brake()
	{
		IN1::set();
		IN2::set();
		motor::set_speed(0);
	}
};

#endif