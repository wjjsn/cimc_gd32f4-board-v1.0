#pragma once

#include <cstdint>

template <typename SPI_Device, typename Delay> class GD30AD3344 {
	std::uint16_t config_register_ = 0;

	std::int16_t transfer_config()
	{
		std::uint8_t data[2] = {
			static_cast<std::uint8_t>(config_register_ >> 8),
			static_cast<std::uint8_t>(config_register_ & 0xFF)
		};
		SPI_Device::transfer(data, 2);
		const std::uint16_t raw =
			(static_cast<std::uint16_t>(data[0]) << 8) | data[1];
		return static_cast<std::int16_t>(raw);
	}

    public:
	enum MUX_options_t : std::uint8_t {
		INP_IN0_INN_IN1 = 0b000,
		INP_IN0_INN_IN3 = 0b001,
		INP_IN1_INN_IN3 = 0b010,
		INP_IN2_INN_IN3 = 0b011,
		INP_IN0_INN_GND = 0b100,
		INP_IN1_INN_GND = 0b101,
		INP_IN2_INN_GND = 0b110,
		INP_IN3_INN_GND = 0b111
	};

	enum PGA_options_t : std::uint8_t {
		_6144V1 = 0b000,
		_4096V1 = 0b001,
		_2048 = 0b010,
		_1024 = 0b011,
		_0512 = 0b100,
		_0256 = 0b101,
		_0064 = 0b110
	};

	enum work_mode_options_t : std::uint8_t {
		CONTINUE_MODE = 0,
		SINGLE_MODE = 1
	};

	enum data_rate_options_t : std::uint8_t {
		SPS6_25 = 0b000,
		SPS12_5 = 0b001,
		SPS25 = 0b010,
		SPS50 = 0b011,
		SPS100 = 0b100,
		SPS250 = 0b101,
		SPS500 = 0b110,
		SPS1000 = 0b111
	};

	enum MISO_PULLUP_options_t : std::uint8_t {
		PULLUP_ENABLE,
		PULLUP_DISABLE
	};

	void init(MUX_options_t mux_option = INP_IN1_INN_GND,
		  PGA_options_t pga_option = _2048,
		  work_mode_options_t work_mode_option = SINGLE_MODE,
		  data_rate_options_t data_rate_option = SPS100,
		  MISO_PULLUP_options_t pullup_option = PULLUP_ENABLE)
	{
		SPI_Device::init();
		Delay::init();
		config_register_ = 0;
		config_MUX(mux_option);
		config_PGA(pga_option);
		config_work_mode(work_mode_option);
		config_data_rate(data_rate_option);
		config_MISO_PULLUP(pullup_option);
		write_config_register();
	}

	void start_single_conversion()
	{
		config_register_ |= std::uint16_t{ 1 } << 15;
		write_config_register();
	}

	std::int16_t read_raw()
	{
		start_single_conversion();
		Delay::delay_1ms(11);
		return transfer_config();
	}

	std::int16_t read_conversion_data()
	{
		return transfer_config();
	}

	void config_MUX(MUX_options_t option)
	{
		config_register_ &= ~(std::uint16_t{ 0b111 } << 12);
		config_register_ |= static_cast<std::uint16_t>(option) << 12;
	}

	void config_PGA(PGA_options_t option)
	{
		config_register_ &= ~(std::uint16_t{ 0b111 } << 9);
		config_register_ |= static_cast<std::uint16_t>(option) << 9;
	}

	void config_work_mode(work_mode_options_t option)
	{
		config_register_ &= ~(std::uint16_t{ 1 } << 8);
		config_register_ |= static_cast<std::uint16_t>(option) << 8;
	}

	void config_data_rate(data_rate_options_t option)
	{
		config_register_ &= ~(std::uint16_t{ 0b111 } << 5);
		config_register_ |= static_cast<std::uint16_t>(option) << 5;
	}

	void config_MISO_PULLUP(MISO_PULLUP_options_t option)
	{
		config_register_ &= ~(std::uint16_t{ 1 } << 3);
		if (option == PULLUP_ENABLE)
			config_register_ |= std::uint16_t{ 1 } << 3;
	}

	std::uint16_t read_config_register()
	{
		std::uint8_t data[4] = {
			static_cast<std::uint8_t>(config_register_ >> 8),
			static_cast<std::uint8_t>(config_register_ & 0xFF),
			static_cast<std::uint8_t>(config_register_ >> 8),
			static_cast<std::uint8_t>(config_register_ & 0xFF)
		};
		SPI_Device::transfer(data, 4);
		return (static_cast<std::uint16_t>(data[2]) << 8) | data[3];
	}

	void write_config_register()
	{
		config_register_ |= std::uint16_t{ 1 } << 1;
		config_register_ &= ~(std::uint16_t{ 1 } << 2);
		(void)transfer_config();
	}
};
