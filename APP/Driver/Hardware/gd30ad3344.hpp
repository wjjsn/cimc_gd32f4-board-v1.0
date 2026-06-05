#pragma once

#include <cstdint>
#include "bits_operation.hpp"

template <typename spi_device>
class GD30AD3344
{
	std::uint16_t config_register;

public:
	enum MUX_options_t
	{
		INP_IN0_INN_IN1,
		INP_IN0_INN_IN3,
		INP_IN1_INN_IN3,
		INP_IN2_INN_IN3,
		INP_IN0_INN_GND,
		INP_IN1_INN_GND,
		INP_IN2_INN_GND,
		INP_IN3_INN_GND
	};

	enum PGA_options_t
	{
		_6144V1,
		_4096V1,
		_2048,
		_1024,
		_0512,
		_0256,
		_0064
	};

	enum work_mode_options_t
	{
		CONTINUE_MODE,
		SINGLE_MODE
	};

	enum data_rate_options_t
	{
		SPS6_25,
		SPS12_5,
		SPS25,
		SPS50,
		SPS100,
		SPS250,
		SPS500,
		SPS1000
	};

	enum MISO_PULLUP_options_t
	{
		PULLUP_ENABLE,
		PULLUP_DISABLE
	};

	void init(MUX_options_t mux_option = INP_IN1_INN_GND, PGA_options_t pga_option = _4096V1, work_mode_options_t work_mode_option = SINGLE_MODE, data_rate_options_t data_rate_option = SPS100, MISO_PULLUP_options_t MISO_PULLUP_option = PULLUP_ENABLE)
	{
		config_MUX(mux_option);
		config_PGA(pga_option);
		config_work_mode(work_mode_option);
		config_data_rate(data_rate_option);
		config_MISO_PULLUP(MISO_PULLUP_option);
		write_config_register();
	}

	void start_single_conversion()
	{
		SET_BIT(config_register, 15);
		write_config_register();
	}

	std::int16_t read_conversion_data(void)
	{
		std::int16_t data;
		spi_device::receive((std::uint8_t *)&data, 2);
		return data;
	}

	void config_MUX(MUX_options_t mux_option)
	{
		switch (mux_option) // 14:12
		{
			case INP_IN0_INN_IN1: // 000
				CLR_BIT(config_register, 12);
				CLR_BIT(config_register, 13);
				CLR_BIT(config_register, 14);
				break;
			case INP_IN0_INN_IN3: // 001
				SET_BIT(config_register, 12);
				CLR_BIT(config_register, 13);
				CLR_BIT(config_register, 14);
				break;
			case INP_IN1_INN_IN3: // 010
				CLR_BIT(config_register, 12);
				SET_BIT(config_register, 13);
				CLR_BIT(config_register, 14);
				break;
			case INP_IN2_INN_IN3: // 011
				SET_BIT(config_register, 12);
				SET_BIT(config_register, 13);
				CLR_BIT(config_register, 14);
				break;
			case INP_IN0_INN_GND: // 100
				CLR_BIT(config_register, 12);
				CLR_BIT(config_register, 13);
				SET_BIT(config_register, 14);
				break;
			case INP_IN1_INN_GND: // 101
				SET_BIT(config_register, 12);
				CLR_BIT(config_register, 13);
				SET_BIT(config_register, 14);
				break;
			case INP_IN2_INN_GND: // 110
				SET_BIT(config_register, 12);
				SET_BIT(config_register, 13);
				CLR_BIT(config_register, 14);
				break;
			case INP_IN3_INN_GND: // 111
				SET_BIT(config_register, 12);
				SET_BIT(config_register, 13);
				SET_BIT(config_register, 14);
				break;
			default:
				break;
		}
	}

	void config_PGA(PGA_options_t pga_option)
	{
		switch (pga_option) // 11:9
		{
			case _6144V1: // 000
				CLR_BIT(config_register, 9);
				CLR_BIT(config_register, 10);
				CLR_BIT(config_register, 11);
				break;
			case _4096V1: // 001
				SET_BIT(config_register, 9);
				CLR_BIT(config_register, 10);
				CLR_BIT(config_register, 11);
				break;
			case _2048: // 010
				CLR_BIT(config_register, 9);
				SET_BIT(config_register, 10);
				CLR_BIT(config_register, 11);
				break;
			case _1024: // 011
				SET_BIT(config_register, 9);
				SET_BIT(config_register, 10);
				CLR_BIT(config_register, 11);
				break;
			case _0512: // 100
				CLR_BIT(config_register, 9);
				CLR_BIT(config_register, 10);
				SET_BIT(config_register, 11);
				break;
			case _0256: // 101
				SET_BIT(config_register, 9);
				CLR_BIT(config_register, 10);
				SET_BIT(config_register, 11);
				break;
			case _0064: // 110
				CLR_BIT(config_register, 9);
				SET_BIT(config_register, 10);
				SET_BIT(config_register, 11);
				break;
			default:
				break;
		}
	}

	void config_work_mode(work_mode_options_t work_mode_option)
	{
		switch (work_mode_option) // 8
		{
			case CONTINUE_MODE:
				CLR_BIT(config_register, 8);
				break;
			case SINGLE_MODE:
				SET_BIT(config_register, 8);
				break;
			default:
				break;
		}
	}

	void config_data_rate(data_rate_options_t data_rate_option)
	{
		switch (data_rate_option) // 7:5
		{
			case SPS6_25: // 000
				CLR_BIT(config_register, 5);
				CLR_BIT(config_register, 6);
				CLR_BIT(config_register, 7);
				break;
			case SPS12_5: // 001
				SET_BIT(config_register, 5);
				CLR_BIT(config_register, 6);
				CLR_BIT(config_register, 7);
				break;
			case SPS25: // 010
				CLR_BIT(config_register, 5);
				SET_BIT(config_register, 6);
				CLR_BIT(config_register, 7);
				break;
			case SPS50: // 011
				SET_BIT(config_register, 5);
				SET_BIT(config_register, 6);
				CLR_BIT(config_register, 7);
				break;
			case SPS100: // 100
				CLR_BIT(config_register, 5);
				CLR_BIT(config_register, 6);
				SET_BIT(config_register, 7);
				break;
			case SPS250: // 101
				SET_BIT(config_register, 5);
				CLR_BIT(config_register, 6);
				SET_BIT(config_register, 7);
				break;
			case SPS500: // 110
				CLR_BIT(config_register, 5);
				SET_BIT(config_register, 6);
				SET_BIT(config_register, 7);
				break;
			case SPS1000: // 111
				SET_BIT(config_register, 5);
				SET_BIT(config_register, 6);
				SET_BIT(config_register, 7);
				break;
			default:
				break;
		}
	}

	void config_MISO_PULLUP(MISO_PULLUP_options_t MISO_PULLUP_option)
	{
		switch (MISO_PULLUP_option)
		{
			case PULLUP_ENABLE:
				SET_BIT(config_register, 3);
				break;
			case PULLUP_DISABLE:
				CLR_BIT(config_register, 3);
				break;
			default:
				break;
		}
	}

	void read_config_register(void)
	{
		uint8_t read_data[4] = {0};
		spi_device::receive(read_data, 4);
		config_register = read_data[2] << 8 | read_data[3];
	}

	void write_config_register(void)
	{
		this->config_register |= 1 << 1;
		this->config_register &= ~(1 << 2);
		spi_device::transmit((std::uint8_t *)&config_register, 2);
	}
};