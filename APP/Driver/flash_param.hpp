#pragma once
// 外部 SPI Flash (GD25Q) 参数区读写 — 自由函数

#include "hal.hpp"
#include "Hardware/gd25q.hpp"
#include "../Function/hardware.hpp"
#include <cstdint>
#include <cstring>

/// 外部 Flash 参数区起始地址
constexpr uint32_t EXFLASH_PARAM_ADDR = 0x00000000;

using ExtFlash = GD25Q<SPI1_FLASH>;

/// 擦除参数区扇区 (4KB)
inline void flash_param_erase_sector()
{
	ExtFlash::sector_erase(EXFLASH_PARAM_ADDR);
}

/// 写一个字 (32-bit) 到指定偏移
inline void flash_param_write_word(uint32_t offset, uint32_t data)
{
	ExtFlash::page_write(reinterpret_cast<uint8_t *>(&data),
			     EXFLASH_PARAM_ADDR + offset, 4);
}

/// 写入数据 (任意长度, 自动分页)
inline void flash_param_write(uint32_t offset, const void *data, uint32_t len)
{
	ExtFlash::block_write(
		const_cast<uint8_t *>(static_cast<const uint8_t *>(data)),
		EXFLASH_PARAM_ADDR + offset, len);
}

/// 从参数区读取数据
inline void flash_param_read(uint32_t offset, void *data, uint32_t len)
{
	ExtFlash::buffer_read(static_cast<uint8_t *>(data),
			      EXFLASH_PARAM_ADDR + offset, len);
}

/// 保存结构体到参数区 (先擦除再写入)
template <typename T> inline void flash_param_save(const T &param)
{
	flash_param_erase_sector();
	flash_param_write(0, &param, sizeof(T));
}

/// 从参数区加载结构体
template <typename T> inline void flash_param_load(T &param)
{
	flash_param_read(0, &param, sizeof(T));
}
