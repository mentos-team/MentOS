/// @file t_mem.c
/// @brief Memory allocation, writing, and deallocation example.
/// @details This program allocates memory for a 2D array, writes to it, and
/// then frees the memory. It demonstrates basic dynamic memory management in C,
/// including error handling for memory allocation failures.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    // Define dimensions for the 2D array (can be modified for testing).
    const int rows = 100; // Number of rows.
    const int cols = 100; // Number of columns.

    // Allocate memory for an array of row pointers (rows x cols matrix).
    int **M = (int **)malloc(rows * sizeof(int *));
    if (M == NULL) {
        fprintf(stderr, "Failed to allocate memory for row pointers.\n");
        return EXIT_FAILURE;
    }

    // Allocate memory for each row (array of integers) and check for allocation errors.
    for (int i = 0; i < rows; ++i) {
        M[i] = (int *)malloc(cols * sizeof(int));
        if (M[i] == NULL) {
            fprintf(stderr, "Failed to allocate memory for row %d.\n", i);

            // Free any previously allocated memory to prevent memory leaks.
            for (int j = 0; j < i; ++j) {
                free(M[j]);
            }
            free(M);
            return EXIT_FAILURE;
        }
    }

    // Write values to the 2D array (fill with i + j).
    for (int i = 0; i < rows; ++i) {
        for (int j = 0; j < cols; ++j) {
            M[i][j] = i + j;
        }
    }

    // Free the memory for odd-indexed rows.
    for (int i = 0; i < rows; ++i) {
        if (i % 2 != 0) {
            free(M[i]);
            M[i] = NULL; // Nullify pointer after freeing.
        }
    }

    // Free the memory for even-indexed rows.
    for (int i = 0; i < rows; ++i) {
        if (i % 2 == 0) {
            free(M[i]);
            M[i] = NULL; // Nullify pointer after freeing.
        }
    }

    // Free the memory allocated for the row pointers array.
    free(M);
    M = NULL; // Nullify pointer after freeing.

    return EXIT_SUCCESS;
}
