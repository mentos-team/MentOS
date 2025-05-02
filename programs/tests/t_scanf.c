/// @file t_scanf.c
/// @brief Test the scanf function.
/// @copyright (c) 2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <stdio.h>

int main(void)
{
    int number;
    char name[100];

    printf("Enter a number: ");
    if (scanf("%d", &number) != 1) {
        printf("Failed to read number.\n");
        return 1;
    }

    printf("Enter your name: ");
    if (scanf("%99s", name) != 1) { // %99s to prevent buffer overflow
        printf("Failed to read name.\n");
        return 1;
    }

    printf("Hello, %s! You entered %d.\n", name, number);
    return 0;
}