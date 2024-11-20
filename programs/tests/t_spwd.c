/// @file t_spwd.c
/// @brief This program generates a SHA-256 hash for a given key or retrieves
/// hashed user password information from the shadow password database.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <shadow.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <time.h>

#include <crypt/sha256.h>

/// @brief Generates a SHA-256 hash for a predefined input string.
/// @return int EXIT_SUCCESS on success, EXIT_FAILURE on error.
int test_generate(void)
{
    // Buffer to hold the hashed output.
    unsigned char buffer[SHA256_BLOCK_SIZE] = { 0 };

    // Input string to be hashed.
    const char input[] = "Knowledge is power, but enthusiasm pulls the switch.";

    // Buffer to hold the hexadecimal representation of the hash.
    char output[SHA256_BLOCK_SIZE * 2 + 1] = { 0 };

    // Input string to be hashed.
    const char expected[] = "6a1399bdcf1fa1ced3d7148a3f5472a5105ff30f730069fc8bdb73bdc018cb42";

    // SHA-256 context.
    SHA256_ctx_t ctx;

    // Initialize the SHA-256 context.
    sha256_init(&ctx);

    // Perform the hashing operation 100 times for security.
    for (unsigned i = 0; i < 100; ++i) {
        sha256_update(&ctx, (unsigned char *)input, strlen(input));
    }

    // Finalize the hashing.
    sha256_final(&ctx, buffer);

    // Convert the hash bytes to a hexadecimal string.
    sha256_bytes_to_hex(buffer, SHA256_BLOCK_SIZE, output, SHA256_BLOCK_SIZE * 2 + 1);

    // Check if both hashes match.
    if (strncmp(output, expected, strlen(expected))) {
        fprintf(stderr, "Hashes do not match:\n");
        fprintf(stderr, "Input    : `%s`\n", input);
        fprintf(stderr, "Output   : `%s`\n", output);
        fprintf(stderr, "Expected : `%s`\n", expected);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

int test_getspnam(void)
{
    const char *username = "root";

    // Retrieve the shadow password entry for the user.
    struct spwd *spbuf = getspnam(username);

    // Check if the user exists in the shadow password database.
    if (!spbuf) {
        fprintf(stderr, "Failed to find user '%s' in the shadow password database.\n", username);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

int main(int argc, char *argv[])
{
    if (test_generate() == EXIT_FAILURE) {
        return EXIT_FAILURE;
    }
    if (test_getspnam() == EXIT_FAILURE) {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
