/// @file ls.c
/// @brief Command 'ls'.
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <sys/dirent.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <sys/unistd.h>
#include <fcntl.h>
#include <strerror.h>
#include <sys/stat.h>
#include <libgen.h>
#include <sys/bitops.h>
#include <debug.h>

#define FLAG_L (1U << 0U)
#define FLAG_A (1U << 1U)

#define FG_BRIGHT_GREEN  "\033[92m"
#define FG_BRIGHT_CYAN   "\033[96m"
#define FG_BRIGHT_WHITE  "\033[97m"
#define FG_BRIGHT_YELLOW "\033[93m"

#define DENTS_NUM 12

static inline void print_dir_entry(dirent_t *dirent, const char *path, unsigned int flags, size_t *total_size)
{
    static char relative_path[PATH_MAX];
    tm_t *timeinfo;
    stat_t dstat;

    // Check if the file starts with a dot (hidden), and we did not receive
    // the `a` flag.
    if ((dirent->d_name[0] == '.') && !bitmask_check(flags, FLAG_A)) {
        return;
    }

    // Prepare the relative path.
    strcpy(relative_path, path);
    if (strcmp(path, "/") != 0)
        strcat(relative_path, "/");
    strcat(relative_path, dirent->d_name);

    // Stat the file.
    if (stat(relative_path, &dstat) == -1) {
        return;
    }

    // Deal with the coloring.
    if ((dirent->d_type == DT_REG) && bitmask_check(dstat.st_mode, S_IXUSR)) {
        puts(FG_BRIGHT_YELLOW);
    } else if (dirent->d_type == DT_DIR) {
        puts(FG_BRIGHT_CYAN);
    } else if (dirent->d_type == DT_BLK) {
        puts(FG_BRIGHT_GREEN);
    }

    // Deal with the -l.
    if (bitmask_check(flags, FLAG_L)) {
        // Get the broken down time from the creation time of the file.
        timeinfo = localtime(&dstat.st_ctime);
        // Print the file type.
        putchar(dt_char_array[dirent->d_type]);
        // Print the access permissions.
        putchar(bitmask_check(dstat.st_mode, S_IRUSR) ? 'r' : '-');
        putchar(bitmask_check(dstat.st_mode, S_IWUSR) ? 'w' : '-');
        putchar(bitmask_check(dstat.st_mode, S_IXUSR) ? 'x' : '-');
        putchar(bitmask_check(dstat.st_mode, S_IRGRP) ? 'r' : '-');
        putchar(bitmask_check(dstat.st_mode, S_IWGRP) ? 'w' : '-');
        putchar(bitmask_check(dstat.st_mode, S_IXGRP) ? 'x' : '-');
        putchar(bitmask_check(dstat.st_mode, S_IROTH) ? 'r' : '-');
        putchar(bitmask_check(dstat.st_mode, S_IWOTH) ? 'w' : '-');
        putchar(bitmask_check(dstat.st_mode, S_IXOTH) ? 'x' : '-');
        // Add a space.
        putchar(' ');
        // Print the rest.
        printf("%4d %4d %11s %2d/%2d %2d:%2d %s\n",
               dstat.st_uid,
               dstat.st_gid,
               to_human_size(dstat.st_size),
               timeinfo->tm_mon,
               timeinfo->tm_mday,
               timeinfo->tm_hour,
               timeinfo->tm_min,
               dirent->d_name);
        (*total_size) += dstat.st_size;
    } else {
        printf("%s ", dirent->d_name);
    }

    // Reset the color.
    puts(FG_BRIGHT_WHITE);
}

static void print_ls(int fd, const char *path, unsigned int flags)
{
    dirent_t dents[DENTS_NUM];
    memset(&dents, 0, DENTS_NUM * sizeof(dirent_t));

    size_t total_size = 0;
    while (getdents(fd, dents, DENTS_NUM * sizeof(dirent_t))) {
        for (size_t i = 0; i < DENTS_NUM; ++i) {
            if (dents[i].d_ino == 0)
                break;
            print_dir_entry(&dents[i], path, flags, &total_size);
        }
    }
    printf("\n");

    if (bitmask_check(flags, FLAG_L)) {
        printf("Total: %d byte\n", total_size);
    }
    printf("\n");
}

int main(int argc, char *argv[])
{
    // Create a variable to store flags.
    uint32_t flags = 0;
    // Check the number of arguments.
    for (int i = 1; i < argc; ++i) {
        if ((strcmp(argv[i], "--help") == 0) || (strcmp(argv[i], "-h") == 0)) {
            printf("List information about files inside a given directory.\n");
            printf("Usage:\n");
            printf("    ls [options] [directory]\n\n");
            return 0;
        } else if (argv[i][0] == '-') {
            if ((strcmp(argv[i], "-l") == 0) || (strcmp(argv[i], "--long") == 0))
                bitmask_set_assign(flags, FLAG_L);
            else if ((strcmp(argv[i], "-a") == 0) || (strcmp(argv[i], "--all") == 0))
                bitmask_set_assign(flags, FLAG_A);
        }
    }

    bool_t no_directory = true;
    for (int i = 1; i < argc; ++i) {
        if (argv[i][0] == '-')
            continue;
        no_directory = false;
        int fd       = open(argv[i], O_RDONLY | O_DIRECTORY, 0);
        if (fd == -1) {
            printf("ls: cannot access '%s': %s\n\n", argv[i], strerror(errno));
        } else {
            printf("%s:\n", argv[i]);
            print_ls(fd, argv[i], flags);
            close(fd);
        }
    }
    if (no_directory) {
        char cwd[PATH_MAX];
        getcwd(cwd, PATH_MAX);
        int fd = open(cwd, O_RDONLY | O_DIRECTORY, 0);
        if (fd == -1) {
            printf("ls: cannot access '%s': %s\n\n", cwd, strerror(errno));
        } else {
            print_ls(fd, cwd, flags);
            close(fd);
        }
    }
    return 0;
}
