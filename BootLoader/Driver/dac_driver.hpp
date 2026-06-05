#pragma once
// DAC0 输出驱动 — PA4

#include "hal.hpp"
#include <cstdint>

inline void dac_init()
{
	rcu_periph_clock_enable(RCU_DAC);
	dac_deinit(DAC0);
	dac_enable(DAC0, DAC_OUT0);
}

/// 设置 DAC0 输出电压 (0~4095)
inline void dac_set(uint16_t value)
{
	if (value > 4095) value = 4095;
	dac_data_set(DAC0, DAC_OUT0, DAC_ALIGN_12B_R, value);
}
