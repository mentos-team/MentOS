/// @file t_spwd.c
/// @brief This program generates a SHA-256 hash for a given key or retrieves
/// hashed user password information from the shadow password database.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <shadow.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <time.h>

#include <crypt/sha256.h>

int main(int argc, char *argv[])
{
    // Check if the correct number of arguments is provided.
    if (argc != 3) {
        printf("You can either:\n");
        printf("    -g, --generate <key> : prints the hashed key.\n");
        printf("    -l, --load <user>    : prints the hashed key stored for the given user.\n");
        return EXIT_FAILURE;
    }

    // Generate SHA-256 hash for the provided key.
    if (!strcmp(argv[1], "--generate") || !strcmp(argv[1], "-g")) {
        // Buffer to hold the hashed output.
        unsigned char buffer[SHA256_BLOCK_SIZE] = { 0 };
        // Buffer to hold the hexadecimal representation of the hash.
        char output[SHA256_BLOCK_SIZE * 2 + 1] = { 0 };
        // SHA-256 context.
        SHA256_ctx_t ctx;

        // Initialize the SHA-256 context.
        sha256_init(&ctx);

        // Perform the hashing operation 100,000 times for security.
        for (unsigned i = 0; i < 100000; ++i) {
            sha256_update(&ctx, (unsigned char *)argv[2], strlen(argv[2]));
        }

        // Finalize the hashing.
        sha256_final(&ctx, buffer);

        // Convert the hash bytes to a hexadecimal string.
        sha256_bytes_to_hex(buffer, SHA256_BLOCK_SIZE, output, SHA256_BLOCK_SIZE * 2 + 1);

        // Print the hashed key.
        printf("%s\n", output);

    }
    // Load and display information for the specified user from the shadow password database.
    else if (!strcmp(argv[1], "--load") || !strcmp(argv[1], "-l")) {
        // Retrieve the shadow password entry for the user.
        struct spwd *spbuf = getspnam(argv[2]);

        // Check if the user exists in the shadow password database.
        if (spbuf) {
            // Retrieve the last change time.
            time_t lstchg = (time_t)spbuf->sp_lstchg;
            // Convert to local time structure.
            tm_t *tm = localtime(&lstchg);

            // Print the user information (month is zero-based).
            printf("name         : %s\n", spbuf->sp_namp);
            printf("password     :\n%s\n", spbuf->sp_pwdp);
            printf("lastchange   : %02d/%02d %02d:%02d\n", tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min);
            printf("days allowed : %d\n", spbuf->sp_min);
            printf("days req.    : %d\n", spbuf->sp_max);
            printf("days warning : %d\n", spbuf->sp_warn);
            printf("days inact   : %d\n", spbuf->sp_inact);
            printf("days expire  : %d\n", spbuf->sp_expire);
        } else {
            printf("User '%s' not found in the shadow password database.\n", argv[2]);
            return EXIT_FAILURE;
        }
    } else {
        printf("Invalid option. Use -g/--generate or -l/--load.\n");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
