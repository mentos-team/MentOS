/// @file sleep.c
/// @brief
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <stdio.h>
#include <time.h>

int main(int argc, char **argv)
{
    if (argc == 2) {
        int amount = atoi(argv[1]);
        if (amount > 0) {
            sleep(amount);
        }
    }
    return 0;
}
