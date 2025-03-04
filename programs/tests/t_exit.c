/// @file t_exit.c
/// @brief Test program for the exit system call.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[])
{
    // Simple test to show successful exit.
    exit(EXIT_SUCCESS);

    // This code should not be reached.
    return EXIT_FAILURE;
}
