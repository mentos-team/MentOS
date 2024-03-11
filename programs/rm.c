/// @file rm.c
/// @brief
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>
#include <fcntl.h>
#include <strerror.h>
#include <stdbool.h>
#include <libgen.h>

bool_t has_option(int argc, char **argv, const char *first, ...)
{
    va_list ap;
    const char *opt;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], first) == 0)
            return true;
        va_start(ap, first);
        while ((opt = va_arg(ap, const char *)) != NULL) {
            if (strcmp(argv[i], opt) == 0)
                return true;
        }
        va_end(ap);
    }
    return false;
}

int main(int argc, char **argv)
{
    if (argc <= 1) {
        printf("%s: missing operand.\n", argv[0]);
        printf("Try '%s --help' for more information.\n", argv[0]);
        return 1;
    }
    if (strcmp(argv[1], "--help") == 0) {
        printf("Remove (unlink) the FILE(s).\n");
        printf("Usage:\n");
        printf("    rm <filename>\n");
        return 0;
    }
    if (strcmp(basename(argv[argc - 1]), "*") == 0) {
        char directory[PATH_MAX], fullpath[PATH_MAX];
        int fd;

        if (strcmp(argv[argc - 1], "*") == 0) {
            getcwd(directory, PATH_MAX);
        } else {
            // Get the parent directory.
            if (!dirname(argv[argc - 1], directory, sizeof(directory))) {
                return 1;
            }
        }

        if ((fd = open(directory, O_RDONLY | O_DIRECTORY, 0)) != -1) {
            dirent_t dent;
            while (getdents(fd, &dent, sizeof(dirent_t)) == sizeof(dirent_t)) {
                strcpy(fullpath, directory);
                strcat(fullpath, dent.d_name);
                if (dent.d_type == DT_REG) {
                    if (unlink(fullpath) == 0) {
                        if (lseek(fd, -1, SEEK_CUR) != -1) {
                            printf("Failed to move back the getdents...\n");
                        }
                    }
                }
            }
            close(fd);
        }
    } else {
        if (unlink(argv[argc - 1]) < 0) {
            printf("%s: cannot remove '%s': %s\n", argv[0], argv[argc - 1], strerror(errno));
            return 1;
        }
    }
    printf("\n");
    return 0;
}
