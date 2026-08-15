#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** SD 卡和文件系统的整体状态。 */
typedef enum {
	/** 尚未调用 sd_storage_init()。 */
	SD_STORAGE_UNINITIALIZED = 0,
	/** SD 卡已初始化并成功挂载 FAT 文件系统，可以读写。 */
	SD_STORAGE_READY,
	/** SD 卡未准备好，常见原因是未插卡、接线错误或初始化超时。 */
	SD_STORAGE_NO_CARD,
	/** SD 卡可以通信，但卡上没有可识别的 FAT12/FAT16/FAT32。 */
	SD_STORAGE_NO_FILESYSTEM,
	/** 发生了上述情况之外的 FatFs 或底层 I/O 错误。 */
	SD_STORAGE_IO_ERROR,
} sd_storage_state_t;

/**
 * sd_readdir() 返回的目录项。
 *
 * date 和 time 是 FAT 原始时间格式。如只需要名字、大小和目录属性，可以忽略它们。
 * attributes 使用 FatFs 的 AM_RDO、AM_HID、AM_SYS、AM_DIR、AM_ARC 位。
 */
typedef struct {
	/** UTF-8 文件名，以 '\0' 结束。 */
	char name[256];
	/** 普通文件大小，单位为字节；目录通常为 0。 */
	uint32_t size;
	/** FAT 日期：年、月、日被压缩在 16 位中。 */
	uint16_t date;
	/** FAT 时间：时、分、2 秒精度的秒被压缩在 16 位中。 */
	uint16_t time;
	/** FatFs AM_* 属性位。用 (attributes & AM_DIR) 判断目录。 */
	uint8_t attributes;
} sd_dirent_t;

/* 目录对象内部包含 FatFs DIR。应用代码只能持有指针，不应访问内部成员。 */
typedef struct sd_dir sd_dir_t;

/** 当前整体状态，主要用于 GDB 观察；业务代码优先调用 sd_storage_is_ready()。 */
extern volatile sd_storage_state_t g_sd_storage_state;
/** 最后一次 f_mount() 的 FRESULT 数值。成功为 FR_OK，即 0。 */
extern volatile int g_sd_storage_error;
/** 最后一次底层 SD 驱动的 sd_error_enum 数值。本驱动 SD_OK 为 29。 */
extern volatile int g_sd_card_error;

/**
 * 初始化 SDIO、SD 卡并立即挂载 FAT 文件系统。
 *
 * 必须在 MCU 时钟和基础外设初始化后调用。重复调用可用于重新尝试挂载。
 *
 * @return 0 表示文件系统可用；-1 表示失败，此时检查三个 g_sd_* 调试变量。
 */
int sd_storage_init(void);

/** @return 文件系统可用时返回 1，否则返回 0。 */
int sd_storage_is_ready(void);

/**
 * 打开目录。
 *
 * @param path `/sd` 开头的绝对路径，或相对当前目录的路径。
 * @return 成功返回目录指针；失败返回 NULL 并设置 errno。
 * @note 成功返回的对象必须交给 sd_closedir()，否则会泄漏堆内存和 FatFs 锁。
 */
sd_dir_t *sd_opendir(const char *path);

/**
 * 读取下一个目录项。
 *
 * @return 1 表示 entry 已填充；0 表示目录结束；-1 表示错误并设置 errno。
 */
int sd_readdir(sd_dir_t *dir, sd_dirent_t *entry);

/** 将目录读取位置恢复到开头。传入 NULL 时什么也不做。 */
void sd_rewinddir(sd_dir_t *dir);

/** @return 成功返回 0；失败返回 -1 并设置 errno。 */
int sd_closedir(sd_dir_t *dir);

/** 自检执行阶段。失败时结合 g_sd_self_test_errno 判断原因。 */
typedef enum {
	SD_SELF_TEST_NOT_RUN = 0,
	SD_SELF_TEST_CREATE_DIR,
	SD_SELF_TEST_OPEN,
	SD_SELF_TEST_WRITE,
	SD_SELF_TEST_SYNC,
	SD_SELF_TEST_READ,
	SD_SELF_TEST_COMPARE,
	SD_SELF_TEST_SEEK,
	SD_SELF_TEST_STAT,
	SD_SELF_TEST_TRUNCATE,
	SD_SELF_TEST_EXTEND,
	SD_SELF_TEST_RANDOM_WRITE,
	SD_SELF_TEST_RANDOM_READ,
	SD_SELF_TEST_APPEND,
	SD_SELF_TEST_RENAME,
	SD_SELF_TEST_DIRECTORY,
	SD_SELF_TEST_CWD,
	SD_SELF_TEST_STDIO,
	SD_SELF_TEST_ERROR_PATHS,
	SD_SELF_TEST_CLEANUP,
	SD_SELF_TEST_PASSED,
	SD_SELF_TEST_FAILED,
} sd_self_test_stage_t;

extern volatile sd_self_test_stage_t g_sd_self_test_stage;
/** 自检失败时保存的 errno；成功为 0。 */
extern volatile int g_sd_self_test_errno;
/** 数据比较失败时，记录第一个不一致字节的偏移；其他错误通常为 0。 */
extern volatile uint32_t g_sd_self_test_offset;
/** 当前测试使用的随机种子；失败后可传给 sd_storage_self_test_seed() 复现。 */
extern volatile uint32_t g_sd_self_test_seed;
/** 当前随机测试轮次，从 0 开始。 */
extern volatile uint32_t g_sd_self_test_round;
/** 当前轮次中的随机操作序号，从 0 开始。 */
extern volatile uint32_t g_sd_self_test_operation;

/**
 * 执行会写卡的完整文件系统自检，并在结束时删除测试文件。
 *
 * 测试路径固定为 /sd/.cimc-test。应先成功调用 sd_storage_init()。
 *
 * @return 全部通过返回 0；失败返回 -1。
 */
int sd_storage_self_test(void);

/**
 * 使用指定种子执行自检，适合复现某一次随机测试。
 *
 * @param seed 随机种子；传入 0 时使用固定的非零默认种子。
 * @return 全部通过返回 0；失败返回 -1。
 */
int sd_storage_self_test_seed(uint32_t seed);

#ifdef __cplusplus
}
#endif
