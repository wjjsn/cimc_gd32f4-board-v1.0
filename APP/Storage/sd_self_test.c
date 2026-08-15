#include "sd_storage.h"

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define TEST_DIR "/sd/.cimc-test"
#define TEST_FILE TEST_DIR "/random.bin"
#define TEST_RENAMED TEST_DIR "/renamed.bin"
#define TEST_STDIO TEST_DIR "/stdio.txt"
#define TEST_MISSING TEST_DIR "/missing.bin"
#define TEST_MODEL_SIZE 8193U
#define TEST_IO_SIZE 521U
#define TEST_RANDOM_ROUNDS 4U
#define TEST_RANDOM_WRITES 48U
#define TEST_RANDOM_READS 48U

volatile sd_self_test_stage_t g_sd_self_test_stage = SD_SELF_TEST_NOT_RUN;
volatile int g_sd_self_test_errno = 0;
volatile uint32_t g_sd_self_test_offset = 0;
volatile uint32_t g_sd_self_test_seed = 0;
volatile uint32_t g_sd_self_test_round = 0;
volatile uint32_t g_sd_self_test_operation = 0;

static uint8_t expected[TEST_MODEL_SIZE];
static uint8_t io_buffer[TEST_IO_SIZE];

static uint32_t random_next(uint32_t *state)
{
	/* xorshift32：状态很小、分布足够用于 I/O 顺序扰动，且给定种子可复现。 */
	uint32_t value = *state;
	value ^= value << 13;
	value ^= value >> 17;
	value ^= value << 5;
	*state = value;
	return value;
}

static void cleanup(void)
{
	(void)chdir("/sd");
	(void)unlink(TEST_FILE);
	(void)unlink(TEST_RENAMED);
	(void)unlink(TEST_STDIO);
	(void)rmdir(TEST_DIR);
}

static int fail_with_errno(int error)
{
	cleanup();
	g_sd_self_test_errno = error != 0 ? error : EIO;
	g_sd_self_test_stage = SD_SELF_TEST_FAILED;
	errno = g_sd_self_test_errno;
	return -1;
}

static int fail(void)
{
	return fail_with_errno(errno);
}

static int write_exact(int fd, const void *buffer, size_t length)
{
	const uint8_t *next = buffer;
	while (length > 0U) {
		ssize_t written = write(fd, next, length);
		if (written <= 0) {
			if (written == 0)
				errno = EIO;
			return -1;
		}
		next += written;
		length -= (size_t)written;
	}
	return 0;
}

static int read_exact(int fd, void *buffer, size_t length)
{
	uint8_t *next = buffer;
	while (length > 0U) {
		ssize_t count = read(fd, next, length);
		if (count <= 0) {
			if (count == 0)
				errno = EIO;
			return -1;
		}
		next += count;
		length -= (size_t)count;
	}
	return 0;
}

static int compare_at(const uint8_t *actual, const uint8_t *reference, size_t length,
			      uint32_t file_offset)
{
	for (size_t index = 0; index < length; ++index) {
		if (actual[index] != reference[index]) {
			g_sd_self_test_offset = file_offset + (uint32_t)index;
			errno = EIO;
			return -1;
		}
	}
	return 0;
}

static int verify_file(int fd, size_t length)
{
	if (lseek(fd, 0, SEEK_SET) != 0)
		return -1;
	for (size_t offset = 0; offset < length;) {
		size_t count = length - offset;
		if (count > sizeof(io_buffer))
			count = sizeof(io_buffer);
		if (read_exact(fd, io_buffer, count) < 0 ||
		    compare_at(io_buffer, expected + offset, count, (uint32_t)offset) < 0)
			return -1;
		offset += count;
	}
	if (read(fd, io_buffer, 1U) != 0) {
		errno = EIO;
		return -1;
	}
	return 0;
}

static int random_io_round(uint32_t seed)
{
	uint32_t random = seed != 0U ? seed : 0x6D2B79F5U;
	int fd = open(TEST_FILE, O_CREAT | O_TRUNC | O_RDWR, 0666);
	if (fd < 0)
		return -1;
	memset(expected, 0, sizeof(expected));

	/* 先扩展到奇数长度，严格验证新区域是否按 POSIX 语义读取为零。 */
	g_sd_self_test_stage = SD_SELF_TEST_EXTEND;
	if (ftruncate(fd, TEST_MODEL_SIZE) < 0 || verify_file(fd, TEST_MODEL_SIZE) < 0)
		goto failed;

	g_sd_self_test_stage = SD_SELF_TEST_RANDOM_WRITE;
	for (uint32_t operation = 0; operation < TEST_RANDOM_WRITES; ++operation) {
		g_sd_self_test_operation = operation;
		size_t count = 1U + random_next(&random) % sizeof(io_buffer);
		size_t offset = random_next(&random) % TEST_MODEL_SIZE;
		if (count > TEST_MODEL_SIZE - offset)
			count = TEST_MODEL_SIZE - offset;
		for (size_t index = 0; index < count; ++index)
			io_buffer[index] = (uint8_t)random_next(&random);
		if (lseek(fd, (off_t)offset, SEEK_SET) != (off_t)offset ||
		    write_exact(fd, io_buffer, count) < 0)
			goto failed;
		memcpy(expected + offset, io_buffer, count);

		/* 周期性同步并立即抽样，覆盖缓存、跨扇区和未对齐 DMA 中转路径。 */
		if ((operation % 7U) == 0U) {
			size_t probe_count = 1U + random_next(&random) % sizeof(io_buffer);
			size_t probe_offset = random_next(&random) % TEST_MODEL_SIZE;
			if (probe_count > TEST_MODEL_SIZE - probe_offset)
				probe_count = TEST_MODEL_SIZE - probe_offset;
			if (fsync(fd) < 0 ||
			    lseek(fd, (off_t)probe_offset, SEEK_SET) != (off_t)probe_offset ||
			    read_exact(fd, io_buffer, probe_count) < 0 ||
			    compare_at(io_buffer, expected + probe_offset, probe_count,
				       (uint32_t)probe_offset) < 0)
				goto failed;
		}
	}
	if (fsync(fd) < 0 || close(fd) < 0)
		return -1;

	/* 关闭后重新打开，以排除只命中 FIL 内存缓存而未真正写卡的假通过。 */
	g_sd_self_test_stage = SD_SELF_TEST_RANDOM_READ;
	fd = open(TEST_FILE, O_RDWR);
	if (fd < 0)
		return -1;
	for (uint32_t operation = 0; operation < TEST_RANDOM_READS; ++operation) {
		g_sd_self_test_operation = operation;
		size_t count = 1U + random_next(&random) % sizeof(io_buffer);
		size_t offset = random_next(&random) % TEST_MODEL_SIZE;
		if (count > TEST_MODEL_SIZE - offset)
			count = TEST_MODEL_SIZE - offset;
		if (lseek(fd, (off_t)offset, SEEK_SET) != (off_t)offset ||
		    read_exact(fd, io_buffer, count) < 0 ||
		    compare_at(io_buffer, expected + offset, count, (uint32_t)offset) < 0)
			goto failed;
	}
	if (verify_file(fd, TEST_MODEL_SIZE) < 0 || close(fd) < 0)
		return -1;
	return 0;

failed: {
	int error = errno;
	(void)close(fd);
	errno = error;
	return -1;
}
}

static int test_append_and_truncate(void)
{
	static const uint8_t first[] = {0x00, 0x7F, 0x80, 0xFF, 0x55};
	static const uint8_t second[] = {0x13, 0x37, 0xC0, 0xDE};
	int fd = open(TEST_FILE, O_WRONLY | O_APPEND);
	if (fd < 0 || lseek(fd, 0, SEEK_SET) != 0 || write_exact(fd, first, sizeof(first)) < 0)
		goto failed;
	if (close(fd) < 0)
		return -1;

	fd = open(TEST_FILE, O_WRONLY | O_APPEND);
	if (fd < 0 || lseek(fd, 1, SEEK_SET) != 1 || write_exact(fd, second, sizeof(second)) < 0)
		goto failed;
	if (close(fd) < 0)
		return -1;
	fd = -1;

	struct stat status;
	if (stat(TEST_FILE, &status) < 0 ||
	    status.st_size != (off_t)(TEST_MODEL_SIZE + sizeof(first) + sizeof(second))) {
		errno = EIO;
		return -1;
	}
	fd = open(TEST_FILE, O_RDONLY);
	if (fd < 0 || lseek(fd, TEST_MODEL_SIZE, SEEK_SET) != TEST_MODEL_SIZE ||
	    read_exact(fd, io_buffer, sizeof(first) + sizeof(second)) < 0 ||
	    memcmp(io_buffer, first, sizeof(first)) != 0 ||
	    memcmp(io_buffer + sizeof(first), second, sizeof(second)) != 0)
		goto failed;
	if (close(fd) < 0)
		return -1;
	fd = -1;

	g_sd_self_test_stage = SD_SELF_TEST_TRUNCATE;
	if (truncate(TEST_FILE, 4099) < 0 || stat(TEST_FILE, &status) < 0 || status.st_size != 4099) {
		errno = EIO;
		return -1;
	}
	fd = open(TEST_FILE, O_RDWR);
	if (fd < 0 || verify_file(fd, 4099) < 0 || ftruncate(fd, 6147) < 0)
		goto failed;
	memset(expected + 4099, 0, 6147 - 4099);
	if (verify_file(fd, 6147) < 0 || close(fd) < 0)
		return -1;
	return 0;

failed: {
	int error = errno;
	if (fd >= 0)
		(void)close(fd);
	errno = error;
	return -1;
}
}

static int test_error_paths(void)
{
	struct stat status;
	uint8_t byte = 0;
	int fd;

	g_sd_self_test_stage = SD_SELF_TEST_ERROR_PATHS;
	errno = 0;
	if (open(TEST_MISSING, O_RDONLY) != -1 || errno != ENOENT)
		goto wrong_error;
	errno = 0;
	if (stat(TEST_MISSING, &status) != -1 || errno != ENOENT)
		goto wrong_error;
	errno = 0;
	fd = open(TEST_FILE, O_CREAT | O_EXCL | O_RDWR, 0666);
	if (fd != -1) {
		(void)close(fd);
		goto wrong_error;
	}
	if (errno != EEXIST)
		goto wrong_error;
	errno = 0;
	if (read(-1, &byte, 1U) != -1 || errno != EBADF)
		goto wrong_error;

	fd = open(TEST_FILE, O_WRONLY);
	if (fd < 0)
		return -1;
	errno = 0;
	if (read(fd, &byte, 1U) != -1 || errno != EBADF) {
		(void)close(fd);
		goto wrong_error;
	}
	if (close(fd) < 0)
		return -1;

	fd = open(TEST_FILE, O_RDONLY);
	if (fd < 0)
		return -1;
	errno = 0;
	if (write(fd, &byte, 1U) != -1 || errno != EBADF) {
		(void)close(fd);
		goto wrong_error;
	}
	errno = 0;
	if (lseek(fd, -1, SEEK_SET) != (off_t)-1 || errno != EINVAL) {
		(void)close(fd);
		goto wrong_error;
	}
	errno = 0;
	if (ftruncate(fd, 1) != -1 || errno != EBADF) {
		(void)close(fd);
		goto wrong_error;
	}
	return close(fd);

wrong_error:
	errno = EPROTO;
	return -1;
}

static int test_directory_and_stdio(void)
{
	g_sd_self_test_stage = SD_SELF_TEST_RENAME;
	if (rename(TEST_FILE, TEST_RENAMED) < 0 || access(TEST_FILE, F_OK) == 0 || errno != ENOENT ||
	    access(TEST_RENAMED, R_OK | W_OK) < 0)
		return -1;

	g_sd_self_test_stage = SD_SELF_TEST_DIRECTORY;
	sd_dir_t *dir = sd_opendir(TEST_DIR);
	if (dir == NULL)
		return -1;
	unsigned int found = 0;
	sd_dirent_t entry;
	int result;
	while ((result = sd_readdir(dir, &entry)) > 0) {
		if (strcmp(entry.name, "renamed.bin") == 0 && entry.size == 6147U)
			++found;
	}
	if (result < 0) {
		(void)sd_closedir(dir);
		return -1;
	}
	sd_rewinddir(dir);
	if (sd_readdir(dir, &entry) <= 0 || sd_closedir(dir) < 0 || found != 1U) {
		errno = EIO;
		return -1;
	}

	g_sd_self_test_stage = SD_SELF_TEST_CWD;
	char cwd[32];
	if (chdir(TEST_DIR) < 0 || getcwd(cwd, sizeof(cwd)) == NULL || strcmp(cwd, TEST_DIR) != 0)
		return -1;
	int fd = open("relative.bin", O_CREAT | O_EXCL | O_RDWR, 0666);
	if (fd < 0 || write_exact(fd, "relative", 8U) < 0 || close(fd) < 0 ||
	    unlink("relative.bin") < 0 || chdir("/sd") < 0)
		return -1;

	g_sd_self_test_stage = SD_SELF_TEST_STDIO;
	FILE *stream = fopen(TEST_STDIO, "w+");
	static const char data[] = "FatFs stdio verified\nline 2\n";
	char readback[sizeof(data)] = {0};
	if (stream == NULL || fwrite(data, 1U, sizeof(data), stream) != sizeof(data) ||
	    fflush(stream) != 0 || fseek(stream, 0L, SEEK_SET) != 0 ||
	    fread(readback, 1U, sizeof(readback), stream) != sizeof(readback) ||
	    memcmp(data, readback, sizeof(data)) != 0 || fclose(stream) != 0) {
		int error = errno != 0 ? errno : EIO;
		if (stream != NULL)
			(void)fclose(stream);
		errno = error;
		return -1;
	}
	return 0;
}

int sd_storage_self_test_seed(uint32_t seed)
{
	cleanup();
	g_sd_self_test_errno = 0;
	g_sd_self_test_offset = UINT32_MAX;
	g_sd_self_test_seed = seed != 0U ? seed : 0xA5C3E27DU;
	g_sd_self_test_round = 0;
	g_sd_self_test_operation = 0;

	if (!sd_storage_is_ready()) {
		errno = ENODEV;
		return fail();
	}

	g_sd_self_test_stage = SD_SELF_TEST_CREATE_DIR;
	if (mkdir(TEST_DIR, 0777) < 0)
		return fail();

	for (uint32_t round = 0; round < TEST_RANDOM_ROUNDS; ++round) {
		g_sd_self_test_round = round;
		uint32_t round_seed = g_sd_self_test_seed ^ (0x9E3779B9U * (round + 1U));
		if (random_io_round(round_seed) < 0)
			return fail();
	}

	g_sd_self_test_stage = SD_SELF_TEST_APPEND;
	if (test_append_and_truncate() < 0 || test_error_paths() < 0 ||
	    test_directory_and_stdio() < 0)
		return fail();

	g_sd_self_test_stage = SD_SELF_TEST_CLEANUP;
	if (unlink(TEST_RENAMED) < 0 || unlink(TEST_STDIO) < 0 || rmdir(TEST_DIR) < 0)
		return fail();

	g_sd_self_test_errno = 0;
	g_sd_self_test_offset = UINT32_MAX;
	g_sd_self_test_stage = SD_SELF_TEST_PASSED;
	return 0;
}

int sd_storage_self_test(void)
{
	/* 每次调用使用不同种子；GDB 可记录 g_sd_self_test_seed 后精确复现。 */
	static uint32_t next_seed = 0xA5C3E27DU;
	uint32_t seed = next_seed;
	next_seed += 0x6D2B79F5U;
	return sd_storage_self_test_seed(seed);
}
