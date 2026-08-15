#include "sd_storage.h"

#include "ff.h"
#include "gd32f4xx_rtc.h"
#include "posix_fs.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

volatile sd_storage_state_t g_sd_storage_state = SD_STORAGE_UNINITIALIZED;
volatile int g_sd_storage_error = FR_NOT_READY;

static FATFS filesystem;

/* 对外隐藏 FatFs 的 DIR，避免与某些 C 库的 DIR 类型冲突。 */
struct sd_dir {
	DIR fat_dir;
};

static uint8_t bcd_to_binary(uint8_t value)
{
	return (uint8_t)(((value >> 4) * 10U) + (value & 0x0FU));
}

DWORD get_fattime(void)
{
	/* GD32 RTC 字段使用 BCD；FatFs 需要 FAT 规范定义的位压缩时间。 */
	rtc_parameter_struct now;
	rtc_current_time_get(&now);
	uint32_t year = 2000U + bcd_to_binary(now.year);
	if (year < 1980U || year > 2107U)
		year = 2026U;
	return ((year - 1980U) << 25) |
	       ((uint32_t)bcd_to_binary(now.month) << 21) |
	       ((uint32_t)bcd_to_binary(now.date) << 16) |
	       ((uint32_t)bcd_to_binary(now.hour) << 11) |
	       ((uint32_t)bcd_to_binary(now.minute) << 5) |
	       ((uint32_t)bcd_to_binary(now.second) >> 1);
}

int sd_storage_init(void)
{
	/* opt=1 表示现在就初始化物理卡并识别文件系统，而不是延迟到首次 I/O。 */
	FRESULT result = f_mount(&filesystem, "0:", 1);
	g_sd_storage_error = result;
	if (result == FR_OK) {
		g_sd_storage_state = SD_STORAGE_READY;
		return 0;
	}
	if (result == FR_NO_FILESYSTEM)
		g_sd_storage_state = SD_STORAGE_NO_FILESYSTEM;
	else if (result == FR_NOT_READY)
		g_sd_storage_state = SD_STORAGE_NO_CARD;
	else
		g_sd_storage_state = SD_STORAGE_IO_ERROR;
	return -1;
}

int sd_storage_is_ready(void)
{
	return g_sd_storage_state == SD_STORAGE_READY;
}

sd_dir_t *sd_opendir(const char *path)
{
	if (path == NULL) {
		errno = EINVAL;
		return NULL;
	}
	char converted[260];
	if (fatfs_path(path, converted, sizeof(converted)) < 0)
		return NULL;
	/* 目录对象生命周期由调用者控制：成功后必须 sd_closedir()。 */
	sd_dir_t *dir = malloc(sizeof(*dir));
	if (dir == NULL) {
		errno = ENOMEM;
		return NULL;
	}
	FRESULT result = f_opendir(&dir->fat_dir, converted);
	if (result != FR_OK) {
		free(dir);
		fatfs_set_errno(result);
		return NULL;
	}
	return dir;
}

int sd_readdir(sd_dir_t *dir, sd_dirent_t *entry)
{
	if (dir == NULL || entry == NULL) {
		errno = EINVAL;
		return -1;
	}
	FILINFO info;
	FRESULT result = f_readdir(&dir->fat_dir, &info);
	if (result != FR_OK) {
		errno = EIO;
		return -1;
	}
	/* FatFs 用空文件名表示已经读到目录末尾。 */
	if (info.fname[0] == '\0')
		return 0;
	strncpy(entry->name, info.fname, sizeof(entry->name) - 1U);
	entry->name[sizeof(entry->name) - 1U] = '\0';
	entry->size = info.fsize;
	entry->date = info.fdate;
	entry->time = info.ftime;
	entry->attributes = info.fattrib;
	return 1;
}

void sd_rewinddir(sd_dir_t *dir)
{
	if (dir != NULL)
		(void)f_readdir(&dir->fat_dir, NULL);
}

int sd_closedir(sd_dir_t *dir)
{
	if (dir == NULL) {
		errno = EINVAL;
		return -1;
	}
	FRESULT result = f_closedir(&dir->fat_dir);
	free(dir);
	if (result != FR_OK) {
		errno = EIO;
		return -1;
	}
	return 0;
}
