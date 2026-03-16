/// @file df.c
/// @brief Report filesystem disk space usage.
/// @copyright (c) 2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/statfs.h>
#include <unistd.h>

#define DF_READ_BUFFER_SIZE 4096

static void print_help(const char *prog)
{
    printf("Usage: %s\n", prog);
    printf("Show filesystem disk usage for mounted filesystems.\n");
}

static void print_header(void)
{
    printf("%-18s %10s %10s %10s %5s %s\n", "Filesystem", "1K-blocks", "Used", "Available", "Use%", "Mounted on");
}

static void print_mount_usage(const char *source, const char *mountpoint)
{
    statfs_t fs;
    if (statfs(mountpoint, &fs) < 0) {
        printf("%-18s %10s %10s %10s %5s %s\n", source, "-", "-", "-", "-", mountpoint);
        return;
    }

    uint32_t blocks_per_k = (fs.f_bsize >= 1024U) ? (fs.f_bsize / 1024U) : 1U;
    uint32_t total_k      = fs.f_blocks * blocks_per_k;
    uint32_t free_k       = fs.f_bavail * blocks_per_k;
    uint32_t used_k       = (total_k > free_k) ? (total_k - free_k) : 0;

    uint32_t use_pct = 0;
    if (total_k > 0) {
        use_pct = (used_k * 100U) / total_k;
    }

    printf("%-18s %10u %10u %10u %4u%% %s\n", source, total_k, used_k, free_k, use_pct, mountpoint);
}

int main(int argc, char **argv)
{
    if ((argc == 2) && (strcmp(argv[1], "--help") == 0)) {
        print_help(argv[0]);
        return 0;
    }
    if (argc != 1) {
        printf("%s: invalid arguments\n", argv[0]);
        print_help(argv[0]);
        return 1;
    }

    int fd = open("/proc/mounts", O_RDONLY, 0);
    if (fd < 0) {
        printf("%s: cannot open '/proc/mounts' (errno=%d)\n", argv[0], errno);
        return 1;
    }

    char buffer[DF_READ_BUFFER_SIZE + 1];
    ssize_t nread = read(fd, buffer, DF_READ_BUFFER_SIZE);
    close(fd);

    if (nread <= 0) {
        printf("%s: cannot read '/proc/mounts'\n", argv[0]);
        return 1;
    }

    buffer[nread] = '\0';

    print_header();

    char *save_line = NULL;
    char *line      = strtok_r(buffer, "\n", &save_line);
    while (line != NULL) {
        char source[PATH_MAX];
        char mountpoint[PATH_MAX];
        char fstype[64];
        char options[64];

        int parsed = sscanf(line, "%4095s %4095s %63s %63s %*s %*s", source, mountpoint, fstype, options);
        if (parsed == 4) {
            print_mount_usage(source, mountpoint);
        }

        line = strtok_r(NULL, "\n", &save_line);
    }

    return 0;
}
