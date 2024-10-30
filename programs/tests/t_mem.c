/// @file t_mem.c
/// @brief Memory allocation, writing, and deallocation example.
/// @details This program allocates memory for a 2D array, writes to it, and
/// then frees the memory. It demonstrates basic dynamic memory management in C,
/// including error handling for memory allocation failures.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <io/debug.h>
#include <time.h>

int main(int argc, char *argv[])
{
    // Ensure the program is provided with exactly 2 arguments (row and column size)
    if (argc != 3) {
        fprintf(STDERR_FILENO, "%s: You must provide 2 dimensions (rows and columns).\n", argv[0]);
        return EXIT_FAILURE;
    }

    char *ptr;
    // Convert the first argument (rows) to an integer and validate it
    int rows = strtol(argv[1], &ptr, 10);
    if (*ptr != '\0' || rows <= 0) {
        fprintf(STDERR_FILENO, "Invalid number of rows: %s\n", argv[1]);
        return EXIT_FAILURE;
    }

    // Convert the second argument (columns) to an integer and validate it
    int cols = strtol(argv[2], &ptr, 10);
    if (*ptr != '\0' || cols <= 0) {
        fprintf(STDERR_FILENO, "Invalid number of columns: %s\n", argv[2]);
        return EXIT_FAILURE;
    }

    printf("Allocating memory!\n");
    pr_warning("Allocating memory!\n");

    // Allocate memory for an array of row pointers (rows x cols matrix)
    int **M = (int **)malloc(rows * sizeof(int *));
    if (M == NULL) {
        perror("Failed to allocate memory for row pointers");
        return EXIT_FAILURE;
    }

    // Allocate memory for each row (array of integers) and check for allocation errors
    for (int i = 0; i < rows; ++i) {
        M[i] = (int *)malloc(cols * sizeof(int));
        if (M[i] == NULL) {
            perror("Failed to allocate memory for columns");
            // Free any previously allocated memory to prevent memory leaks
            for (int j = 0; j < i; ++j) {
                free(M[j]);
            }
            free(M);
            return EXIT_FAILURE;
        }
    }

    // Simulate delay for demonstration purposes
    sleep(5);
    printf("Writing memory!\n");
    pr_warning("Writing memory!\n");

    // Write values to the 2D array (fill with i + j)
    for (int i = 0; i < rows; ++i) {
        for (int j = 0; j < cols; ++j) {
            M[i][j] = i + j;
        }
    }

    // Simulate delay for demonstration purposes
    sleep(5);
    printf("Freeing memory (1)!\n");
    pr_warning("Freeing memory (1)!\n");

    // Free the memory for odd-indexed rows
    for (int i = 0; i < rows; ++i) {
        if (i % 2 != 0) {
            free(M[i]);
        }
    }

    printf("Freeing memory (2)!\n");
    pr_warning("Freeing memory (2)!\n");

    // Free the memory for even-indexed rows
    for (int i = 0; i < rows; ++i) {
        if (i % 2 == 0) {
            free(M[i]);
        }
    }

    // Free the memory allocated for the row pointers array
    free(M);

    // Simulate delay before exiting
    sleep(5);
    pr_warning("Exiting!\n");

    return EXIT_SUCCESS;
}
