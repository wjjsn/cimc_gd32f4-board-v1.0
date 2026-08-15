#include "diskio.h"
#include "sdcard.h"

#include <stdint.h>
#include <string.h>

#define SD_SECTOR_SIZE 512U

/*
 * FatFs 只认识“物理驱动号 + 扇区号”，不知道 SDIO 或 SD 卡命令。
 * 本文件就是这两者之间的适配层。工程只有一张卡，所以合法驱动号只有 0。
 */
static DSTATUS disk_state = STA_NOINIT;
static sd_card_info_struct card_info;

/*
 * 官方 DMA 驱动以 uint32_t 访问内存，缓冲区地址必须 4 字节对齐。FatFs 或用户
 * 传入的 BYTE* 不保证对齐，所以未对齐时通过这个 512 字节缓冲逐扇区中转。
 */
static uint32_t bounce_buffer[SD_SECTOR_SIZE / sizeof(uint32_t)];
volatile int g_sd_card_error = SD_OK;

static sd_error_enum card_initialize(void)
{
	sd_error_enum error = SD_ERROR;
	uint32_t card_status = 0;

	/* DMA 多块传输依靠 SDIO 中断通知数据结束。 */
	nvic_irq_enable(SDIO_IRQn, 2U, 1U);
	/* 上电后的部分卡需要重试，有限重试避免无卡时永久阻塞启动。 */
	for (unsigned int attempt = 0; attempt < 5; ++attempt) {
		error = sd_init();
		if (error == SD_OK)
			break;
	}
	if (error != SD_OK)
		return error;

	error = sd_card_information_get(&card_info);
	if (error == SD_OK)
		error = sd_card_select_deselect(card_info.card_rca);
	if (error == SD_OK)
		error = sd_cardstatus_get(&card_status);
	if (error == SD_OK && (card_status & 0x02000000U) != 0U)
		return SD_LOCK_UNLOCK_FAILED;
	if (error == SD_OK)
		error = sd_bus_mode_config(SDIO_BUSMODE_4BIT);
	if (error == SD_OK)
		error = sd_transfer_mode_config(SD_DMA_MODE);
	g_sd_card_error = error;
	return error;
}

void SDIO_IRQHandler(void)
{
	/* 中断函数只转发，不在中断上下文做文件系统操作。 */
	(void)sd_interrupts_process();
}

DSTATUS disk_initialize(BYTE pdrv)
{
	if (pdrv != 0U)
		return STA_NOINIT;

	if (card_initialize() == SD_OK)
		disk_state = 0;
	else
		disk_state = STA_NOINIT | STA_NODISK;
	return disk_state;
}

DSTATUS disk_status(BYTE pdrv)
{
	return pdrv == 0U ? disk_state : STA_NOINIT;
}

static DRESULT read_aligned(BYTE *buffer, DWORD sector, UINT count)
{
	sd_error_enum error;
	if (count == 1U)
		error = sd_block_read((uint32_t *)buffer, sector, SD_SECTOR_SIZE);
	else
		error = sd_multiblocks_read((uint32_t *)buffer, sector,
					    SD_SECTOR_SIZE, count);
	g_sd_card_error = error;
	return error == SD_OK ? RES_OK : RES_ERROR;
}

DRESULT disk_read(BYTE pdrv, BYTE *buffer, DWORD sector, UINT count)
{
	if (pdrv != 0U || buffer == NULL || count == 0U)
		return RES_PARERR;
	if (disk_state & STA_NOINIT)
		return RES_NOTRDY;

	/* 对齐时可一次 DMA 多扇区读取；未对齐时安全地逐扇区复制。 */
	if (((uintptr_t)buffer & 3U) == 0U)
		return read_aligned(buffer, sector, count);

	for (UINT index = 0; index < count; ++index) {
		if (read_aligned((BYTE *)bounce_buffer, sector + index, 1U) != RES_OK)
			return RES_ERROR;
		memcpy(buffer + index * SD_SECTOR_SIZE, bounce_buffer, SD_SECTOR_SIZE);
	}
	return RES_OK;
}

#if FF_FS_READONLY == 0
static DRESULT write_aligned(const BYTE *buffer, DWORD sector, UINT count)
{
	sd_error_enum error;
	if (count == 1U)
		error = sd_block_write((uint32_t *)buffer, sector, SD_SECTOR_SIZE);
	else
		error = sd_multiblocks_write((uint32_t *)buffer, sector,
					     SD_SECTOR_SIZE, count);
	g_sd_card_error = error;
	return error == SD_OK ? RES_OK : RES_ERROR;
}

DRESULT disk_write(BYTE pdrv, const BYTE *buffer, DWORD sector, UINT count)
{
	if (pdrv != 0U || buffer == NULL || count == 0U)
		return RES_PARERR;
	if (disk_state & STA_NOINIT)
		return RES_NOTRDY;
	if (disk_state & STA_PROTECT)
		return RES_WRPRT;

	/* 对齐判断和读取相同，不能把未对齐地址强转为 uint32_t* 交给 DMA。 */
	if (((uintptr_t)buffer & 3U) == 0U)
		return write_aligned(buffer, sector, count);

	for (UINT index = 0; index < count; ++index) {
		memcpy(bounce_buffer, buffer + index * SD_SECTOR_SIZE, SD_SECTOR_SIZE);
		if (write_aligned((const BYTE *)bounce_buffer, sector + index, 1U) != RES_OK)
			return RES_ERROR;
	}
	return RES_OK;
}
#endif

DRESULT disk_ioctl(BYTE pdrv, BYTE command, void *buffer)
{
	if (pdrv != 0U)
		return RES_PARERR;
	if (disk_state & STA_NOINIT)
		return RES_NOTRDY;

	switch (command) {
	case CTRL_SYNC:
		/* SD 驱动的写函数返回前已等待卡退出 PROGRAMMING 状态。 */
		return RES_OK;
	case GET_SECTOR_COUNT:
		if (buffer == NULL)
			return RES_PARERR;
		/* sd_card_capacity_get() 返回 KiB；每 KiB 含两个 512 字节扇区。 */
		*(DWORD *)buffer = sd_card_capacity_get() * 2U;
		return RES_OK;
	case GET_SECTOR_SIZE:
		if (buffer == NULL)
			return RES_PARERR;
		*(WORD *)buffer = SD_SECTOR_SIZE;
		return RES_OK;
	case GET_BLOCK_SIZE:
		if (buffer == NULL)
			return RES_PARERR;
		*(DWORD *)buffer = 1U;
		return RES_OK;
	default:
		return RES_PARERR;
	}
}
