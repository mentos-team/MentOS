/// @file test_vfs.c
/// @brief VFS and mount-table related tests.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

// Setup the logging for this file (do this before any other include).
#include "sys/kernel_levels.h"          // Include kernel log levels.
#define __DEBUG_HEADER__ "[TUNIT ]"     ///< Change header.
#define __DEBUG_LEVEL__  LOGLEVEL_NOTICE ///< Set log level.
#include "io/debug.h"                   // Include debugging functions.

#include "fcntl.h"
#include "fs/vfs.h"
#include "string.h"
#include "tests/test.h"
#include "tests/test_utils.h"

typedef struct {
    int mount_count;
    int has_root_mount;
    int has_proc_mount;
} test_mounts_ctx_t;

static int test_vfs_mounts_iter_cb(super_block_t *sb, void *ctx)
{
    if (!sb || !ctx) {
        return 1;
    }

    test_mounts_ctx_t *mounts_ctx = (test_mounts_ctx_t *)ctx;
    mounts_ctx->mount_count++;

    if (strcmp(sb->path, "/") == 0) {
        mounts_ctx->has_root_mount = 1;
    }
    if (strcmp(sb->path, "/proc") == 0) {
        mounts_ctx->has_proc_mount = 1;
    }

    return 0;
}

/// @brief Validate mounted superblock enumeration through the VFS iterator.
TEST(memory_vfs_mount_iterator)
{
    TEST_SECTION_START("VFS mount iterator");

    test_mounts_ctx_t ctx = {0};
    int ret = vfs_superblock_for_each(test_vfs_mounts_iter_cb, &ctx);

    ASSERT_MSG(ret == 0, "vfs_superblock_for_each must complete");
    ASSERT_MSG(ctx.mount_count > 0, "there must be at least one mounted filesystem");
    ASSERT_MSG(ctx.has_root_mount, "root mount '/' must exist");
    ASSERT_MSG(ctx.has_proc_mount, "proc mount '/proc' must exist");

    TEST_SECTION_END();
}

/// @brief Validate /proc/mounts output is readable and contains key mount points.
TEST(memory_vfs_proc_mounts_content)
{
    TEST_SECTION_START("/proc/mounts content");

    vfs_file_t *file = vfs_open("/proc/mounts", O_RDONLY, 0);
    ASSERT_MSG(file != NULL, "vfs_open('/proc/mounts') must succeed");

    char buffer[1024];
    memset(buffer, 0, sizeof(buffer));

    ssize_t read_count = vfs_read(file, buffer, 0, sizeof(buffer) - 1);
    ASSERT_MSG(read_count > 0, "vfs_read('/proc/mounts') must return data");
    buffer[read_count] = '\0';

    ASSERT_MSG(strstr(buffer, " / ") != NULL, "'/proc/mounts' must include root mountpoint");
    ASSERT_MSG(strstr(buffer, " /proc ") != NULL, "'/proc/mounts' must include /proc mountpoint");

    ASSERT_MSG(vfs_close(file) == 0, "vfs_close('/proc/mounts') must succeed");

    TEST_SECTION_END();
}

/// @brief Main test function for VFS subsystem.
void test_vfs(void)
{
    test_memory_vfs_mount_iterator();
    test_memory_vfs_proc_mounts_content();
}
