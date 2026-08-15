#pragma once
#include "button.hpp"
#include "hardware.hpp"
#include "SEGGER_RTT.h"
#include "chry_ringbuffer.hpp"
#include "device.hpp"
extern chry_ringbuffer_t ctx_uart1_buffer;
using Uart1RB = Cherry_RingBuffer<&ctx_uart1_buffer, 128>;
extern Device<Uart1RB> g_device;

inline void enter_factory_mode(){
	g_device.params_.use_factory_mode=true;
	g_device.params_save();
	SEGGER_RTT_WriteString(0, "enter_factory_mode OK\r\n");
}

inline void enter_normal_mode()
{
	g_device.params_.use_factory_mode = false;
	g_device.params_save();
	SEGGER_RTT_WriteString(0, "enter_normal_mode OK\r\n");
}

inline  void save_to_tfcard()
{
	SEGGER_RTT_WriteString(0, "save_to_tfcard OK\r\n");
}

using key1 = StaticKey<KEY1_GPIO, false, nullptr, enter_factory_mode>;
using key2 = StaticKey<KEY2_GPIO, false, nullptr, enter_normal_mode>;
using key3 = StaticKey<KEY3_GPIO, false, nullptr, save_to_tfcard>;