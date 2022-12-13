/// @file t_mem.c
/// @brief
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <stdio.h>
#include <stdlib.h>
#include <sys/unistd.h>
#include <sys/wait.h>

int main(int argc, char *argv[])
{
    if (argc != 2) {
        return 1;
    }
    char *ptr;
    int N = strtol(argv[1], &ptr, 10), *V;
    for (int i = 0; i < N; ++i) {
        for (int j = 1; j < N; ++j) {
            V = (int *)malloc(j * sizeof(int));
            free(V);
        }
    }
    return 0;
}
