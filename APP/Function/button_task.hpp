#pragma once
#include "button.hpp"
#include "hardware.hpp"
#include "SEGGER_RTT.h"

inline void enter_factory_mode(){
	SEGGER_RTT_WriteString(0, "enter_factory_mode OK\r\n");
}

inline void enter_normal_mode()
{
	SEGGER_RTT_WriteString(0, "enter_normal_mode OK\r\n");
}

inline  void save_to_tfcard()
{
	SEGGER_RTT_WriteString(0, "save_to_tfcard OK\r\n");
}

using key1 = StaticKey<KEY1_GPIO, false, nullptr, enter_factory_mode>;
using key2 = StaticKey<KEY2_GPIO, false, nullptr, enter_normal_mode>;
using key3 = StaticKey<KEY3_GPIO, false, nullptr, save_to_tfcard>;