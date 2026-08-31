/// @file t_procfs_read.c
/// @brief Regression test for #194: reads of `/proc/<pid>/stat` and
/// `/proc/<pid>/cmdline` must never write more than `nbyte` bytes into the
/// caller's buffer.
/// @details The kernel used to compute the correct byte count but then copied
/// with `strcpy`, writing the whole file content (plus the terminating NUL)
/// through the user buffer. This test detects that overwrite through guard
/// regions adjacent to the read area (not just the returned byte count),
/// checks small reads, sequential non-zero offsets, EOF behavior and a normal
/// full read, and reproduces the original crash symptom in a child process.
/// @copyright (c) 2014-2026 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <strerror.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <syslog.h>
#include <unistd.h>

/// Fill pattern of every guard region.
#define GUARD_BYTE 0xA5

/// Size of the read area inside the guarded block.
#define AREA_SIZE 16

/// Guard region after the read area: bigger than BUFSIZ (512), the largest
/// content the proc read handlers can generate, so even a full `strcpy` of
/// the longest file stays inside the block and is detected as corruption
/// instead of smashing unrelated memory.
#define POST_SIZE 640

/// A read area surrounded by guard regions, so writes beyond `nbyte` bytes
/// are observable as changed guard bytes.
typedef struct {
    unsigned char pre[32];
    unsigned char area[AREA_SIZE];
    unsigned char post[POST_SIZE];
} guarded_block_t;

/// The guarded block used by every sub-test.
static guarded_block_t block;

/// @brief Resets the guarded block: read area and guards all GUARD_BYTE.
static void guarded_reset(void)
{
    memset(&block, GUARD_BYTE, sizeof(block));
}

/// @brief Counts the guard bytes that have been overwritten.
/// @return the number of bytes in pre/post that differ from GUARD_BYTE.
static int guarded_clobbered(void)
{
    int clobbered = 0;
    for (size_t i = 0; i < sizeof(block.pre); ++i) {
        if (block.pre[i] != GUARD_BYTE) {
            ++clobbered;
        }
    }
    for (size_t i = 0; i < sizeof(block.post); ++i) {
        if (block.post[i] != GUARD_BYTE) {
            ++clobbered;
        }
    }
    return clobbered;
}

/// @brief Checks that only the first `n` bytes of the read area were written.
/// @param n the number of bytes the read reported.
/// @return 0 on success, -1 on failure.
static int area_boundaries_respected(size_t n)
{
    for (size_t i = 0; i < n; ++i) {
        if (block.area[i] == GUARD_BYTE) {
            syslog(LOG_ERR, "[t_procfs_read] byte %u of the read area untouched", (unsigned)i);
            return -1;
        }
    }
    for (size_t i = n; i < AREA_SIZE; ++i) {
        if (block.area[i] != GUARD_BYTE) {
            syslog(LOG_ERR, "[t_procfs_read] byte %u beyond nbyte was written", (unsigned)i);
            return -1;
        }
    }
    return 0;
}

/// @brief Opens one of this process's own proc files.
/// @param leaf "stat" or "cmdline".
/// @return the file descriptor, or -1 on failure.
static int open_self(const char *leaf)
{
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/%s", getpid(), leaf);
    int fd = open(path, O_RDONLY, 0);
    if (fd < 0) {
        syslog(LOG_ERR, "[t_procfs_read] open %s: %s", path, strerror(errno));
        return -1;
    }
    return fd;
}

/// @brief Small read of `stat`: only nbyte bytes may be written.
/// @return 0 on success, -1 on failure.
static int check_stat_small_read(void)
{
    guarded_reset();
    int fd = open_self("stat");
    if (fd < 0) {
        return -1;
    }
    ssize_t n = read(fd, block.area, 4);
    int clobbered = guarded_clobbered();
    close(fd);
    if (n != 4) {
        syslog(LOG_ERR, "[t_procfs_read] small stat read returned %d, expected 4", (int)n);
        return -1;
    }
    if (clobbered != 0) {
        syslog(LOG_ERR, "[t_procfs_read] small stat read clobbered %d adjacent bytes", clobbered);
        return -1;
    }
    if (area_boundaries_respected(4) < 0) {
        return -1;
    }
    return 0;
}

/// @brief Sequential reads at a non-zero offset must respect the same bound.
/// @return 0 on success, -1 on failure.
static int check_stat_offset_read(void)
{
    guarded_reset();
    int fd = open_self("stat");
    if (fd < 0) {
        return -1;
    }
    // First read advances the file position.
    if (read(fd, block.area, 4) != 4) {
        syslog(LOG_ERR, "[t_procfs_read] offset test: first read failed");
        close(fd);
        return -1;
    }
    // The second read starts at offset 4.
    ssize_t n = read(fd, block.area, 8);
    int clobbered = guarded_clobbered();
    close(fd);
    if (n != 8) {
        syslog(LOG_ERR, "[t_procfs_read] offset read returned %d, expected 8", (int)n);
        return -1;
    }
    if (clobbered != 0) {
        syslog(LOG_ERR, "[t_procfs_read] offset read clobbered %d adjacent bytes", clobbered);
        return -1;
    }
    if (area_boundaries_respected(8) < 0) {
        return -1;
    }
    return 0;
}

/// @brief Reads the whole file in chunks: every chunk must respect the byte
/// bound and the file must end with a 0-byte read.
/// @return 0 on success, -1 on failure.
static int check_stat_read_to_eof(void)
{
    guarded_reset();
    int fd = open_self("stat");
    if (fd < 0) {
        return -1;
    }
    unsigned total     = 0;
    unsigned chunks    = 0;
    ssize_t n          = 0;
    int failed         = 0;
    for (;;) {
        // Only the first AREA_SIZE bytes of the area are handed to each
        // read: re-arm them so a short chunk proves the kernel stopped at
        // nbyte instead of seeing the previous chunk's bytes.
        memset(block.area, GUARD_BYTE, AREA_SIZE);
        n = read(fd, block.area, AREA_SIZE);
        if (n < 0) {
            syslog(LOG_ERR, "[t_procfs_read] eof loop read: %s", strerror(errno));
            failed = 1;
            break;
        }
        if (n == 0) {
            break;
        }
        ++chunks;
        total += (unsigned)n;
        if (n > AREA_SIZE) {
            syslog(LOG_ERR, "[t_procfs_read] eof loop read returned %d > %d", (int)n, AREA_SIZE);
            failed = 1;
            break;
        }
        if ((guarded_clobbered() != 0) || (area_boundaries_respected((size_t)n) < 0)) {
            syslog(LOG_ERR, "[t_procfs_read] eof loop chunk %u wrote past nbyte", chunks);
            failed = 1;
            break;
        }
    }
    // A further read at EOF must keep returning 0.
    if ((!failed) && (read(fd, block.area, AREA_SIZE) != 0)) {
        syslog(LOG_ERR, "[t_procfs_read] read at EOF did not return 0");
        failed = 1;
    }
    close(fd);
    if (failed) {
        return -1;
    }
    // The stat line is several hundred bytes long; a sane total proves the
    // loop walked the whole file without needing an exact golden length,
    // which would be fragile across content changes.
    if ((total < 128) || (total > 512) || (chunks < 2)) {
        syslog(LOG_ERR, "[t_procfs_read] stat total %u bytes in %u chunks is out of range", total, chunks);
        return -1;
    }
    return 0;
}

/// @brief A normal full read into a large buffer must return the whole file
/// without touching the guard region after it.
/// @return 0 on success, -1 on failure.
static int check_stat_full_read(void)
{
    guarded_reset();
    int fd = open_self("stat");
    if (fd < 0) {
        return -1;
    }
    // area is 16 bytes, so use the start of post as the read target: a
    // full-file buffer of BUFSIZ (512) bytes followed by what is left of the
    // guard region.
    unsigned char *buf     = block.post;
    const size_t buf_size  = 512;
    size_t guard_left      = sizeof(block.post) - buf_size;
    ssize_t n              = read(fd, buf, buf_size);
    close(fd);
    if (n <= 0) {
        syslog(LOG_ERR, "[t_procfs_read] full read returned %d", (int)n);
        return -1;
    }
    for (ssize_t i = 0; i < n; ++i) {
        if (buf[i] == GUARD_BYTE) {
            syslog(LOG_ERR, "[t_procfs_read] full read byte %d untouched", (int)i);
            return -1;
        }
    }
    for (size_t i = buf_size; i < buf_size + guard_left; ++i) {
        if (block.post[i] != GUARD_BYTE) {
            syslog(LOG_ERR, "[t_procfs_read] full read wrote past the buffer");
            return -1;
        }
    }
    return 0;
}

/// @brief Small read of `cmdline`: the name is short, the bound still holds.
/// @return 0 on success, -1 on failure.
static int check_cmdline_small_read(void)
{
    guarded_reset();
    int fd = open_self("cmdline");
    if (fd < 0) {
        return -1;
    }
    ssize_t n = read(fd, block.area, 4);
    int clobbered = guarded_clobbered();
    close(fd);
    if (n != 4) {
        syslog(LOG_ERR, "[t_procfs_read] small cmdline read returned %d, expected 4", (int)n);
        return -1;
    }
    if (clobbered != 0) {
        syslog(LOG_ERR, "[t_procfs_read] small cmdline read clobbered %d adjacent bytes", clobbered);
        return -1;
    }
    if (area_boundaries_respected(4) < 0) {
        return -1;
    }
    return 0;
}

/// @brief Child body: the original #194 symptom. A tiny stack buffer next to
/// live frame data, destroyed by the unbounded copy.
/// @return 0 (the parent checks how the child terminated).
static int child_stack_smash(void)
{
    char small[8];
    int fd = open_self("stat");
    if (fd < 0) {
        return -1;
    }
    // Ask for 4 bytes; the kernel must not write more, or the return path of
    // this very frame is destroyed.
    ssize_t n = read(fd, small, 4);
    close(fd);
    if (n != 4) {
        return -1;
    }
    return 0;
}

/// @brief Runs the stack-smash case in a child and requires a clean exit.
/// @return 0 on success, -1 on failure.
static int check_stack_survival(void)
{
    pid_t pid = fork();
    if (pid < 0) {
        syslog(LOG_ERR, "[t_procfs_read] fork: %s", strerror(errno));
        return -1;
    }
    if (pid == 0) {
        exit(child_stack_smash());
    }
    int status;
    if (waitpid(pid, &status, 0) == -1) {
        syslog(LOG_ERR, "[t_procfs_read] waitpid: %s", strerror(errno));
        return -1;
    }
    if (WIFSIGNALED(status)) {
        syslog(LOG_ERR, "[t_procfs_read] child killed by signal %d reading stat into a small buffer", WTERMSIG(status));
        return -1;
    }
    if (!WIFEXITED(status) || (WEXITSTATUS(status) != 0)) {
        syslog(LOG_ERR, "[t_procfs_read] child exited abnormally");
        return -1;
    }
    return 0;
}

int main(int argc, char *argv[])
{
    if (check_stat_small_read() < 0) {
        return EXIT_FAILURE;
    }
    if (check_stat_offset_read() < 0) {
        return EXIT_FAILURE;
    }
    if (check_stat_read_to_eof() < 0) {
        return EXIT_FAILURE;
    }
    if (check_stat_full_read() < 0) {
        return EXIT_FAILURE;
    }
    if (check_cmdline_small_read() < 0) {
        return EXIT_FAILURE;
    }
    if (check_stack_survival() < 0) {
        return EXIT_FAILURE;
    }
    syslog(LOG_INFO, "[t_procfs_read] all procfs read boundary checks passed");
    return EXIT_SUCCESS;
}
