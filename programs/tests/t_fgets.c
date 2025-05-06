/// @file t_fgets.c
/// @brief Test the fgets function with STDIN_FILENO.
/// @copyright (c) 2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <stdio.h>
#include <string.h>
#include <unistd.h>

int main(void)
{
    char buffer[128];

    printf("Enter a line of text (including spaces): ");
    if (fgets(buffer, sizeof(buffer), STDIN_FILENO) == NULL) {
        printf("Failed to read line.\n");
        return 1;
    }

    // Remove trailing newline if present
    size_t len = strlen(buffer);
    if (len > 0 && buffer[len - 1] == '\n') {
        buffer[len - 1] = '\0';
    }

    printf("You entered: \"%s\"\n", buffer);
    return 0;
}
