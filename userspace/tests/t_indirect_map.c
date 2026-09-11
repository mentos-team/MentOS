/// @file t_indirect_map.c
/// @brief Regression test for #356: a block whose mapping could not be read
/// must not be reported as a hole.
/// @details `ext2_get_real_block_index` returned the block on the device as
/// its value, and 0 meant two opposite things: the file has a hole at that
/// position and must read as zeros (#192), or the index block that holds the
/// pointer could not be read. `ext2_read_inode_block` could only take the
/// first reading, so a file whose index block was unreadable came back as a
/// sparse file — a buffer of zeros and a successful `read`.
///
/// The mapping now returns 0/-errno and passes the index out through a
/// parameter, so the two answers are distinguishable and the failure reaches
/// the caller.
///
/// Reaching the defect needs the index block read to fail and the data block
/// read to succeed, so the failure has to be aimed at one specific sector.
/// The test works out which one:
///
///   1. it writes a file long enough to need the single-indirect block, with
///      a pattern that has no zero byte in it;
///   2. it finds the block size, by reading one byte at 1024, 2048 and 4096
///      and watching for the first offset that makes the driver touch a
///      different sector from the one offset 0 touches;
///   3. it reads block 12 — the first block reached through the indirect
///      block — and takes the last sector touched, which is the last sector
///      of that data block. `ext2_allocate_inode_block` allocates the data
///      block before the index block that points at it, so the index block is
///      the next one on the device: its first sector is that sector plus one.
///
/// Arming that sector fails the index block read and nothing else. The driver
/// retries a failed sector three times, so three failures are armed and all
/// three have to fire. Before the fix the read returned zeros and success; now
/// it fails. The test checks that the failures were really injected, so a
/// layout that breaks the assumption in step 3 is reported rather than passing
/// quietly.
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

/// The file the test works on.
#define TARGET "/home/user/t_indirect_map.bin"

/// How many direct block pointers an inode holds. The first block that needs
/// the indirect block is the one with this index.
#define DIRECT_BLOCKS 12

/// How long the file is. Big enough to pass the twelfth block for every ext2
/// block size, the largest of which is 4096.
#define FILE_SIZE (16 * 4096)

/// How much is written at a time while filling the file.
#define CHUNK 512

/// How many times the driver retries a failed sector, and so how many failures
/// have to be armed for one read to actually fail.
#define ATTEMPTS 3

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
        syslog(LOG_ERR, "[t_indirect_map] " CONTROL " does not report `%s`", name);
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

/// @brief Reads one byte at the given offset and reports the sector the driver
/// touched last while doing it.
/// @param fd the file to read.
/// @param offset where to read.
/// @param sector where the sector is stored.
/// @return 0 on success, -1 on failure.
static int __read_at(int fd, off_t offset, unsigned *sector)
{
    char byte = 0;
    if (lseek(fd, offset, SEEK_SET) < 0) {
        syslog(LOG_ERR, "[t_indirect_map] lseek to %ld: %s", offset, strerror(errno));
        return -1;
    }
    if (read(fd, &byte, 1) != 1) {
        syslog(LOG_ERR, "[t_indirect_map] read at %ld: %s", offset, strerror(errno));
        return -1;
    }
    return __counter("last_read_sector", sector);
}

/// @brief Works out the block size of the filesystem.
/// @param fd a file to read while probing.
/// @param block_size where the block size is stored.
/// @return 0 on success, -1 on failure.
/// @details Two offsets inside the same block make the driver touch the same
///          sectors, so the first offset that touches a different one is the
///          start of the second block, which is the block size.
static int __find_block_size(int fd, unsigned *block_size)
{
    static const unsigned candidates[] = {1024, 2048, 4096};
    unsigned first                     = 0;
    unsigned probe                     = 0;

    if (__read_at(fd, 0, &first) < 0) {
        return -1;
    }
    for (unsigned i = 0; i < (sizeof(candidates) / sizeof(candidates[0])); ++i) {
        if (__read_at(fd, (off_t)candidates[i], &probe) < 0) {
            return -1;
        }
        if (probe != first) {
            *block_size = candidates[i];
            return 0;
        }
    }
    syslog(LOG_ERR, "[t_indirect_map] no offset up to 4096 left the first block: cannot find the block size");
    return -1;
}

/// @brief Fills the file with a pattern that contains no zero byte.
/// @return 0 on success, -1 on failure.
static int __create_target(void)
{
    char chunk[CHUNK];
    int fd = open(TARGET, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        syslog(LOG_ERR, "[t_indirect_map] creating %s: %s", TARGET, strerror(errno));
        return -1;
    }
    for (unsigned written = 0; written < FILE_SIZE; written += CHUNK) {
        for (unsigned i = 0; i < CHUNK; ++i) {
            // Never zero, so a block of zeros can only come from the kernel.
            chunk[i] = (char)(((written + i) % 251U) + 1U);
        }
        if (write(fd, chunk, CHUNK) != CHUNK) {
            syslog(LOG_ERR, "[t_indirect_map] writing %s: %s", TARGET, strerror(errno));
            close(fd);
            return -1;
        }
    }
    close(fd);
    return 0;
}

int main(void)
{
    int probe = open(CONTROL, O_RDONLY, 0);
    if (probe < 0) {
        syslog(
            LOG_INFO, "[t_indirect_map] " CONTROL " is absent: built without ENABLE_ATA_FAULT_INJECTION, nothing to do");
        return EXIT_SUCCESS;
    }
    close(probe);

    if (__create_target() < 0) {
        return EXIT_FAILURE;
    }

    int failures = 0;
    int fd       = open(TARGET, O_RDONLY, 0);
    if (fd < 0) {
        syslog(LOG_ERR, "[t_indirect_map] opening %s: %s", TARGET, strerror(errno));
        unlink(TARGET);
        return EXIT_FAILURE;
    }

    unsigned block_size = 0;
    unsigned data_last  = 0;
    if ((__find_block_size(fd, &block_size) < 0) ||
        (__read_at(fd, (off_t)(DIRECT_BLOCKS * block_size), &data_last) < 0)) {
        close(fd);
        unlink(TARGET);
        return EXIT_FAILURE;
    }

    // The index block was allocated right after the data block it points at,
    // so it starts where that block ends.
    char command[64];
    sprintf(command, "read %d sector %u", ATTEMPTS, data_last + 1);
    if (__arm(command) < 0) {
        syslog(LOG_ERR, "[t_indirect_map] arming `%s`: %s", command, strerror(errno));
        close(fd);
        unlink(TARGET);
        return EXIT_FAILURE;
    }

    // Read the block that is reached through the index block. The index block
    // read is the one that fails; the data block is never reached.
    char buffer[16] = {0};
    ssize_t bytes   = -1;
    if (lseek(fd, (off_t)(DIRECT_BLOCKS * block_size), SEEK_SET) < 0) {
        syslog(LOG_ERR, "[t_indirect_map] lseek to the indirect block: %s", strerror(errno));
        ++failures;
    } else {
        bytes = read(fd, buffer, sizeof(buffer));
    }

    unsigned injected = 0;
    if (__counter("reads_injected", &injected) < 0) {
        ++failures;
    } else if (injected != ATTEMPTS) {
        syslog(
            LOG_ERR,
            "[t_indirect_map] the failures armed on sector %u fired %u times out of %d: the index block is not where "
            "the test expects it, so nothing was tested",
            data_last + 1, injected, ATTEMPTS);
        ++failures;
    } else if (bytes >= 0) {
        int all_zero = 1;
        for (ssize_t i = 0; i < bytes; ++i) {
            if (buffer[i] != 0) {
                all_zero = 0;
            }
        }
        syslog(
            LOG_ERR, "[t_indirect_map] the read of an unreadable index block returned %d bytes, %s", (int)bytes,
            all_zero ? "all zeros: the file was reported as sparse" : "of data that was never read");
        ++failures;
    }

    close(fd);
    if (__arm("off") < 0) {
        syslog(LOG_ERR, "[t_indirect_map] disarming: %s", strerror(errno));
        ++failures;
    }
    unlink(TARGET);

    if (failures == 0) {
        syslog(LOG_INFO, "[t_indirect_map] an index block that cannot be read fails the read instead of reading zeros");
        return EXIT_SUCCESS;
    }
    syslog(LOG_ERR, "[t_indirect_map] %d FAILURES", failures);
    return EXIT_FAILURE;
}
