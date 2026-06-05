#pragma once

#include <cstdint>
#include "bits_operation.hpp"
/*从机地址：0b0110110*/
template <typename i2c_device_7bits>
class AS5600
{
	static const std::uint16_t DEFAULT_TIMEOUT = 1000;
	enum reg : std::uint8_t
	{
		ZMCO   = 0X00, /*展示ZPOS、MPOS被永久写入的次数*/
		ZPOS_H = 0x02,
		ZPOS_L = 0x01, /*起始位置，相当于手动设置零点*/
		MPOS_H = 0x04,
		MPOS_L = 0x03, /*停止位置*/
		MANG_H = 0x06,
		MANG_L = 0x05, /*最大角度*/
		CONF_H = 0x08,
		/*看门狗开关，当角度持续变化不大时，会自动切换为低功耗模式*/
		/*高速滤波器*/
		/*低速滤波器*/
		/*配置PWM的频率*/
		/*配置输出为模拟输出范围为(0%~100%)或(10%~90%)，或为数字输出PWM*/
		/*未避免磁铁不移动，但是数值变动的情况，配置此来屏蔽LSB的变化(1~3位)*/
		/*设置电源模式(轮询间隔)，间隔越短功耗越大*/
		CONF_L		= 0x07,
		RAW_ANGLE_H = 0x0D,
		RAW_ANGLE_L = 0x0C,
		ANGLE_H		= 0x0F,
		ANGLE_L		= 0x0E,
		STATUS		= 0x0B, /*表示检测到磁铁或磁性太强或太弱*/
		AGC			= 0x1A, /*AGC的值会自动变化来补偿由于外接环境变化导致的磁场变化。通过调整磁铁与芯片的距离，将此值控制在128(5V)或64(3.3V)*/
		MAGNITUDE_H = 0x1C,
		MAGNITUDE_L = 0x1B, /*展示内部的CORDIC(坐标旋转数字计算机)的幅度值，作用暂时不清楚*/
		BURN		= 0xFF	/*用于对ZPOS、MPOS、MANG、CONF的永久写入操作。ZPOS、MPOS最多可以写3次，MANG、CONF最多写一次*/
	};
	enum power_mode
	{
		NOM,  /*不省电*/
		LPM1, /*一般省电*/
		LPM2, /*比较省电*/
		LPM3  /*最省电*/
	};
	enum Hysteresis
	{
		DISBALE,
		LSB1,
		LSB2,
		LSB3
	};
	enum output_mode
	{
		ANALOG1, /*GND~VDD(0%~100%)*/
		ANALOG2, /*GND~VDD(10%~90%)*/
		DIGITAL_PWM
	};
	enum PWM_frequency
	{
		_115Hz,
		_230Hz,
		_460Hz,
		_920Hz
	};
	enum slow_filter
	{
		_16x,
		_8x,
		_4x,
		_2x
	};
	enum fast_filter_threshold
	{
		ONLY_SLOW,
		LSB6,
		LSB7,
		LSB9,
		LSB18,
		LSB21,
		LSB24,
		LSB10
	};
	/*不提供对BURN、ZMCO寄存器的操作*/
	std::uint16_t config_register_;
	void set_power_mode(power_mode mode) // 1:0
	{
		switch (mode)
		{
			case NOM:
				BIT::CLR(config_register_, 0);
				BIT::CLR(config_register_, 1);
				break;
			case LPM1:
				BIT::SET(config_register_, 0);
				BIT::CLR(config_register_, 1);
				break;
			case LPM2:
				BIT::CLR(config_register_, 0);
				BIT::SET(config_register_, 1);
				break;
			case LPM3:
				BIT::SET(config_register_, 0);
				BIT::SET(config_register_, 1);
				break;
			default:;
		}
	}
	void set_hysteresis(Hysteresis mode) // 3:2
	{
		switch (mode)
		{
			case DISBALE:
				BIT::CLR(config_register_, 2);
				BIT::CLR(config_register_, 3);
				break;
			case LSB1:
				BIT::SET(config_register_, 2);
				BIT::CLR(config_register_, 3);
				break;
			case LSB2:
				BIT::CLR(config_register_, 2);
				BIT::SET(config_register_, 3);
				break;
			case LSB3:
				BIT::SET(config_register_, 2);
				BIT::SET(config_register_, 3);
				break;
			default:;
		}
	}
	void set_output_mode(output_mode mode) // 5:4
	{
		switch (mode)
		{
			case ANALOG1:
				BIT::CLR(config_register_, 4);
				BIT::CLR(config_register_, 5);
				break;
			case ANALOG2:
				BIT::SET(config_register_, 4);
				BIT::CLR(config_register_, 5);
				break;
			case DIGITAL_PWM:
				BIT::CLR(config_register_, 4);
				BIT::SET(config_register_, 5);
				break;
			default:;
		}
	}
	void set_PWM_frequency(PWM_frequency frequency) // 7:6
	{
		switch (frequency)
		{
			case _115Hz:
				BIT::CLR(config_register_, 6);
				BIT::CLR(config_register_, 7);
				break;
			case _230Hz:
				BIT::SET(config_register_, 6);
				BIT::CLR(config_register_, 7);
				break;
			case _460Hz:
				BIT::CLR(config_register_, 6);
				BIT::SET(config_register_, 7);
				break;
			case _920Hz:
				BIT::SET(config_register_, 6);
				BIT::SET(config_register_, 7);
				break;
			default:;
		}
	}
	void set_slow_filter(slow_filter mode) // 9:8
	{
		switch (mode)
		{
			case _16x:
				BIT::CLR(config_register_, 8);
				BIT::CLR(config_register_, 9);
				break;
			case _8x:
				BIT::SET(config_register_, 8);
				BIT::CLR(config_register_, 9);
				break;
			case _4x:
				BIT::CLR(config_register_, 8);
				BIT::SET(config_register_, 9);
				break;
			case _2x:
				BIT::SET(config_register_, 8);
				BIT::SET(config_register_, 9);
				break;
			default:;
		}
	}
	void set_fast_filter_threshold(fast_filter_threshold thresholds) // 12:10
	{
		switch (thresholds)
		{
			case ONLY_SLOW:
				BIT::CLR(config_register_, 10);
				BIT::CLR(config_register_, 11);
				BIT::CLR(config_register_, 12);
				break;
			case LSB6:
				BIT::SET(config_register_, 10);
				BIT::CLR(config_register_, 11);
				BIT::CLR(config_register_, 12);
				break;
			case LSB7:
				BIT::CLR(config_register_, 10);
				BIT::SET(config_register_, 11);
				BIT::CLR(config_register_, 12);
				break;
			case LSB9:
				BIT::SET(config_register_, 10);
				BIT::SET(config_register_, 11);
				BIT::CLR(config_register_, 12);
				break;
			case LSB18:
				BIT::CLR(config_register_, 10);
				BIT::CLR(config_register_, 11);
				BIT::SET(config_register_, 12);
				break;
			case LSB21:
				BIT::CLR(config_register_, 10);
				BIT::SET(config_register_, 11);
				BIT::CLR(config_register_, 12);
				break;
			case LSB24:
				BIT::SET(config_register_, 10);
				BIT::CLR(config_register_, 11);
				BIT::CLR(config_register_, 12);
				break;
			case LSB10:
				BIT::SET(config_register_, 10);
				BIT::SET(config_register_, 11);
				BIT::SET(config_register_, 12);
				break;
			default:;
		}
	}
	void set_watchdog(bool enable) // 13
	{
		enable ? BIT::SET(config_register_, 13) : BIT::CLR(config_register_, 13);
	}

public:
	static std::uint16_t
	read_CORDIC_magnitude_value()
	{
		std::uint8_t read_buf[2];
		i2c_device_7bits::mem_read(MAGNITUDE_L, i2c_device_7bits::MEMADD_SIZE_8BIT, read_buf, 2, DEFAULT_TIMEOUT);
		return (read_buf[1] << 8) | read_buf[0];
	}
	static std::uint8_t read_AGC()
	{
		std::uint8_t read_buf;
		i2c_device_7bits::mem_read(AGC, i2c_device_7bits::MEMADD_SIZE_8BIT, &read_buf, 1, DEFAULT_TIMEOUT);
		return read_buf;
	}
	static std::uint16_t read_angle_raw()
	{
		std::uint8_t read_buf[2];
		i2c_device_7bits::mem_read(RAW_ANGLE_L, i2c_device_7bits::MEMADD_SIZE_8BIT, read_buf, 2, DEFAULT_TIMEOUT);
	}
	static std::uint16_t read_angle()
	{
		std::uint8_t read_buf[2];
		i2c_device_7bits::mem_read(ANGLE_L, i2c_device_7bits::MEMADD_SIZE_8BIT, read_buf, 2, DEFAULT_TIMEOUT);
	}
	static bool check_status()
	{
		std::uint8_t read_buf;
		i2c_device_7bits::mem_read(STATUS, i2c_device_7bits::MEMADD_SIZE_8BIT, &read_buf, 1, DEFAULT_TIMEOUT);
		return (!BIT::READ(read_buf, 3) /*磁铁太强*/) && (!BIT::READ(read_buf, 4) /*磁铁太弱*/) && BIT::READ(read_buf, 5) /*检测到磁铁*/;
	}
	static void set_range(std::uint16_t start_angle = 0, std::uint16_t stop_angle = 0xFFF, std::uint16_t max_angle = 0xFFF)
	{
		if (start_angle > 0xFFF || stop_angle > 0xFFF || max_angle > 0xFFF)
		{
			return;
		}
		std::uint8_t write_buf[2] = {static_cast<std::uint8_t>(start_angle), static_cast<std::uint8_t>(start_angle >> 8)};
		i2c_device_7bits::mem_write(ZPOS_L, i2c_device_7bits::MEMADD_SIZE_8BIT, write_buf, 2, DEFAULT_TIMEOUT);
		write_buf[0] = static_cast<std::uint8_t>(stop_angle);
		write_buf[1] = static_cast<std::uint8_t>(stop_angle >> 8);
		i2c_device_7bits::mem_write(MPOS_L, i2c_device_7bits::MEMADD_SIZE_8BIT, write_buf, 2, DEFAULT_TIMEOUT);
		write_buf[0] = static_cast<std::uint8_t>(max_angle);
		write_buf[1] = static_cast<std::uint8_t>(max_angle >> 8);
		i2c_device_7bits::mem_write(MANG_L, i2c_device_7bits::MEMADD_SIZE_8BIT, write_buf, 2, DEFAULT_TIMEOUT);
	}
	void conf(power_mode mode = NOM, Hysteresis hyst = DISBALE, output_mode out_mode = ANALOG1, PWM_frequency pwm_freq = _115Hz, slow_filter slow_f = _16x, fast_filter_threshold fast_f = ONLY_SLOW, bool watchdog = false)
	{
		set_power_mode(mode);
		set_hysteresis(hyst);
		set_output_mode(out_mode);
		set_PWM_frequency(pwm_freq);
		set_slow_filter(slow_f);
		set_fast_filter_threshold(fast_f);
		set_watchdog(watchdog);
		std::uint8_t write_buf[2] = {static_cast<std::uint8_t>(config_register_), static_cast<std::uint8_t>(config_register_ >> 8)};
		i2c_device_7bits::mem_write(CONF_L, i2c_device_7bits::MEMADD_SIZE_8BIT, write_buf, 2, DEFAULT_TIMEOUT);
	}
};