/// @file t_mkdir.c
/// @brief Test directory creation
/// @details This program tests the creation, checking, and removal of directories.
/// It demonstrates basic directory management in C, including error handling for directory operations.
/// @copyright (c) 2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <io/debug.h>
#include <stdlib.h>
#include <strerror.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/// @brief Create a directory.
/// @param parent_directory The parent directory path.
/// @param directory_name The name of the directory to create.
/// @param mode The permissions to set for the new directory.
/// @return EXIT_SUCCESS on success, EXIT_FAILURE on failure.
int create_dir(const char *parent_directory, const char *directory_name, mode_t mode)
{
    char path[PATH_MAX];
    memset(path, 0, PATH_MAX);
    strcat(path, parent_directory);
    strcat(path, directory_name);
    if (mkdir(path, mode) < 0) {
        fprintf(STDERR_FILENO, "Failed to create directory %s: %s\n", path, strerror(errno));
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

/// @brief Remove a directory.
/// @param parent_directory The parent directory path.
/// @param directory_name The name of the directory to remove.
/// @return EXIT_SUCCESS on success, EXIT_FAILURE on failure.
int remove_dir(const char *parent_directory, const char *directory_name)
{
    char path[PATH_MAX];
    memset(path, 0, PATH_MAX);
    strcat(path, parent_directory);
    strcat(path, directory_name);
    if (rmdir(path) < 0) {
        fprintf(STDERR_FILENO, "Failed to remove directory %s: %s\n", path, strerror(errno));
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

/// @brief Check if a directory exists.
/// @param parent_directory The parent directory path.
/// @param directory_name The name of the directory to check.
/// @return EXIT_SUCCESS if the directory exists, EXIT_FAILURE otherwise.
int check_dir(const char *parent_directory, const char *directory_name)
{
    char path[PATH_MAX];
    memset(path, 0, PATH_MAX);
    strcat(path, parent_directory);
    strcat(path, directory_name);
    struct stat buffer;
    if (stat(path, &buffer) < 0) {
        fprintf(STDERR_FILENO, "Failed to check directory `%s`: %s\n", path, strerror(errno));
        return EXIT_FAILURE;
    }
    if (!S_ISDIR(buffer.st_mode)) {
        fprintf(STDERR_FILENO, "Path `%s` is not a directory.\n", path);
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

/// @brief Test the creation, checking, and removal of consecutive directories.
/// @param parent_directory The parent directory path.
/// @return EXIT_SUCCESS on success, EXIT_FAILURE on failure.
int test_consecutive_dirs(const char *parent_directory)
{
    int ret = EXIT_SUCCESS;
    if (create_dir(parent_directory, "/t_mkdir", 0777) == EXIT_FAILURE) {
        return EXIT_FAILURE;
    }
    if (create_dir(parent_directory, "/t_mkdir/outer", 0777) == EXIT_FAILURE) {
        remove_dir(parent_directory, "/t_mkdir");
        return EXIT_FAILURE;
    }
    if (create_dir(parent_directory, "/t_mkdir/outer/inner", 0777) == EXIT_FAILURE) {
        remove_dir(parent_directory, "/t_mkdir/outer");
        remove_dir(parent_directory, "/t_mkdir");
        return EXIT_FAILURE;
    }
    // Check if all directories are present.
    if ((ret = check_dir(parent_directory, "/t_mkdir")) == EXIT_SUCCESS) {
        if ((ret = check_dir(parent_directory, "/t_mkdir/outer")) == EXIT_SUCCESS) {
            ret = check_dir(parent_directory, "/t_mkdir/outer/inner");
        }
    }
    // Remove the directories.
    if (remove_dir(parent_directory, "/t_mkdir/outer/inner") == EXIT_FAILURE) {
        return EXIT_FAILURE;
    }
    if (remove_dir(parent_directory, "/t_mkdir/outer") == EXIT_FAILURE) {
        return EXIT_FAILURE;
    }
    if (remove_dir(parent_directory, "/t_mkdir") == EXIT_FAILURE) {
        return EXIT_FAILURE;
    }
    return ret;
}

int main(int argc, char *argv[])
{
    if (test_consecutive_dirs("")) {
        return EXIT_FAILURE;
    }
    if (test_consecutive_dirs("/home/user")) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
