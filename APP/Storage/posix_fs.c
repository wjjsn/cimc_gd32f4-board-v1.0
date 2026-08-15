#include "posix_fs.h"

#include "SEGGER_RTT.h"

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define SD_FD_BASE 3
#define SD_MAX_OPEN_FILES 8
#define SD_PATH_MAX 260

/*
 * 裸机没有内核替我们保存“文件描述符 -> 文件对象”的关系，因此使用固定数组。
 * fd 0/1/2 留给 stdin/stdout/stderr，数组下标 0 对应用户看到的 fd 3。
 * 修改 SD_MAX_OPEN_FILES 时，同时检查 ffconf.h 的 FF_FS_LOCK 和 RAM 占用。
 */
typedef struct {
	FIL file;
	int flags;
	uint8_t used;
} file_slot_t;

static file_slot_t files[SD_MAX_OPEN_FILES];

int fatfs_set_errno(FRESULT result)
{
	/* 应用代码只理解 errno；这里集中维护 FatFs 错误到 errno 的对应关系。 */
	switch (result) {
	case FR_OK:
		return 0;
	case FR_NO_FILE:
	case FR_NO_PATH:
		errno = ENOENT;
		break;
	case FR_INVALID_NAME:
	case FR_INVALID_PARAMETER:
		errno = EINVAL;
		break;
	case FR_DENIED:
		errno = EACCES;
		break;
	case FR_DISK_ERR:
		errno = EIO;
		break;
	case FR_EXIST:
		errno = EEXIST;
		break;
	case FR_WRITE_PROTECTED:
		errno = EROFS;
		break;
	case FR_INVALID_DRIVE:
	case FR_NOT_ENABLED:
	case FR_NOT_READY:
		errno = ENODEV;
		break;
	case FR_NO_FILESYSTEM:
		errno = ENXIO;
		break;
	case FR_LOCKED:
		errno = EBUSY;
		break;
	case FR_NOT_ENOUGH_CORE:
		errno = ENOMEM;
		break;
	case FR_TOO_MANY_OPEN_FILES:
		errno = EMFILE;
		break;
	default:
		errno = EIO;
		break;
	}
	return -1;
}

int fatfs_path(const char *path, char *output, size_t output_size)
{
	if (path == NULL || output == NULL || output_size < 4U) {
		errno = EINVAL;
		return -1;
	}

	/* `/sd` 是应用层挂载点，FatFs 中唯一卷的名字是 `0:`。 */
	const char *suffix = path;
	if (strncmp(path, "/sd", 3U) == 0) {
		if (path[3] != '\0' && path[3] != '/') {
			errno = ENOENT;
			return -1;
		}
		suffix = path + 3;
	} else if (path[0] == '/') {
		errno = ENOENT;
		return -1;
	}

	int written;
	if (suffix[0] == '\0')
		written = snprintf(output, output_size, "0:/");
	else if (suffix[0] == '/')
		written = snprintf(output, output_size, "0:%s", suffix);
	else
		written = snprintf(output, output_size, "%s", suffix);
	if (written < 0 || (size_t)written >= output_size) {
		errno = ENAMETOOLONG;
		return -1;
	}
	return 0;
}

static file_slot_t *slot_get(int fd)
{
	if (fd < SD_FD_BASE || fd >= SD_FD_BASE + SD_MAX_OPEN_FILES) {
		errno = EBADF;
		return NULL;
	}
	file_slot_t *slot = &files[fd - SD_FD_BASE];
	if (!slot->used) {
		errno = EBADF;
		return NULL;
	}
	return slot;
}

static BYTE open_mode(int flags, int *valid)
{
	/* 把 <fcntl.h> 的 O_* 标志翻译为 FatFs 的 FA_* 标志。 */
	BYTE mode = 0;
	*valid = 1;
	switch (flags & O_ACCMODE) {
	case O_RDONLY:
		mode = FA_READ;
		break;
	case O_WRONLY:
		mode = FA_WRITE;
		break;
	case O_RDWR:
		mode = FA_READ | FA_WRITE;
		break;
	default:
		*valid = 0;
		return 0;
	}

	if ((flags & O_EXCL) && !(flags & O_CREAT)) {
		*valid = 0;
		return 0;
	}
	if ((flags & O_CREAT) && (flags & O_EXCL))
		mode |= FA_CREATE_NEW;
	else if ((flags & O_CREAT) && (flags & O_TRUNC))
		mode |= FA_CREATE_ALWAYS;
	else if (flags & O_CREAT)
		mode |= FA_OPEN_ALWAYS;
	else if (flags & O_TRUNC)
		mode |= FA_CREATE_ALWAYS;
	else
		mode |= FA_OPEN_EXISTING;
	return mode;
}

int open(const char *path, int flags, ...)
{
	char converted[SD_PATH_MAX];
	if (fatfs_path(path, converted, sizeof(converted)) < 0)
		return -1;

	int valid;
	BYTE mode = open_mode(flags, &valid);
	if (!valid) {
		errno = EINVAL;
		return -1;
	}

	/* 找到一个空槽位。固定数组不会发生堆内存碎片，也便于估算 RAM。 */
	file_slot_t *slot = NULL;
	int fd = -1;
	for (int index = 0; index < SD_MAX_OPEN_FILES; ++index) {
		if (!files[index].used) {
			slot = &files[index];
			fd = index + SD_FD_BASE;
			break;
		}
	}
	if (slot == NULL) {
		errno = EMFILE;
		return -1;
	}

	FRESULT result = f_open(&slot->file, converted, mode);
	if (result != FR_OK)
		return fatfs_set_errno(result);
	slot->flags = flags;
	slot->used = 1U;
	if ((flags & O_APPEND) != 0) {
		result = f_lseek(&slot->file, f_size(&slot->file));
		if (result != FR_OK) {
			(void)f_close(&slot->file);
			slot->used = 0U;
			return fatfs_set_errno(result);
		}
	}
	return fd;
}

int close(int fd)
{
	file_slot_t *slot = slot_get(fd);
	if (slot == NULL)
		return -1;
	FRESULT result = f_close(&slot->file);
	slot->used = 0U;
	return result == FR_OK ? 0 : fatfs_set_errno(result);
}

ssize_t read(int fd, void *buffer, size_t length)
{
	if (fd == STDIN_FILENO)
		return 0;
	file_slot_t *slot = slot_get(fd);
	if (slot == NULL)
		return -1;
	if ((slot->flags & O_ACCMODE) == O_WRONLY) {
		errno = EBADF;
		return -1;
	}
	UINT read_count = 0;
	FRESULT result = f_read(&slot->file, buffer, length, &read_count);
	return result == FR_OK ? (ssize_t)read_count : fatfs_set_errno(result);
}

ssize_t write(int fd, const void *buffer, size_t length)
{
	/* printf/fprintf(stdout, ...) 最终会写 fd 1；将标准输出保留在 RTT。 */
	if (fd == STDOUT_FILENO || fd == STDERR_FILENO) {
		SEGGER_RTT_Write(0, buffer, length);
		return (ssize_t)length;
	}
	file_slot_t *slot = slot_get(fd);
	if (slot == NULL)
		return -1;
	if ((slot->flags & O_ACCMODE) == O_RDONLY) {
		errno = EBADF;
		return -1;
	}
	if ((slot->flags & O_APPEND) != 0) {
		/* 保证每次 write 都追加，即使调用者之前执行过 lseek。 */
		FRESULT seek_result = f_lseek(&slot->file, f_size(&slot->file));
		if (seek_result != FR_OK)
			return fatfs_set_errno(seek_result);
	}
	UINT write_count = 0;
	FRESULT result = f_write(&slot->file, buffer, length, &write_count);
	return result == FR_OK ? (ssize_t)write_count : fatfs_set_errno(result);
}

off_t lseek(int fd, off_t offset, int whence)
{
	file_slot_t *slot = slot_get(fd);
	if (slot == NULL)
		return (off_t)-1;
	int64_t target;
	switch (whence) {
	case SEEK_SET:
		target = offset;
		break;
	case SEEK_CUR:
		target = (int64_t)f_tell(&slot->file) + offset;
		break;
	case SEEK_END:
		target = (int64_t)f_size(&slot->file) + offset;
		break;
	default:
		errno = EINVAL;
		return (off_t)-1;
	}
	if (target < 0 || (uint64_t)target > UINT32_MAX) {
		errno = EINVAL;
		return (off_t)-1;
	}
	FRESULT result = f_lseek(&slot->file, (FSIZE_t)target);
	if (result != FR_OK) {
		fatfs_set_errno(result);
		return (off_t)-1;
	}
	return (off_t)f_tell(&slot->file);
}

static void stat_fill(struct stat *status, FSIZE_t size, BYTE attributes)
{
	/* FAT 没有 POSIX 用户/组权限，只把目录、普通文件和只读属性映射过去。 */
	memset(status, 0, sizeof(*status));
	status->st_mode = (attributes & AM_DIR) ? S_IFDIR | 0777 : S_IFREG | 0666;
	if (attributes & AM_RDO)
		status->st_mode &= ~(S_IWUSR | S_IWGRP | S_IWOTH);
	status->st_nlink = 1;
	status->st_size = size;
	status->st_blksize = 512;
	status->st_blocks = (size + 511U) / 512U;
}

int fstat(int fd, struct stat *status)
{
	if (status == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (fd >= 0 && fd <= 2) {
		memset(status, 0, sizeof(*status));
		status->st_mode = S_IFCHR | 0666;
		return 0;
	}
	file_slot_t *slot = slot_get(fd);
	if (slot == NULL)
		return -1;
	stat_fill(status, f_size(&slot->file), 0);
	return 0;
}

int stat(const char *path, struct stat *status)
{
	char converted[SD_PATH_MAX];
	if (status == NULL || fatfs_path(path, converted, sizeof(converted)) < 0)
		return -1;
	FILINFO info;
	FRESULT result = f_stat(converted, &info);
	if (result != FR_OK)
		return fatfs_set_errno(result);
	stat_fill(status, info.fsize, info.fattrib);
	return 0;
}

int fsync(int fd)
{
	file_slot_t *slot = slot_get(fd);
	if (slot == NULL)
		return -1;
	FRESULT result = f_sync(&slot->file);
	return result == FR_OK ? 0 : fatfs_set_errno(result);
}

int ftruncate(int fd, off_t length)
{
	file_slot_t *slot = slot_get(fd);
	if (slot == NULL)
		return -1;
	if ((slot->flags & O_ACCMODE) == O_RDONLY) {
		errno = EBADF;
		return -1;
	}
	if (length < 0 || (uint64_t)length > UINT32_MAX) {
		errno = EINVAL;
		return -1;
	}
	/*
	 * FatFs 的 f_lseek() 可以扩展文件，但新分配区域内容未定义；POSIX 要求扩展
	 * 区域读回为零，所以扩展时必须显式写零。缩短则在目标位置调用 f_truncate()。
	 */
	FSIZE_t original = f_tell(&slot->file);
	FSIZE_t old_size = f_size(&slot->file);
	FRESULT result = f_lseek(&slot->file, (FSIZE_t)length);
	if (result == FR_OK && (FSIZE_t)length < old_size) {
		result = f_truncate(&slot->file);
	} else if (result == FR_OK && (FSIZE_t)length > old_size) {
		static const uint8_t zeros[64] = {0};
		result = f_lseek(&slot->file, old_size);
		FSIZE_t remaining = (FSIZE_t)length - old_size;
		while (result == FR_OK && remaining > 0U) {
			UINT count = remaining > sizeof(zeros) ? sizeof(zeros) : (UINT)remaining;
			UINT written = 0;
			result = f_write(&slot->file, zeros, count, &written);
			if (result == FR_OK && written != count) {
				errno = ENOSPC;
				return -1;
			}
			remaining -= written;
		}
	}
	if (result == FR_OK && original <= (FSIZE_t)length)
		result = f_lseek(&slot->file, original);
	return result == FR_OK ? 0 : fatfs_set_errno(result);
}

int truncate(const char *path, off_t length)
{
	int fd = open(path, O_WRONLY);
	if (fd < 0)
		return -1;
	int result = ftruncate(fd, length);
	int close_result = close(fd);
	return result != 0 ? result : close_result;
}

int unlink(const char *path)
{
	char converted[SD_PATH_MAX];
	if (fatfs_path(path, converted, sizeof(converted)) < 0)
		return -1;
	FRESULT result = f_unlink(converted);
	return result == FR_OK ? 0 : fatfs_set_errno(result);
}

int remove(const char *path)
{
	return unlink(path);
}

int access(const char *path, int mode)
{
	struct stat status;
	if ((mode & ~(R_OK | W_OK | X_OK)) != 0) {
		errno = EINVAL;
		return -1;
	}
	if (stat(path, &status) < 0)
		return -1;
	if ((mode & W_OK) != 0 && (status.st_mode & S_IWUSR) == 0) {
		errno = EACCES;
		return -1;
	}
	return 0;
}

int chmod(const char *path, mode_t mode)
{
	char converted[SD_PATH_MAX];
	if (fatfs_path(path, converted, sizeof(converted)) < 0)
		return -1;
	BYTE attributes = (mode & (S_IWUSR | S_IWGRP | S_IWOTH)) ? 0U : AM_RDO;
	FRESULT result = f_chmod(converted, attributes, AM_RDO);
	return result == FR_OK ? 0 : fatfs_set_errno(result);
}

int mkdir(const char *path, mode_t mode)
{
	(void)mode;
	char converted[SD_PATH_MAX];
	if (fatfs_path(path, converted, sizeof(converted)) < 0)
		return -1;
	FRESULT result = f_mkdir(converted);
	return result == FR_OK ? 0 : fatfs_set_errno(result);
}

int rmdir(const char *path)
{
	return unlink(path);
}

int rename(const char *old_path, const char *new_path)
{
	char old_converted[SD_PATH_MAX];
	char new_converted[SD_PATH_MAX];
	if (fatfs_path(old_path, old_converted, sizeof(old_converted)) < 0 ||
	    fatfs_path(new_path, new_converted, sizeof(new_converted)) < 0)
		return -1;
	FRESULT result = f_rename(old_converted, new_converted);
	return result == FR_OK ? 0 : fatfs_set_errno(result);
}

int chdir(const char *path)
{
	char converted[SD_PATH_MAX];
	if (fatfs_path(path, converted, sizeof(converted)) < 0)
		return -1;
	FRESULT result = f_chdir(converted);
	return result == FR_OK ? 0 : fatfs_set_errno(result);
}

char *getcwd(char *buffer, size_t size)
{
	if (buffer == NULL || size < 4U) {
		errno = EINVAL;
		return NULL;
	}
	char fat_path[SD_PATH_MAX];
	FRESULT result = f_getcwd(fat_path, sizeof(fat_path));
	if (result != FR_OK) {
		fatfs_set_errno(result);
		return NULL;
	}
	const char *suffix = fat_path;
	if (strncmp(fat_path, "0:", 2U) == 0)
		suffix = fat_path + 2;
	int written = snprintf(buffer, size, "/sd%s", suffix[0] ? suffix : "/");
	if (written < 0 || (size_t)written >= size) {
		errno = ERANGE;
		return NULL;
	}
	return buffer;
}
