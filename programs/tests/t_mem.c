/// @file t_mem.c
/// @brief
/// @copyright (c) 2014-2023 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "io/debug.h"
#include "time.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/unistd.h>
#include <sys/wait.h>

int main(int argc, char *argv[])
{
    if (argc != 3) {
        printf("%s: You must provide 2 dimensions.\n", argv[0]);
        return 1;
    }
    char *ptr;
    int rows = strtol(argv[1], &ptr, 10);
    int cols = strtol(argv[2], &ptr, 10);

    printf("Allocating memory!\n");
    pr_warning("Allocating memory!\n");
    int **M = (int **)malloc(rows * sizeof(int *));
    for (int i = 0; i < rows; ++i)
        M[i] = (int *)malloc(cols * sizeof(int));

    sleep(5);
    printf("Writing memory!\n");
    pr_warning("Writing memory!\n");
    for (int i = 0; i < rows; ++i)
        for (int j = 0; j < cols; ++j)
            M[i][j] = i + j;

    sleep(5);
    printf("Freeing memory (1)!\n");
    pr_warning("Freeing memory (1)!\n");
    for (int i = 0; i < rows; ++i)
        if (i % 2 != 0)
            free(M[i]);
    printf("Freeing memory (2)!\n");
    pr_warning("Freeing memory (2)!\n");
    for (int i = 0; i < rows; ++i)
        if (i % 2 == 0)
            free(M[i]);
    free(M);

    sleep(5);
    pr_warning("Exiting!\n");
    return 0;
}
