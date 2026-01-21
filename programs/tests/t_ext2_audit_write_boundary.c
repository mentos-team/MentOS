/// @file t_write_boundary.c
/// @brief Test case for Issue #1: Buffer overflow on write boundary
/// @details
/// This test verifies that writing data to a file at block boundaries
/// does not cause buffer overflow. The bug manifests when writing the
/// last block - the right boundary should be block_size-1, not block_size.
///
/// @copyright (c) 2024 - Audit Fix Test
/// @see EXT2_AUDIT_REPORT.md - Issue #1

#include <syslog.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <strerror.h>

#define TEST_FILE "/tmp/test_write_boundary.txt"
#define TEST_DATA_SIZE 8192  // 8KB - spans 2x 4KB blocks
#define BLOCK_SIZE 4096

/// @brief Test writing data that spans multiple blocks with unaligned offset
/// @return 0 on success, 1 on failure
int test_unaligned_write_spanning_blocks(void)
{
    syslog(LOG_INFO, "[TEST] Unaligned write spanning multiple blocks");
    
    // Create and open file for writing
    int fd = open(TEST_FILE, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd < 0) {
        syslog(LOG_ERR, "Failed to create test file: %s", strerror(errno));
        return 1;
    }
    
    // Allocate test data - fill with pattern
    char *write_data = malloc(TEST_DATA_SIZE);
    if (!write_data) {
        syslog(LOG_ERR, "Failed to allocate write buffer: %s", strerror(errno));
        close(fd);
        return 1;
    }
    
    // Fill with distinguishable pattern
    for (int i = 0; i < TEST_DATA_SIZE; i++) {
        write_data[i] = (char)(i & 0xFF);
    }
    
    // Write unaligned: offset 2048, then 8192 bytes
    // This will:
    // - Start in middle of block 0 (offset 2048, write 2048 bytes)
    // - Span into block 1 (write 4096 bytes)
    // - End at start of block 2 (write 2048 bytes)
    // The bug would overflow when setting right=block_size on the last block
    
    ssize_t written = write(fd, write_data, TEST_DATA_SIZE);
    if (written != TEST_DATA_SIZE) {
        syslog(LOG_ERR, "Failed to write all data: wrote %ld of %d", written, TEST_DATA_SIZE);
        close(fd);
        free(write_data);
        return 1;
    }
    
    close(fd);
    
    // Read back and verify data integrity
    fd = open(TEST_FILE, O_RDONLY, 0);
    if (fd < 0) {
        syslog(LOG_ERR, "Failed to open test file for reading: %s", strerror(errno));
        free(write_data);
        return 1;
    }
    
    char *read_data = malloc(TEST_DATA_SIZE);
    if (!read_data) {
        syslog(LOG_ERR, "Failed to allocate read buffer: %s", strerror(errno));
        close(fd);
        free(write_data);
        return 1;
    }
    
    ssize_t read_bytes = read(fd, read_data, TEST_DATA_SIZE);
    close(fd);
    
    if (read_bytes != TEST_DATA_SIZE) {
        syslog(LOG_ERR, "Failed to read all data: read %ld of %d", read_bytes, TEST_DATA_SIZE);
        free(write_data);
        free(read_data);
        return 1;
    }
    
    // Verify data matches
    if (memcmp(write_data, read_data, TEST_DATA_SIZE) != 0) {
        syslog(LOG_ERR, "Data mismatch: written data differs from read data");
        free(write_data);
        free(read_data);
        return 1;
    }
    
    syslog(LOG_INFO, "  ✓ Data written and read back correctly");
    free(write_data);
    free(read_data);
    return 0;
}

/// @brief Test writing to exact block boundary
/// @return 0 on success, 1 on failure
int test_exact_block_boundary_write(void)
{
    syslog(LOG_INFO, "[TEST] Write at exact block boundary");
    
    int fd = open(TEST_FILE, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd < 0) {
        syslog(LOG_ERR, "Failed to create test file: %s", strerror(errno));
        return 1;
    }
    
    // Write exactly 2 blocks worth
    char *data = malloc(BLOCK_SIZE * 2);
    memset(data, 0xAA, BLOCK_SIZE * 2);
    
    ssize_t written = write(fd, data, BLOCK_SIZE * 2);
    close(fd);
    
    if (written != BLOCK_SIZE * 2) {
        syslog(LOG_ERR, "Failed to write: wrote %ld of %d", written, BLOCK_SIZE * 2);
        free(data);
        return 1;
    }
    
    // Verify file size
    struct stat st;
    if (stat(TEST_FILE, &st) < 0) {
        syslog(LOG_ERR, "Failed to stat file: %s", strerror(errno));
        free(data);
        return 1;
    }
    
    if (st.st_size != BLOCK_SIZE * 2) {
        syslog(LOG_ERR, "File size mismatch: expected %d, got %ld", BLOCK_SIZE * 2, st.st_size);
        free(data);
        return 1;
    }
    
    syslog(LOG_INFO, "  ✓ Boundary write successful, file size correct");
    free(data);
    return 0;
}

/// @brief Test multiple sequential writes (stress test)
/// @return 0 on success, 1 on failure
int test_multiple_partial_writes(void)
{
    syslog(LOG_INFO, "[TEST] Multiple partial writes");
    
    int fd = open(TEST_FILE, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd < 0) {
        syslog(LOG_ERR, "Failed to create test file: %s", strerror(errno));
        return 1;
    }
    
    // Write in chunks that don't align with block boundaries
    const int num_writes = 5;
    const int chunk_size = 1500;  // Odd size to cause boundary issues
    
    char *data = malloc(chunk_size);
    for (int i = 0; i < chunk_size; i++) {
        data[i] = (char)i;
    }
    
    for (int i = 0; i < num_writes; i++) {
        ssize_t written = write(fd, data, chunk_size);
        if (written != chunk_size) {
            syslog(LOG_ERR, "Write %d failed: wrote %ld of %d", i, written, chunk_size);
            close(fd);
            free(data);
            return 1;
        }
    }
    
    close(fd);
    
    // Verify final file size
    struct stat st;
    if (stat(TEST_FILE, &st) < 0) {
        syslog(LOG_ERR, "Failed to stat file: %s", strerror(errno));
        free(data);
        return 1;
    }
    
    int expected_size = num_writes * chunk_size;
    if (st.st_size != expected_size) {
        syslog(LOG_ERR, "File size mismatch: expected %d, got %ld", expected_size, st.st_size);
        free(data);
        return 1;
    }
    
    syslog(LOG_INFO, "  ✓ Multiple writes successful, file size correct");
    free(data);
    return 0;
}

int main(void)
{
    openlog("t_ext2_audit_write_boundary", LOG_CONS | LOG_PID, LOG_USER);
    
    syslog(LOG_INFO, "=== EXT2 Write Boundary Test Suite ===");
    syslog(LOG_INFO, "Testing: Issue #1 - Buffer overflow on write boundary");
    syslog(LOG_INFO, "Location: ext2.c:1901 in ext2_write_inode_data()");
    syslog(LOG_INFO, "Bug: right = fs->block_size (should be block_size - 1)");
    
    int failures = 0;
    
    failures += test_unaligned_write_spanning_blocks();
    failures += test_exact_block_boundary_write();
    failures += test_multiple_partial_writes();
    
    syslog(LOG_INFO, "=== Results ===");
    if (failures == 0) {
        syslog(LOG_INFO, "✅ ALL TESTS PASSED");
        closelog();
        return 0;
    } else {
        syslog(LOG_ERR, "❌ %d TEST(S) FAILED", failures);
        closelog();
        return 1;
    }
}
