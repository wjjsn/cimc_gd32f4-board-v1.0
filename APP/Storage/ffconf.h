#pragma once

/*
 * FatFs R0.13b 项目配置。
 *
 * 修改任何选项前先阅读 APP/Storage/README.md 的“FatFs 配置怎么改”。
 * 本文件会被 Meson 复制到构建目录，与子模块中的 ff.c 一起编译，因此不需要、
 * 也不应直接编辑 common/FatFs/source/ffconf.h。
 */
#define FFCONF_DEF 63463

/* 完整读写构建，并保留 stat、目录、删除、重命名和定位等基本功能。 */
#define FF_FS_READONLY 0
#define FF_FS_MINIMIZE 0
#define FF_USE_STRFUNC 0
#define FF_USE_FIND 0
#define FF_USE_MKFS 0
#define FF_USE_FASTSEEK 0
#define FF_USE_EXPAND 0
#define FF_USE_CHMOD 1
#define FF_USE_LABEL 0
#define FF_USE_FORWARD 0

/* API 路径使用 UTF-8，并启用最长 255 字符的长文件名。 */
#define FF_CODE_PAGE 437
#define FF_USE_LFN 1
#define FF_MAX_LFN 255
#define FF_LFN_UNICODE 2
#define FF_LFN_BUF 255
#define FF_SFN_BUF 12
#define FF_STRF_ENCODE 3
#define FF_FS_RPATH 2

/* 只有一个逻辑卷 0:，对应 SD 卡；通用 SD 卡扇区固定为 512 字节。 */
#define FF_VOLUMES 1
#define FF_STR_VOLUME_ID 0
#define FF_MULTI_PARTITION 0
#define FF_MIN_SS 512
#define FF_MAX_SS 512
#define FF_USE_TRIM 0
#define FF_FS_NOFSINFO 0

/*
 * 每个 FIL 保留自己的 512 字节缓存。这样速度和隔离性较好，但增加打开文件数时
 * 会增加 RAM。当前不启用 exFAT，时间戳来自 get_fattime() 读取硬件 RTC。
 */
#define FF_FS_TINY 0
#define FF_FS_EXFAT 0
#define FF_FS_NORTC 0
#define FF_NORTC_MON 1
#define FF_NORTC_MDAY 1
#define FF_NORTC_YEAR 2026
/* 必须不小于应用可能同时打开的文件和目录对象总数。 */
#define FF_FS_LOCK 12
/* 当前为无 RTOS 单线程主循环；引入多任务后不能只把此项改为 1。 */
#define FF_FS_REENTRANT 0
#define FF_FS_TIMEOUT 1000
#define FF_SYNC_t int
