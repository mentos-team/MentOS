/// @file echo.c
/// @brief
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    char *arg;

    // Iterate all words.
    while ((arg = *++argv) != NULL) {
        puts(arg);
        if (*(argv+1) != NULL) {
            putchar(' ');
        }
    }
    printf("\n");
    return 0;
}
