/// @file t_mkdir.c
/// @brief Test directory creation
/// @copyright (c) 2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <strerror.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/unistd.h>

#define BASEDIR "/t_mkdir"

int create_dir(const char *pathname, mode_t mode)
{
    int ret = mkdir(pathname, mode);
    if (ret < 0) {
        printf("Failed to create dir %s: %s\n", pathname, strerror(errno));
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

int test_consecutive_dirs(void)
{
    int ret = EXIT_SUCCESS;
    if (create_dir(BASEDIR "/outer", 0777) == EXIT_FAILURE) {
        return EXIT_FAILURE;
    }
    if (create_dir(BASEDIR "/outer/inner", 0777) == EXIT_FAILURE) {
        ret = EXIT_FAILURE;
        goto clean_outer;
    }

    // Check if both directories are present
    int fd = open(BASEDIR "/outer", O_RDONLY | O_DIRECTORY, 0);
    if (fd == -1) {
        ret = EXIT_FAILURE;
        printf("failed to open dir %s: %s\n", BASEDIR "/outer", strerror(errno));
        goto clean_inner;
    }

    dirent_t dent;
    ssize_t read_bytes = getdents(fd, &dent, sizeof(dirent_t));
    // We reached the end of the folder.
    if (read_bytes <= 0) {
        ret = EXIT_FAILURE;
        // We encountered an error.
        if (read_bytes == -1) {
            printf("failed to read outer dir: %s\n", strerror(errno));
        } else { // There was nothing to read
            printf("dir outer does not contain any entries\n");
        }
        goto clean_inner;
    }

    if (strcmp(dent.d_name, "outer") != 0) {
        ret = EXIT_FAILURE;
        printf("outer expected, %s found\n", dent.d_name);
    }

clean_inner:
    rmdir(BASEDIR "/outer/inner");
clean_outer:
    rmdir(BASEDIR "/outer");
    return ret;
}

int main(int argc, char *argv[])
{
    int ret = mkdir(BASEDIR, 0777);
    if (ret == -1) {
        err(EXIT_FAILURE, "Failed to create base dir %s", BASEDIR);
    }
    printf("Running `test_consecutive_dirs`...\n");
    if (test_consecutive_dirs()) {
        return EXIT_FAILURE;
    }

    ret = rmdir(BASEDIR);
    if (ret == -1) {
        err(EXIT_FAILURE, "Failed to remove base dir %s", BASEDIR);
    }

    return EXIT_SUCCESS;
}
