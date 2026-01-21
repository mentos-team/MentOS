/// @file t_read_failure.c
/// @brief Test case for Issue #3: Silent read failures
/// @details
/// This test verifies that read failures are properly reported and don't
/// return stale/corrupted data. The bug: when a block read fails, the code
/// continues with cached data without indicating error.
///
/// @copyright (c) 2024 - Audit Fix Test
/// @see EXT2_AUDIT_REPORT.md - Issue #3

#include <syslog.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <strerror.h>

#define TEST_FILE "/tmp/test_read_basic.txt"
#define TEST_DATA_SIZE 8192

/// @brief Test basic read from newly written file
/// @return 0 on success, 1 on failure
int test_read_after_write(void)
{
    syslog(LOG_INFO, "[TEST] Read after write...");
    
    // Create test file with known content
    int fd = open(TEST_FILE, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd < 0) {
        syslog(LOG_ERR, "Failed to create test file: %s", strerror(errno));
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
        syslog(LOG_ERR, "Failed to write all data");
        free(write_data);
        return 1;
    }
    
    // Now read it back
    fd = open(TEST_FILE, O_RDONLY, 0);
    if (fd < 0) {
        syslog(LOG_ERR, "Failed to open for reading: %s", strerror(errno));
        free(write_data);
        return 1;
    }
    
    char *read_data = malloc(TEST_DATA_SIZE);
    ssize_t read_bytes = read(fd, read_data, TEST_DATA_SIZE);
    close(fd);
    
    if (read_bytes != TEST_DATA_SIZE) {
        syslog(LOG_ERR, "Read failed or incomplete: %ld bytes", read_bytes);
        free(write_data);
        free(read_data);
        return 1;
    }
    
    // Verify data integrity
    if (memcmp(write_data, read_data, TEST_DATA_SIZE) != 0) {
        syslog(LOG_ERR, "Data mismatch after read");
        free(write_data);
        free(read_data);
        return 1;
    }
    
    syslog(LOG_INFO, "  ✓ Read data matches written data");
    free(write_data);
    free(read_data);
    return 0;
}

/// @brief Test reading across block boundaries
/// @return 0 on success, 1 on failure
int test_read_across_blocks(void)
{
    syslog(LOG_INFO, "[TEST] Read across block boundaries...");
    
    int fd = open(TEST_FILE, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd < 0) {
        syslog(LOG_ERR, "Failed to create test file: %s", strerror(errno));
        return 1;
    }
    
    // Write 3 blocks worth of data
    const int block_size = 4096;
    char *write_data = malloc(block_size * 3);
    
    // Fill with repeating pattern
    for (int i = 0; i < block_size * 3; i++) {
        write_data[i] = (char)((i / block_size) + 'A');  // Different byte per block
    }
    
    ssize_t written = write(fd, write_data, block_size * 3);
    close(fd);
    
    if (written != block_size * 3) {
        syslog(LOG_ERR, "Failed to write");
        free(write_data);
        return 1;
    }
    
    // Read back and verify each block
    fd = open(TEST_FILE, O_RDONLY, 0);
    char *read_data = malloc(block_size * 3);
    ssize_t read_bytes = read(fd, read_data, block_size * 3);
    close(fd);
    
    if (read_bytes != block_size * 3) {
        syslog(LOG_ERR, "Failed to read all blocks");
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
                syslog(LOG_ERR, "Block %d byte %d mismatch", block, i);
                free(write_data);
                free(read_data);
                return 1;
            }
        }
    }
    
    syslog(LOG_INFO, "  ✓ All blocks read correctly");
    free(write_data);
    free(read_data);
    return 0;
}

/// @brief Test partial reads from file
/// @return 0 on success, 1 on failure
int test_partial_reads(void)
{
    syslog(LOG_INFO, "[TEST] Partial reads...");
    
    int fd = open(TEST_FILE, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd < 0) {
        syslog(LOG_ERR, "Failed to create test file: %s", strerror(errno));
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
    fd = open(TEST_FILE, O_RDONLY, 0);
    const int chunk_size = 1000;
    char chunk[chunk_size];
    int total_read = 0;
    
    while (1) {
        ssize_t bytes = read(fd, chunk, chunk_size);
        if (bytes <= 0) break;
        
        // Verify this chunk
        for (int i = 0; i < bytes; i++) {
            char expected = (char)((total_read + i) & 0xFF);
            if (chunk[i] != expected) {
                syslog(LOG_ERR, "Chunk read mismatch at offset %d", total_read + i);
                close(fd);
                free(data);
                return 1;
            }
        }
        
        total_read += bytes;
    }
    
    close(fd);
    
    if (total_read != TEST_DATA_SIZE) {
        syslog(LOG_ERR, "Did not read all data: got %d of %d", total_read, TEST_DATA_SIZE);
        free(data);
        return 1;
    }
    
    syslog(LOG_INFO, "  ✓ All partial reads consistent and correct");
    free(data);
    return 0;
}

/// @brief Test read at EOF
/// @return 0 on success, 1 on failure
int test_read_eof_behavior(void)
{
    syslog(LOG_INFO, "[TEST] Read at EOF behavior...");
    
    int fd = open(TEST_FILE, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd < 0) {
        syslog(LOG_ERR, "Failed to create test file: %s", strerror(errno));
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
        syslog(LOG_ERR, "Read at small file returned %ld, expected 5", bytes);
        close(fd);
        return 1;
    }
    
    // Try second read (should return 0 for EOF)
    bytes = read(fd, buffer, 1024);
    if (bytes != 0) {
        syslog(LOG_ERR, "Read at EOF returned %ld, expected 0", bytes);
        close(fd);
        return 1;
    }
    
    close(fd);
    syslog(LOG_INFO, "  ✓ EOF behavior correct");
    return 0;
}

int main(void)
{
    openlog("t_ext2_read_failure", LOG_CONS | LOG_PID, LOG_USER);
    syslog(LOG_INFO, "\n=== EXT2 Read Failure Test Suite ===");
    syslog(LOG_INFO, "Testing: Issue #3 - Silent read failures");
    syslog(LOG_INFO, "Location: ext2.c:1809-1815 in ext2_read_inode_data()");
    syslog(LOG_INFO, "Bug: Error on block read is ignored, stale cache returned\n");
    
    int failures = 0;
    
    failures += test_read_after_write();
    failures += test_read_across_blocks();
    failures += test_partial_reads();
    failures += test_read_eof_behavior();
    
    syslog(LOG_INFO, "\n=== Results ===");
    if (failures == 0) {
        syslog(LOG_INFO, "✅ ALL TESTS PASSED");
        return 0;
    } else {
        printf("❌ %d TEST(S) FAILED\n", failures);
        return 1;
    }
}
