#pragma once
// DAC0 输出驱动 — PA4

#include "gd32f4xx_dac.h"
#include <cstdint>

inline void dac_init()
{
	rcu_periph_clock_enable(RCU_DAC);
	dac_deinit(DAC0);
	// 4. 选择软件触发
	dac_trigger_source_config(DAC0, DAC_OUT0, DAC_TRIGGER_SOFTWARE);
	dac_trigger_enable(DAC0, DAC_OUT0);

	// 5. 关闭波形发生器, 开启输出缓冲 (关闭缓冲可得到更高的输出范围, 但驱动能力下降)
	dac_wave_mode_config(DAC0, DAC_OUT0, DAC_WAVE_DISABLE);
	dac_output_buffer_enable(DAC0, DAC_OUT0);
	dac_enable(DAC0, DAC_OUT0);
}

/// 设置 DAC0 输出电压 (0~4095)
inline void dac_set(uint16_t value)
{
	if (value > 4095)
		value = 4095;
	dac_data_set(DAC0, DAC_OUT0, DAC_ALIGN_12B_R, value);
}
