/// @file man.c
/// @brief Shows the available commands.
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <strerror.h>
#include <sys/dirent.h>
#include <sys/unistd.h>
#include <fcntl.h>
#include <stdio.h>

int main(int argc, char *argv[])
{
    int fd = open("/bin", O_RDONLY | O_DIRECTORY, 0);
    if (fd == -1) {
        printf("%s: cannot access '/bin': %s\n\n", argv[0], strerror(errno));
        return 1;
    }
    dirent_t dent;
    int per_line = 0;
    while (getdents(fd, &dent, sizeof(dirent_t)) == sizeof(dirent_t)) {
        printf("%10s ", dent.d_name);
        if (++per_line == 6) {
            per_line = 0;
            putchar('\n');
        }
    }
    putchar('\n');
    close(fd);
    return 0;
}
