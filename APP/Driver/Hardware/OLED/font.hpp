#pragma once

#include <cstdint>

typedef struct
{
	char Index[5];	  // 汉字索引，空间为5字节
	uint8_t Data[32]; // 字模数据
} ChineseCell_t;

// 兼容C++17之前，不在头文件内联变量
extern const std::uint8_t EN8_16[][16];
extern const ChineseCell_t OLED_CF16x16[];