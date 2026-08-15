#pragma once

#include "ff.h"

#include <stddef.h>

/**
 * 将应用路径转换为 FatFs 路径。
 *
 * 例如 `/sd/log.txt` 转换为 `0:/log.txt`。这是适配层内部接口，普通应用代码
 * 不需要调用。
 *
 * @return 成功返回 0；失败返回 -1 并设置 errno。
 */
int fatfs_path(const char *path, char *output, size_t output_size);

/** 将 FatFs FRESULT 转换为 errno，并统一返回 -1。 */
int fatfs_set_errno(FRESULT result);
