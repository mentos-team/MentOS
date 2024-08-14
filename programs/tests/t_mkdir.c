/// @file t_mkdir.c
/// @brief Test directory creation
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
#include <sys/unistd.h>

int create_dir(const char *parent_directory, const char *directory_name, mode_t mode)
{
    char path[PATH_MAX];
    memset(path, 0, PATH_MAX);
    strcat(path, parent_directory);
    strcat(path, directory_name);
    pr_notice("Creating directory `%s`...\n", path);
    if (mkdir(path, mode) < 0) {
        pr_err("Failed to create directory %s: %s\n", path, strerror(errno));
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

int remove_dir(const char *parent_directory, const char *directory_name)
{
    char path[PATH_MAX];
    memset(path, 0, PATH_MAX);
    strcat(path, parent_directory);
    strcat(path, directory_name);
    pr_notice("Removing directory `%s`...\n", path);
    if (rmdir(path) < 0) {
        pr_err("Failed to remove directory %s: %s\n", path, strerror(errno));
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

int check_dir(const char *parent_directory, const char *directory_name)
{
    char path[PATH_MAX];
    memset(path, 0, PATH_MAX);
    strcat(path, parent_directory);
    strcat(path, directory_name);
    pr_notice("Checking directory `%s`...\n", path);
    stat_t buffer;
    stat(path, &buffer);
    if (!S_ISDIR(buffer.st_mode)) {
        pr_err("Failed to check directory `%s` : %s.\n", path, strerror(errno));
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

int test_consecutive_dirs(const char *parent_directory)
{
    int ret;
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
    // Check if both directories are present.
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
    pr_notice("Running `test_consecutive_dirs`...\n");
    if (test_consecutive_dirs("/home/user")) {
        return EXIT_FAILURE;
    }
    if (test_consecutive_dirs("")) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
