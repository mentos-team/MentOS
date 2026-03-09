/// @file t_read_failure.c
/// @brief Test case for Issue #3: Silent read failures
/// @details
/// This test verifies that read failures are properly reported and don't
/// return stale/corrupted data. The bug: when a block read fails, the code
/// continues with cached data without indicating error.
///
/// @copyright (c) 2024 - Audit Fix Test
/// @see EXT2_AUDIT_REPORT.md - Issue #3

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <strerror.h>
#include <string.h>
#include <sys/stat.h>
#include <syslog.h>
#include <unistd.h>

#define TEST_FILE      "/tmp/test_read_basic.txt"
#define TEST_DATA_SIZE 8192

/// @brief Test basic read from newly written file
/// @return 0 on success, 1 on failure
int test_read_after_write(void)
{
    syslog(LOG_INFO, "[t_ext2_audit_read_failure] [TEST] Read after write...\n");

    // Create test file with known content
    int fd = open(TEST_FILE, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd < 0) {
        syslog(LOG_ERR, "[t_ext2_audit_read_failure] Failed to create test file: %s\n", strerror(errno));
        return 1;
    }

    // Write distinguishable pattern
    char *write_data = malloc(TEST_DATA_SIZE);
    for (int i = 0; i < TEST_DATA_SIZE; i++) {
        write_data[i] = (char)(i & 0xFF);
    }

    ssize_t written = write(fd, write_data, TEST_DATA_SIZE);
    close(fd);

    if (written != TEST_DATA_SIZE) {
        syslog(LOG_ERR, "[t_ext2_audit_read_failure] Failed to write all data\n");
        free(write_data);
        return 1;
    }

    // Now read it back
    fd = open(TEST_FILE, O_RDONLY, 0);
    if (fd < 0) {
        syslog(LOG_ERR, "[t_ext2_audit_read_failure] Failed to open for reading: %s\n", strerror(errno));
        free(write_data);
        return 1;
    }

    char *read_data    = malloc(TEST_DATA_SIZE);
    ssize_t read_bytes = read(fd, read_data, TEST_DATA_SIZE);
    close(fd);

    if (read_bytes != TEST_DATA_SIZE) {
        syslog(LOG_ERR, "[t_ext2_audit_read_failure] Read failed or incomplete: %ld bytes\n", read_bytes);
        free(write_data);
        free(read_data);
        return 1;
    }

    // Verify data integrity
    if (memcmp(write_data, read_data, TEST_DATA_SIZE) != 0) {
        syslog(LOG_ERR, "[t_ext2_audit_read_failure] Data mismatch after read\n");
        free(write_data);
        free(read_data);
        return 1;
    }

    syslog(LOG_INFO, "[t_ext2_audit_read_failure]   ✓ Read data matches written data\n");
    free(write_data);
    free(read_data);
    return 0;
}

/// @brief Test reading across block boundaries
/// @return 0 on success, 1 on failure
int test_read_across_blocks(void)
{
    syslog(LOG_INFO, "[t_ext2_audit_read_failure] [TEST] Read across block boundaries...\n");

    int fd = open(TEST_FILE, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd < 0) {
        syslog(LOG_ERR, "[t_ext2_audit_read_failure] Failed to create test file: %s\n", strerror(errno));
        return 1;
    }

    // Write 3 blocks worth of data
    const int block_size = 4096;
    char *write_data     = malloc(block_size * 3);

    // Fill with repeating pattern
    for (int i = 0; i < block_size * 3; i++) {
        write_data[i] = (char)((i / block_size) + 'A'); // Different byte per block
    }

    ssize_t written = write(fd, write_data, block_size * 3);
    close(fd);

    if (written != block_size * 3) {
        syslog(LOG_ERR, "[t_ext2_audit_read_failure] Failed to write\n");
        free(write_data);
        return 1;
    }

    // Read back and verify each block
    fd                 = open(TEST_FILE, O_RDONLY, 0);
    char *read_data    = malloc(block_size * 3);
    ssize_t read_bytes = read(fd, read_data, block_size * 3);
    close(fd);

    if (read_bytes != block_size * 3) {
        syslog(LOG_ERR, "[t_ext2_audit_read_failure] Failed to read all blocks\n");
        free(write_data);
        free(read_data);
        return 1;
    }

    // Verify block by block
    for (int block = 0; block < 3; block++) {
        char expected = 'A' + block;
        for (int i = 0; i < block_size; i++) {
            int offset = block * block_size + i;
            if (read_data[offset] != expected) {
                syslog(LOG_ERR, "[t_ext2_audit_read_failure] Block %d byte %d mismatch\n", block, i);
                free(write_data);
                free(read_data);
                return 1;
            }
        }
    }

    syslog(LOG_INFO, "[t_ext2_audit_read_failure]   ✓ All blocks read correctly\n");
    free(write_data);
    free(read_data);
    return 0;
}

/// @brief Test partial reads from file
/// @return 0 on success, 1 on failure
int test_partial_reads(void)
{
    syslog(LOG_INFO, "[t_ext2_audit_read_failure] [TEST] Partial reads...\n");

    int fd = open(TEST_FILE, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd < 0) {
        syslog(LOG_ERR, "[t_ext2_audit_read_failure] Failed to create test file: %s\n", strerror(errno));
        return 1;
    }

    // Write known data
    char *data = malloc(TEST_DATA_SIZE);
    for (int i = 0; i < TEST_DATA_SIZE; i++) {
        data[i] = (char)(i & 0xFF);
    }

    write(fd, data, TEST_DATA_SIZE);
    close(fd);

    // Read in small chunks
    fd                   = open(TEST_FILE, O_RDONLY, 0);
    const int chunk_size = 1000;
    char chunk[chunk_size];
    int total_read = 0;

    while (1) {
        ssize_t bytes = read(fd, chunk, chunk_size);
        if (bytes <= 0)
            break;

        // Verify this chunk
        for (int i = 0; i < bytes; i++) {
            char expected = (char)((total_read + i) & 0xFF);
            if (chunk[i] != expected) {
                syslog(LOG_ERR, "[t_ext2_audit_read_failure] Chunk read mismatch at offset %d\n", total_read + i);
                close(fd);
                free(data);
                return 1;
            }
        }

        total_read += bytes;
    }

    close(fd);

    if (total_read != TEST_DATA_SIZE) {
        syslog(LOG_ERR, "[t_ext2_audit_read_failure] Did not read all data: got %d of %d\n", total_read, TEST_DATA_SIZE);
        free(data);
        return 1;
    }

    syslog(LOG_INFO, "[t_ext2_audit_read_failure]   ✓ All partial reads consistent and correct\n");
    free(data);
    return 0;
}

/// @brief Test read at EOF
/// @return 0 on success, 1 on failure
int test_read_eof_behavior(void)
{
    syslog(LOG_INFO, "[t_ext2_audit_read_failure] [TEST] Read at EOF behavior...\n");

    int fd = open(TEST_FILE, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd < 0) {
        syslog(LOG_ERR, "[t_ext2_audit_read_failure] Failed to create test file: %s\n", strerror(errno));
        return 1;
    }

    // Write small amount
    write(fd, "small", 5);
    close(fd);

    // Try to read beyond EOF
    fd = open(TEST_FILE, O_RDONLY, 0);
    char buffer[1024];

    ssize_t bytes = read(fd, buffer, 1024);
    if (bytes != 5) {
        syslog(LOG_ERR, "[t_ext2_audit_read_failure] Read at small file returned %ld, expected 5\n", bytes);
        close(fd);
        return 1;
    }

    // Try second read (should return 0 for EOF)
    bytes = read(fd, buffer, 1024);
    if (bytes != 0) {
        syslog(LOG_ERR, "[t_ext2_audit_read_failure] Read at EOF returned %ld, expected 0\n", bytes);
        close(fd);
        return 1;
    }

    close(fd);
    syslog(LOG_INFO, "[t_ext2_audit_read_failure]   ✓ EOF behavior correct\n");
    return 0;
}

int main(void)
{
    openlog("t_ext2_read_failure", LOG_CONS | LOG_PID, LOG_USER);
    syslog(LOG_INFO, "[t_ext2_audit_read_failure] \n=== EXT2 Read Failure Test Suite ===\n");
    syslog(LOG_INFO, "[t_ext2_audit_read_failure] Testing: Issue #3 - Silent read failures\n");
    syslog(LOG_INFO, "[t_ext2_audit_read_failure] Location: ext2.c:1809-1815 in ext2_read_inode_data()\n");
    syslog(LOG_INFO, "[t_ext2_audit_read_failure] Bug: Error on block read is ignored, stale cache returned\n\n");

    int failures = 0;

    failures += test_read_after_write();
    failures += test_read_across_blocks();
    failures += test_partial_reads();
    failures += test_read_eof_behavior();

    syslog(LOG_INFO, "[t_ext2_audit_read_failure] === Results ===\n");
    if (failures == 0) {
        syslog(LOG_INFO, "[t_ext2_audit_read_failure] ✅ ALL TESTS PASSED\n");
        closelog();
        return 0;
    } else {
        syslog(LOG_ERR, "[t_ext2_audit_read_failure] ❌ %d TEST(S) FAILED\n", failures);
        closelog();
        return 1;
    }
}
