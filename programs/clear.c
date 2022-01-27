/// @file clear.c
/// @brief `clear` program.
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <stdio.h>

int main(int argc, char **argv)
{
    puts("\033[J");
    return 0;
}
