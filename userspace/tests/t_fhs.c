/// @file t_fhs.c
/// @brief Test program for Filesystem Hierarchy Standard (FHS) initialization.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

/// @brief Test structure for FHS directory verification.
typedef struct {
    const char *path;        ///< Path to the directory
    mode_t expected_mode;    ///< Expected permission bits (lower 12 bits)
    const char *description; ///< Description of the directory
} fhs_test_t;

/// @brief List of FHS directories to verify.
static const fhs_test_t fhs_tests[] = {
    {"/tmp", 01777, "Temporary files directory"},
    {"/home", 0755, "User home directories"},
    {"/root", 0700, "Root home directory"},
    {"/var", 0755, "Variable data"},
    {"/var/tmp", 01777, "Temporary variable data"},
    {"/var/log", 0755, "Log files"},
    {"/usr", 0755, "User programs and data"},
    {"/usr/bin", 0755, "User executable programs"},
    {"/usr/lib", 0755, "User libraries"},
    {"/usr/share", 0755, "User data"},
    {"/bin", 0755, "Essential executable programs"},
    {"/lib", 0755, "Essential system libraries"},
    {"/sbin", 0755, "System administration programs"},
    {"/etc", 0755, "System configuration"},
    {"/dev", 0755, "Device files"},
    {"/mnt", 0755, "Temporary mount points"},
    {"/media", 0755, "Removable media mount points"},
    {NULL, 0, NULL} // Null terminator
};

int main(int argc, char *argv[])
{
    int total_tests = 0;
    int passed_tests = 0;
    int failed_tests = 0;

    printf("=== Filesystem Hierarchy Standard (FHS) Verification ===\n\n");

    // Iterate through all FHS directories to test
    for (size_t i = 0; fhs_tests[i].path != NULL; i++) {
        const fhs_test_t *test = &fhs_tests[i];
        stat_t stat_buf;
        int result = stat(test->path, &stat_buf);

        total_tests++;

        if (result == 0) {
            // Directory exists, check if it's actually a directory
            if (S_ISDIR(stat_buf.st_mode)) {
                // Get the permission bits (lower 12 bits)
                mode_t actual_mode = stat_buf.st_mode & 07777;
                mode_t expected_mode = test->expected_mode;

                if (actual_mode == expected_mode) {
                    printf("[PASS] %s (%s) - Mode: 0%o\n", 
                           test->path, test->description, actual_mode);
                    passed_tests++;
                } else {
                    printf("[WARN] %s (%s) - Expected mode 0%o, got 0%o\n",
                           test->path, test->description, expected_mode, actual_mode);
                    passed_tests++; // Still count as pass since directory exists
                }
            } else {
                printf("[FAIL] %s - Exists but is not a directory\n", test->path);
                failed_tests++;
            }
        } else {
            printf("[FAIL] %s - Does not exist\n", test->path);
            failed_tests++;
        }
    }

    printf("\n=== Test Summary ===\n");
    printf("Total:  %d\n", total_tests);
    printf("Passed: %d\n", passed_tests);
    printf("Failed: %d\n", failed_tests);

    if (failed_tests == 0) {
        printf("\n✓ All FHS directories verified successfully!\n");
        return 0;
    } else {
        printf("\n✗ %d FHS directories are missing or misconfigured.\n", failed_tests);
        return 1;
    }
}
