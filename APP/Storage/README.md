# SD 卡、FatFs 与 C 文件 API 使用指南

本文档面向第一次接触裸机文件系统、FatFs 或 POSIX 文件 API 的开发者。
看完后应能完成以下工作：

- 理解一次 `write()` 最终如何变成 SDIO 总线上的 SD 卡写操作。
- 使用 `open/read/write/close` 或 `fopen/fread/fwrite/fclose` 读写文件。
- 创建、删除、重命名文件和目录。
- 遍历目录。
- 修改 SDIO 引脚、总线宽度、DMA、文件数量和 FatFs 功能。
- 根据 `errno`、FatFs 错误和 SD 驱动错误定位故障。

## 1. 模块结构

文件系统分为四层。调用方向从上到下，错误返回方向从下到上：

```text
应用程序
  open / read / write / fopen / mkdir / stat / ...
             |
             v
APP/Storage/posix_fs.c
  把 C/POSIX API 转换为 FatFs API
  把 /sd/test.txt 转换为 0:/test.txt
             |
             v
common/FatFs/source/ff.c
  解析 FAT12/FAT16/FAT32 文件系统、目录和文件簇链
             |
             v
APP/Storage/diskio.c
  把扇区读写转换为 SD 卡块读写
             |
             v
APP/Storage/sdcard.c
  GD32F470 SDIO + DMA1 Channel3 硬件驱动
```

各文件职责如下：

| 文件 | 作用 | 通常什么时候修改 |
|---|---|---|
| `sdcard.c/.h` | 官方 SDIO 卡协议和 DMA 驱动 | 换芯片、换 SDIO 引脚、换 DMA、调时钟 |
| `diskio.c` | FatFs 要求的块设备接口 | 换存储介质、加卡检测/写保护、改扇区策略 |
| `ffconf.h` | FatFs 编译配置 | 开关长文件名、exFAT、并发锁、格式化功能 |
| `sd_storage.c/.h` | 挂载、RTC 时间、目录遍历、调试状态 | 改挂载策略、时间来源、目录 API |
| `posix_fs.c/.h` | `open()` 等 C API 适配 | 增加 API、改路径前缀、改最大文件数 |
| `sd_self_test.c` | 实机读写自检 | 增加测试项或关闭启动测试 |
| `meson.build` | 将 FatFs 和适配代码编入 APP | 增删源文件、替换 FatFs 版本 |

FatFs 上游源码位于 Git 子模块 `common/FatFs`，固定到 `ff13b`。不要直接
修改子模块里的 `ff.c` 或 `ff.h`；项目配置放在 `APP/Storage/ffconf.h`。

## 2. 当前硬件连接

当前使用 GD32F470 的标准 4-bit SDIO 接口：

| MCU 引脚 | SD 卡信号 | GPIO 配置 |
|---|---|---|
| PC8 | DAT0 | AF12，上拉，25 MHz |
| PC9 | DAT1 | AF12，上拉，25 MHz |
| PC10 | DAT2 | AF12，上拉，25 MHz |
| PC11 | DAT3 | AF12，上拉，25 MHz |
| PC12 | CLK | AF12，无上下拉，25 MHz |
| PD2 | CMD | AF12，上拉，25 MHz |

数据传输使用 `DMA1 Channel3 / Subperipheral4`。SDIO 中断由
`diskio.c::SDIO_IRQHandler()` 转发给 `sd_interrupts_process()`。

### 2.1 更换 SDIO 引脚

引脚初始化位于 `sdcard.c` 末尾的 `gpio_config()`。例如当前代码中：

```c
gpio_af_set(GPIOC, GPIO_AF_12,
            GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 |
            GPIO_PIN_11 | GPIO_PIN_12);
gpio_af_set(GPIOD, GPIO_AF_12, GPIO_PIN_2);
```

更换引脚时必须同时确认：

1. 芯片数据手册中该引脚确实支持 SDIO 复用功能。
2. `gpio_af_set()` 的端口、复用号和引脚掩码正确。
3. `gpio_mode_set()` 和 `gpio_output_options_set()` 使用相同引脚。
4. `rcu_config()` 已使能新 GPIO 端口时钟。
5. PCB 上 DAT0~DAT3 和 CMD 有可靠上拉。仅依赖 MCU 内部上拉可能不适合高速或长走线。

### 2.2 改成 1-bit SDIO

如果硬件只连接 DAT0、CLK、CMD：

1. 在 `sdcard.c::gpio_config()` 中移除 PC9、PC10、PC11。
2. 在 `diskio.c::card_initialize()` 中将：

```c
sd_bus_mode_config(SDIO_BUSMODE_4BIT);
```

改为：

```c
sd_bus_mode_config(SDIO_BUSMODE_1BIT);
```

1-bit 模式速度较低，但接线更少。不要在没有连接 DAT1~DAT3 时启用 4-bit 模式。

### 2.3 增加 Card Detect 和 Write Protect

当前板级资料未明确卡检测 CD 和写保护 WP 引脚，所以代码通过 SDIO 初始化结果判断
卡是否存在，并不主动检测机械开关。如果卡座接出了这些信号，可在 `diskio.c` 增加：

- `card_detected()`：读取 CD GPIO。
- `card_write_protected()`：读取 WP GPIO。
- `disk_status()`：无卡返回 `STA_NODISK | STA_NOINIT`，写保护返回 `STA_PROTECT`。
- `disk_write()`：写保护时返回 `RES_WRPRT`。

注意先确认有效电平。不同卡座的机械开关可能是插卡接地，也可能是插卡断开。

## 3. 初始化和启动流程

APP 在 `Function/main.cpp` 中执行：

```c
device_init_all();
if (sd_storage_init() == 0)
    (void)sd_storage_self_test();
```

`sd_storage_init()` 内部调用 `f_mount(&filesystem, "0:", 1)`。参数 `1` 表示立即挂载，
因此调用返回成功时，SD 卡初始化、分区识别和 FAT 文件系统识别均已完成。

返回值约定：

| 返回值 | 含义 |
|---|---|
| `0` | SD 卡和 FAT 文件系统可用 |
| `-1` | 初始化或挂载失败，查看全局调试变量 |

推荐业务代码先检查：

```c
if (!sd_storage_is_ready()) {
    /* 不要继续创建文件；此处可以报警、显示错误或稍后重试。 */
    return;
}
```

### 3.1 生产固件是否保留启动自检

自检会在 SD 卡上短暂创建 `/sd/.cimc-test`，测试完成后删除。它经过实机验证，
但每次开机都会产生若干写入。生产环境若不希望每次启动写卡，在 `main.cpp` 中改为：

```c
device_init_all();
(void)sd_storage_init();
```

需要维护或产测时，再通过命令或调试器显式调用：

```c
int result = sd_storage_self_test();
```

## 4. 路径规则

应用层统一使用 `/sd` 作为 SD 卡根目录：

| 应用路径 | FatFs 内部路径 | 含义 |
|---|---|---|
| `/sd` | `0:/` | SD 卡根目录 |
| `/sd/test.txt` | `0:/test.txt` | 根目录文件 |
| `/sd/log/1.txt` | `0:/log/1.txt` | 子目录文件 |
| `log/1.txt` | `log/1.txt` | 相对当前目录 |

其他绝对路径，例如 `/flash/a.txt`，会返回 `ENOENT`。路径转换在
`posix_fs.c::fatfs_path()` 中完成。如果需要把前缀改为 `/mnt/sd`，应修改该函数，
并同步修改文档和所有硬编码测试路径。

文件名采用 UTF-8，支持最长 255 个字符的 FatFs 长文件名。完整路径缓冲上限由
`SD_PATH_MAX` 控制，当前为 260 字节。UTF-8 中文字符通常占 3 字节，因此“字符数”
和“字节数”不是一回事。

## 5. 两套文件 API 怎么选

工程支持两套常见接口。

### 5.1 文件描述符 API

主要函数是 `open/read/write/lseek/fsync/close`。它更接近操作系统接口，返回一个整数
文件描述符 `fd`。适合二进制数据、协议日志和需要精确控制同步的场景。

必须包含：

```c
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
```

完整写入示例：

```c
int save_text(void)
{
    static const char text[] = "hello SD card\n";
    int fd = open("/sd/example.txt", O_CREAT | O_TRUNC | O_WRONLY, 0666);
    if (fd < 0) {
        /* errno 表示失败原因，例如 ENOENT、ENODEV、EIO。 */
        return -1;
    }

    ssize_t written = write(fd, text, sizeof(text) - 1U);
    if (written != (ssize_t)(sizeof(text) - 1U)) {
        int saved_errno = errno;
        (void)close(fd);
        errno = saved_errno;
        return -1;
    }

    /* fsync 将 FatFs 缓存和目录项立即写回卡中。 */
    if (fsync(fd) < 0) {
        int saved_errno = errno;
        (void)close(fd);
        errno = saved_errno;
        return -1;
    }

    return close(fd);
}
```

完整读取示例：

```c
int load_text(char *buffer, size_t capacity)
{
    if (buffer == NULL || capacity == 0U) {
        errno = EINVAL;
        return -1;
    }

    int fd = open("/sd/example.txt", O_RDONLY);
    if (fd < 0)
        return -1;

    /* 留一个字节存放字符串结束符 '\0'。 */
    ssize_t count = read(fd, buffer, capacity - 1U);
    int saved_errno = errno;
    (void)close(fd);
    errno = saved_errno;

    if (count < 0)
        return -1;
    buffer[count] = '\0';
    return (int)count;
}
```

### 5.2 stdio API

主要函数是 `fopen/fread/fwrite/fseek/fflush/fclose`。它使用 `FILE *`，适合熟悉标准 C
库的开发者和文本文件。必须包含 `<stdio.h>`。

```c
int append_log(float temperature)
{
    FILE *file = fopen("/sd/log.txt", "a");
    if (file == NULL)
        return -1;

    if (fprintf(file, "temperature=%.2f\n", (double)temperature) < 0) {
        (void)fclose(file);
        return -1;
    }

    /* fflush 类似 fsync，但作用于 FILE 流。 */
    if (fflush(file) != 0) {
        (void)fclose(file);
        return -1;
    }
    return fclose(file);
}
```

不要对同一个底层文件同时混用一个 `fd` 和一个 `FILE *`。如果使用 `fopen()` 打开，
就用 `fclose()` 关闭；如果使用 `open()` 打开，就用 `close()` 关闭。

## 6. `open()` 参数说明

`open(path, flags, mode)` 的第二个参数由多个标志按位或组成：

| 标志 | 含义 |
|---|---|
| `O_RDONLY` | 只读 |
| `O_WRONLY` | 只写 |
| `O_RDWR` | 可读可写 |
| `O_CREAT` | 文件不存在时创建 |
| `O_TRUNC` | 打开时把原文件清空为 0 字节 |
| `O_EXCL` | 与 `O_CREAT` 同用，文件已存在时失败 |
| `O_APPEND` | 每次写入前移动到文件末尾 |

常见组合：

```c
open(path, O_RDONLY);                         /* 读取已有文件 */
open(path, O_CREAT | O_TRUNC | O_WRONLY, 0666); /* 新建或覆盖 */
open(path, O_CREAT | O_APPEND | O_WRONLY, 0666);/* 追加日志 */
open(path, O_CREAT | O_EXCL | O_WRONLY, 0666);  /* 只允许新建 */
open(path, O_CREAT | O_RDWR, 0666);             /* 读写，不清空 */
```

第三个参数 `0666` 是 POSIX 权限写法。FAT 文件系统没有 Linux 那样完整的用户、组和
其他人权限，当前实现主要把“是否有任意写权限”映射为 FAT 的只读属性。因此一般统一
传 `0666` 即可。

## 7. 返回值和错误处理

多数 POSIX API 成功返回非负值，失败返回 `-1` 并设置全局变量 `errno`：

```c
int fd = open("/sd/missing.txt", O_RDONLY);
if (fd < 0) {
    switch (errno) {
    case ENOENT:
        /* 文件或目录不存在。 */
        break;
    case ENODEV:
        /* SD 卡没有准备好，可能未插卡或初始化失败。 */
        break;
    case EROFS:
        /* 介质只读或写保护。 */
        break;
    case EIO:
        /* 底层 SDIO、DMA 或文件系统 I/O 错误。 */
        break;
    default:
        break;
    }
}
```

常见错误：

| errno | 含义 | 常见原因 |
|---|---|---|
| `ENOENT` | 文件或目录不存在 | 路径写错、父目录未创建 |
| `EEXIST` | 对象已经存在 | `O_EXCL` 新建已有文件 |
| `EACCES` | 没有访问权限 | 文件只读、错误的打开方式 |
| `EBADF` | 文件描述符无效 | 重复关闭、使用未成功打开的 fd |
| `EMFILE` | 文件描述符用完 | 同时打开超过 8 个文件 |
| `ENODEV` | 设备未准备好 | 未插卡、接线错误、初始化失败 |
| `ENXIO` | 没有 FAT 文件系统 | 卡未格式化为 FAT12/16/32 |
| `EROFS` | 只读文件系统 | SD 卡写保护或 FatFs 返回写保护 |
| `ENAMETOOLONG` | 路径过长 | 超过 `SD_PATH_MAX` |
| `EIO` | I/O 错误 | SDIO CRC、超时、DMA、卡损坏 |

注意：`read()` 成功返回 `0` 表示已经到文件末尾，不是错误。

## 8. 文件位置与大小

### 8.1 `lseek()`

```c
lseek(fd, 0, SEEK_SET);   /* 移动到文件开头 */
lseek(fd, 10, SEEK_SET);  /* 移动到第 10 字节 */
lseek(fd, -4, SEEK_CUR);  /* 从当前位置向前 4 字节 */
lseek(fd, 0, SEEK_END);   /* 移动到文件末尾 */
```

返回值是移动后的绝对位置，失败返回 `(off_t)-1`。

### 8.2 `stat()` 和 `fstat()`

```c
struct stat info;
if (stat("/sd/example.txt", &info) == 0) {
    long size = (long)info.st_size;
    int is_directory = S_ISDIR(info.st_mode);
    int is_file = S_ISREG(info.st_mode);
}
```

`stat()` 根据路径查询，`fstat()` 根据已打开的文件描述符查询。

### 8.3 `truncate()` 和 `ftruncate()`

```c
truncate("/sd/example.bin", 1024); /* 按路径调整为 1024 字节 */
ftruncate(fd, 512);                 /* 调整已打开文件 */
```

FatFs R0.13b 在当前未启用 exFAT 的配置下使用 32 位文件大小，单文件最大接近 4 GiB。

## 9. 目录和文件管理

### 9.1 创建、删除和重命名

```c
mkdir("/sd/log", 0777);
rename("/sd/log/old.txt", "/sd/log/new.txt");
unlink("/sd/log/new.txt"); /* 删除文件 */
rmdir("/sd/log");          /* 只能删除空目录 */
```

FatFs 不会自动创建中间目录。要创建 `/sd/a/b/file.txt`，必须先创建 `/sd/a`，再创建
`/sd/a/b`。

### 9.2 当前目录

```c
char cwd[260];
chdir("/sd/log");
getcwd(cwd, sizeof(cwd)); /* 得到 /sd/log */

int fd = open("today.txt", O_CREAT | O_WRONLY, 0666);
```

裸机程序通常更推荐使用绝对路径，避免不同模块修改当前目录后互相影响。

### 9.3 遍历目录

该目标的 picolibc `<dirent.h>` 明确不支持裸机目录接口，而且 FatFs 自身也定义了
名为 `DIR` 的类型。因此项目提供独立的 `sd_*dir` API：

```c
#include "sd_storage.h"
#include <stdio.h>

void list_directory(const char *path)
{
    sd_dir_t *dir = sd_opendir(path);
    if (dir == NULL)
        return;

    sd_dirent_t entry;
    int result;
    while ((result = sd_readdir(dir, &entry)) > 0) {
        printf("name=%s size=%lu attributes=0x%02x\n",
               entry.name,
               (unsigned long)entry.size,
               entry.attributes);
    }

    /* result == 0 表示正常结束；result == -1 表示错误。 */
    (void)sd_closedir(dir);
}
```

`sd_readdir()` 返回值：

| 返回值 | 含义 |
|---|---|
| `1` | 成功读取一个目录项，`entry` 有效 |
| `0` | 已到目录末尾 |
| `-1` | 读取失败，查看 `errno` |

`entry.attributes` 使用 FatFs 属性位：

| 属性 | 含义 |
|---|---|
| `AM_RDO` | 只读 |
| `AM_HID` | 隐藏 |
| `AM_SYS` | 系统文件 |
| `AM_DIR` | 目录 |
| `AM_ARC` | 归档 |

判断是否为目录：

```c
if ((entry.attributes & AM_DIR) != 0U) {
    /* 这是目录。 */
}
```

## 10. 同时打开文件数量

`posix_fs.c` 中：

```c
#define SD_MAX_OPEN_FILES 8
```

表示文件描述符层最多同时打开 8 个 SD 文件。文件描述符 0、1、2 保留给标准输入、
标准输出和标准错误，SD 文件从 3 开始。

FatFs 的锁数量在 `ffconf.h` 中：

```c
#define FF_FS_LOCK 12
```

如果要把同时打开文件数改成 12，至少要同时修改：

```c
/* posix_fs.c */
#define SD_MAX_OPEN_FILES 12

/* ffconf.h，必须不小于文件和目录可能同时打开的总数 */
#define FF_FS_LOCK 16
```

每个 `FIL` 在当前 `FF_FS_TINY=0` 配置下自带 512 字节缓存。增加文件数量会明显增加
RAM 占用。修改后必须查看链接器输出中的 RAM 使用量。

## 11. FatFs 配置怎么改

项目配置位于 `APP/Storage/ffconf.h`。

### 11.1 只读模式

```c
#define FF_FS_READONLY 1
```

会从 FatFs 中移除写入、删除、重命名等功能。本项目的 POSIX 写接口仍会参与编译，
所以若真正改为只读，还应在 `posix_fs.c` 中禁用或调整写相关实现。

### 11.2 长文件名

当前配置：

```c
#define FF_USE_LFN 1
#define FF_MAX_LFN 255
#define FF_LFN_UNICODE 2
```

- `FF_USE_LFN=1`：使用 FatFs 内部静态长文件名工作区，简单但不支持多线程并发。
- `FF_LFN_UNICODE=2`：API 路径使用 UTF-8。
- `FF_MAX_LFN=255`：最长 255 个 UTF-16 代码单元。

本工程是单线程主循环，`FF_USE_LFN=1` 合适。如果以后加入 RTOS 并从多个任务访问
文件系统，需要重新评估 `FF_USE_LFN` 和 `FF_FS_REENTRANT`。

### 11.3 exFAT

当前：

```c
#define FF_FS_EXFAT 0
```

因此支持 FAT12/FAT16/FAT32，不支持 exFAT。启用 exFAT 会增加 Flash、RAM 和长文件名
工作区，并涉及 exFAT 的授权历史问题。修改前应确认产品需求、FatFs 版本许可和内存。

### 11.4 自动格式化

当前：

```c
#define FF_USE_MKFS 0
```

固件不会格式化 SD 卡，避免误删用户数据。若要支持 `f_mkfs()`，将其改为 `1`，编写
明确的用户确认流程，并实现可靠的工作缓冲。不要在挂载失败时自动格式化。

### 11.5 RTC 时间戳

当前 `FF_FS_NORTC=0`，FatFs 调用 `sd_storage.c::get_fattime()` 读取 GD32 RTC。
RTC 年份按 2000~2099 解释，FAT 可表示 1980~2107 年，秒精度为 2 秒。

若没有 RTC，可改为：

```c
#define FF_FS_NORTC 1
#define FF_NORTC_YEAR 2026
#define FF_NORTC_MON 1
#define FF_NORTC_MDAY 1
```

这样所有新文件使用固定日期。

## 12. 修改 SDIO 速度和 DMA

SDIO 初始化和传输时钟分频位于 `sdcard.c`：

```c
#define SD_CLK_DIV_INIT  0x0076
#define SD_CLK_DIV_TRANS 0x0002
```

初始化阶段必须低速。传输阶段调高速度前，应确认：

- `SystemCoreClock` 和 SDIO 输入时钟。
- SD 卡规格允许的频率。
- PCB 走线、上拉电阻和信号完整性。
- 多种品牌和容量的卡都能稳定通过长时间写读测试。

DMA 映射在 `dma_transfer_config()` 和 `dma_receive_config()`：

```c
DMA1, DMA_CH3, DMA_SUBPERI4
```

换 DMA 时，要同步修改所有 `dma_flag_get()`、`dma_flag_clear()`、通道初始化和
subperipheral 选择，并检查是否与 USART、ADC 等外设冲突。

## 13. 自检说明

`sd_storage_self_test()` 创建 `/sd/.cimc-test`，依次测试：

1. 创建目录。
2. 通过 `open()` 创建文件。
3. 将文件扩展为 8193 字节，确认新区域全部读取为零。
4. 使用 4 个派生随机种子，每轮执行 48 次随机偏移、随机长度覆盖写。
5. 写入长度覆盖 1~521 字节，刻意包含非 4 字节对齐和跨 512 字节扇区操作。
6. 周期性 `fsync()` 后随机抽样读回，并与 RAM 中的参考模型逐字节比较。
7. 关闭并重新打开文件，再执行每轮 48 次随机定位读取和完整文件校验。
8. 验证 `O_APPEND` 在手动 `lseek()` 后仍然只在文件末尾追加，并读取末尾内容核对顺序。
9. 截断到 4099 字节，再扩展到 6147 字节，验证大小和扩展区域补零。
10. 验证不存在文件、`O_EXCL`、非法 fd、只写文件读取、只读文件写入/截断、负偏移等错误路径和 `errno`。
11. 重命名文件，通过 `access()` 验证新旧路径。
12. 遍历目录，检查文件名、大小、唯一性，并验证 `sd_rewinddir()`。
13. 切换当前目录，使用相对路径创建、写入和删除文件，再检查 `getcwd()`。
14. 使用 stdio 再完成一次写、刷盘、定位和读取。
15. 删除全部测试文件和目录。

`sd_storage_self_test()` 每次调用会改变基础种子。若某次失败，记录
`g_sd_self_test_seed`，然后在 GDB 中调用下面的接口即可精确复现：

```gdb
print sd_storage_self_test_seed(g_sd_self_test_seed)
```

自检的错误路径依赖 picolibc 的 `errno`。picolibc 将 `errno` 实现为 TLS 变量，
所以 `APP/app_flash.ld` 必须为 `.tbss` 分配实际 RAM，`picolibc_tls.c` 必须提供
单线程裸机的 ARM TLS 指针。删除其中任一项都可能使 `errno` 与普通 `.bss` 重叠，
表现为随机错误码或内存破坏。

返回值：

| 返回值 | 含义 |
|---|---|
| `0` | 全部测试通过 |
| `-1` | 某一步失败，查看调试变量 |

## 14. 调试变量

这些变量声明为 `volatile`，便于 GDB 在优化构建中观察：

| 变量 | 含义 |
|---|---|
| `g_sd_storage_state` | 当前挂载状态 |
| `g_sd_storage_error` | 最后一次 `f_mount()` 返回的 `FRESULT` |
| `g_sd_card_error` | 最后一次底层 SD 驱动错误；本驱动 `SD_OK` 数值为 29 |
| `g_sd_self_test_stage` | 自检当前或最终阶段 |
| `g_sd_self_test_errno` | 自检失败时的 `errno` |
| `g_sd_self_test_offset` | 数据比较首次不一致的字节位置 |
| `g_sd_self_test_seed` | 当前随机种子，可用于复现 |
| `g_sd_self_test_round` | 当前随机测试轮次 |
| `g_sd_self_test_operation` | 当前轮次中的随机操作序号 |

挂载状态：

| 状态 | 含义 |
|---|---|
| `SD_STORAGE_UNINITIALIZED` | 尚未调用初始化 |
| `SD_STORAGE_READY` | 可正常读写 |
| `SD_STORAGE_NO_CARD` | 卡未准备好或初始化失败 |
| `SD_STORAGE_NO_FILESYSTEM` | 找不到 FAT 文件系统 |
| `SD_STORAGE_IO_ERROR` | 其他 I/O 错误 |

常用 GDB 检查命令：

```gdb
print g_sd_storage_state
print g_sd_storage_error
print g_sd_card_error
print g_sd_self_test_stage
print g_sd_self_test_errno
print g_sd_self_test_offset
print/x g_sd_self_test_seed
print g_sd_self_test_round
print g_sd_self_test_operation
```

## 15. 常见故障排查

### 15.1 `SD_STORAGE_NO_CARD`

按顺序检查：

1. SD 卡是否插紧、供电是否为 3.3 V。
2. PC8~PC12、PD2 接线是否和代码一致。
3. CMD、DAT0~DAT3 是否有上拉。
4. GPIO 是否为 AF12。
5. SDIO 和 GPIO 时钟是否已使能。
6. `g_sd_card_error` 对应哪个 `sd_error_enum`。
7. 降低 SDIO 传输时钟后是否恢复。

### 15.2 `SD_STORAGE_NO_FILESYSTEM`

卡能通信，但没有识别到 FAT12/FAT16/FAT32。常见原因：

- SD 卡是 exFAT，当前未启用 exFAT。
- 分区表或文件系统损坏。
- 卡是未格式化裸介质。

先在电脑备份数据，再格式化为 FAT32。固件不会自动格式化。

### 15.3 写入后电脑看不到最新内容

确保调用了 `fsync(fd)`、`fflush(file)` 或正常 `close/fclose`。突然断电前没有同步，
FatFs 缓存和目录项可能尚未写回。

### 15.4 偶发 CRC 或超时

检查供电压降、卡座接触、走线长度、上拉阻值和 SDIO 时钟。低速稳定而高速不稳定，
通常优先怀疑信号完整性，而不是文件 API。

### 15.5 `EMFILE`

有文件没有关闭，或者同时打开超过 `SD_MAX_OPEN_FILES`。检查所有成功的 `open/fopen`
是否在每条返回路径中对应 `close/fclose`。

## 16. 增加一个新的文件 API

例如要增加 `utime()`：

1. 在 `posix_fs.c` 中实现标准函数签名。
2. 用 `fatfs_path()` 转换 `/sd` 路径。
3. 把参数转换为 FatFs `FILINFO` 日期时间格式。
4. 调用 `f_utime()`。
5. 用 `fatfs_set_errno()` 转换 `FRESULT`。
6. 在自检中增加成功和失败用例。
7. 编译并实机验证。
8. 在本文档的接口清单和示例中补充说明。

不要直接修改 picolibc 或 FatFs 上游源码来增加项目 API；适配逻辑应保留在
`APP/Storage`，这样升级第三方库时更容易比较和迁移。

## 17. 编译和实机验证

在 `APP` 目录执行：

```sh
PATH=/usr/bin:/bin:/usr/sbin:/sbin meson setup build --cross-file arm-none-eabi.ini
PATH=/usr/bin:/bin:/usr/sbin:/sbin meson compile -C build/
```

若 `build` 已存在，配置改变后可用：

```sh
PATH=/usr/bin:/bin:/usr/sbin:/sbin meson setup build --cross-file arm-none-eabi.ini --reconfigure
```

真实设备调试按仓库约定使用：

```sh
gdb-multiarch -x init.gdb build/APP.elf -ex '<command>' -batch
```

修改文件系统后，至少重新验证：

- 编译和链接成功。
- Flash/RAM 未超限。
- `g_sd_storage_state == SD_STORAGE_READY`。
- `g_sd_self_test_stage == SD_SELF_TEST_PASSED`。
- 将卡插入电脑后能正常挂载，文件系统检查无错误。
