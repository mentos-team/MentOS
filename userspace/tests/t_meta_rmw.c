/// @file t_meta_rmw.c
/// @brief Regression test for #344: a metadata block that could not be read
/// must not be written back.
/// @details Several ext2 functions read a metadata block into a cache, patch
/// one field and write the whole block back. None checked the read, and
/// `ext2_alloc_cache` zeroes what it returns — so a failed read left not stale
/// contents but *zeroes*, and writing those back destroyed everything else in
/// the block.
///
/// `ext2_write_inode` was the worst of them: one inode is patched into a block
/// that holds many, so with 4 KiB blocks and 128-byte inodes a single failed
/// read zeroed the other 31. It also returned 0 unconditionally, so an inode
/// that never reached the disk was reported as written.
///
/// This is what the fault injection of #338 was built for, and it needed the
/// injection to grow a sector target to be reachable at all: a bare count of
/// failures is spent by path resolution long before the interesting read
/// happens, which is why an earlier version of this test passed with the fix
/// reverted. The test therefore stats a file to learn which sector carries its
/// inode table, arms failures for that sector alone, and then creates a file
/// whose inode goes into it. Creation is the path that reaches the defect:
/// `ext2_create_inode` builds an inode from nothing and writes it, so the read
/// inside `ext2_write_inode` is the first read of that block. Modifying an
/// existing inode would not work, because `ext2_setattr` reads it first and
/// `ext2_read_inode` already checked its read. The other inodes in the block
/// have to survive.
///
/// Without ENABLE_ATA_FAULT_INJECTION there is no way to make the read fail,
/// so the test reports the facility as absent and passes.
/// @copyright (c) 2014-2026 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <strerror.h>
#include <string.h>
#include <sys/stat.h>
#include <syslog.h>
#include <unistd.h>

/// The control file of the fault injection.
#define CONTROL "/proc/faultinj"

/// How many neighbours to create. They are allocated consecutively, so they
/// share an inode table block with the file that gets modified.
#define NEIGHBOURS 8

/// The size each neighbour is given, so a zeroed inode is visible as a size of
/// zero rather than as a plausible value.
#define MARK_SIZE 7

/// Buffer for building the paths.
static char path[64];

/// @brief Builds the path of the nth neighbour.
/// @param n which neighbour.
static void __path_of(unsigned n) { sprintf(path, "/home/user/t_rmw_%u.bin", n); }

/// @brief Reads a counter out of the control file.
/// @param name the counter to read.
/// @param value where the value is stored.
/// @return 0 on success, -1 on failure.
static int __counter(const char *name, unsigned *value)
{
    int fd = open(CONTROL, O_RDONLY, 0);
    if (fd < 0) {
        return -1;
    }
    char buffer[256] = {0};
    ssize_t bytes    = read(fd, buffer, sizeof(buffer) - 1);
    close(fd);
    if (bytes <= 0) {
        return -1;
    }
    char *at = strstr(buffer, name);
    if (at == NULL) {
        syslog(LOG_ERR, "[t_meta_rmw] " CONTROL " does not report `%s`", name);
        return -1;
    }
    at += strlen(name);
    while (*at == ' ') {
        ++at;
    }
    *value = 0;
    for (; (*at >= '0') && (*at <= '9'); ++at) {
        *value = (*value * 10U) + (unsigned)(*at - '0');
    }
    return 0;
}

/// @brief Sends a command to the fault injection.
/// @param command what to send.
/// @return 0 on success, -1 on failure.
static int __arm(const char *command)
{
    int fd = open(CONTROL, O_WRONLY, 0);
    if (fd < 0) {
        return -1;
    }
    ssize_t written = write(fd, command, strlen(command));
    close(fd);
    return (written == (ssize_t)strlen(command)) ? 0 : -1;
}

int main(void)
{
    int probe = open(CONTROL, O_RDONLY, 0);
    if (probe < 0) {
        syslog(LOG_INFO, "[t_meta_rmw] " CONTROL " is absent: built without ENABLE_ATA_FAULT_INJECTION, nothing to do");
        return EXIT_SUCCESS;
    }
    close(probe);

    int failures = 0;

    // Create the neighbours and give each a known size.
    for (unsigned n = 0; n < NEIGHBOURS; ++n) {
        __path_of(n);
        int fd = creat(path, 0644);
        if (fd < 0) {
            syslog(LOG_ERR, "[t_meta_rmw] creating %s: %s", path, strerror(errno));
            return EXIT_FAILURE;
        }
        if (write(fd, "marker", MARK_SIZE) != MARK_SIZE) {
            syslog(LOG_ERR, "[t_meta_rmw] writing %s: %s", path, strerror(errno));
            close(fd);
            return EXIT_FAILURE;
        }
        close(fd);
    }

    // Learn which sector carries the inode table. Touching a neighbour makes
    // its inode be read, and last_read_sector then names the block that read
    // came from — which is the same block every one of these inodes lives in.
    __path_of(1);
    stat_t probe_st;
    if (stat(path, &probe_st) < 0) {
        syslog(LOG_ERR, "[t_meta_rmw] stat of %s to find the inode table: %s", path, strerror(errno));
        return EXIT_FAILURE;
    }
    unsigned table_sector = 0;
    if (__counter("last_read_sector", &table_sector) < 0) {
        return EXIT_FAILURE;
    }
    if (table_sector == 0) {
        syslog(LOG_ERR, "[t_meta_rmw] the injection reported no last read sector");
        return EXIT_FAILURE;
    }

    // Target the *first* sector of that block, not the last. A filesystem
    // block is eight 512-byte sectors, and ata_read fills the buffer up to the
    // sector that fails — so failing the last one leaves the first seven
    // eighths of the block intact and proves nothing. last_read_sector reports
    // the highest sector of the block just read, so the base is that with the
    // low three bits cleared.
    unsigned block_base = table_sector & ~7u;
    char command[64];
    // Skip one matching read. Every path that writes a metadata block reads
    // something from the same block first — here `ext2_find_direntry` reads the
    // parent directory's inode — and that read goes through `ext2_read_inode`,
    // which already checks its result. An unskipped failure therefore stops
    // the creation before it reaches the write-back under test, which is what
    // made three earlier versions of this control pass.
    sprintf(command, "read 4 sector %u skip 1", block_base);
    if (__arm(command) < 0) {
        syslog(LOG_ERR, "[t_meta_rmw] could not arm the injection");
        ++failures;
    } else {
        // Creating a file is what reaches the defect. `chmod` would not:
        // ext2_setattr reads the inode first, and ext2_read_inode *does* check
        // its read, so it fails before ext2_write_inode is ever called.
        // ext2_create_inode builds an inode from nothing and writes it, with no
        // prior read of the block it goes into — so the read inside
        // ext2_write_inode is the first one, and it is the one that used to be
        // discarded.
        int fd = creat("/home/user/t_rmw_new.bin", 0644);
        if (fd >= 0) {
            close(fd);
        }
        unsigned injected = 0;
        if ((__counter("reads_injected", &injected) == 0) && (injected == 0)) {
            syslog(
                LOG_ERR, "[t_meta_rmw] no failure was injected on sector %u; the test proved nothing", block_base);
            ++failures;
        }
        __arm("off");
        unlink("/home/user/t_rmw_new.bin");
    }

    // Whatever happened to the file that was being modified, the others share
    // its inode table block and must be untouched. A zeroed inode reads back
    // as size 0 and mode 0.
    for (unsigned n = 1; n < NEIGHBOURS; ++n) {
        __path_of(n);
        stat_t st;
        if (stat(path, &st) < 0) {
            syslog(LOG_ERR, "[t_meta_rmw] %s cannot be stat'd after the injection: %s", path, strerror(errno));
            ++failures;
            continue;
        }
        if ((unsigned)st.st_size != MARK_SIZE) {
            syslog(LOG_ERR, "[t_meta_rmw] %s is %u bytes, expected %u: its inode was overwritten", path, (unsigned)st.st_size, (unsigned)MARK_SIZE);
            ++failures;
        }
        if ((st.st_mode & 0777) == 0) {
            syslog(LOG_ERR, "[t_meta_rmw] %s has mode 0: its inode was zeroed", path);
            ++failures;
        }
    }

    for (unsigned n = 0; n < NEIGHBOURS; ++n) {
        __path_of(n);
        unlink(path);
    }

    if (failures == 0) {
        syslog(LOG_INFO, "[t_meta_rmw] a failed metadata read left the neighbouring inodes intact");
        return EXIT_SUCCESS;
    }
    syslog(LOG_ERR, "[t_meta_rmw] %d FAILURES", failures);
    return EXIT_FAILURE;
}
