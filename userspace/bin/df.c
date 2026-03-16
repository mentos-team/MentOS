/// @file df.c
/// @brief Report filesystem disk space usage.
/// @copyright (c) 2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/statfs.h>
#include <syslog.h>
#include <unistd.h>

#define DF_READ_BUFFER_SIZE 4096

static void print_help(const char *prog)
{
    printf("Usage: %s [OPTION]\n", prog);
    printf("Show filesystem disk usage for mounted filesystems.\n");
    printf("  -h, --human-readable  print sizes in powers of 1024 (e.g., 1023M)\n");
    printf("      --help            display this help and exit\n");
}

static const char *to_human_size(unsigned long bytes)
{
    static char output[32];
    const char *suffix[] = {"B", "K", "M", "G", "T"};
    int len              = sizeof(suffix) / sizeof(suffix[0]);
    int i                = 0;
    double val           = (double)bytes;
    while (val >= 1024.0 && i < len - 1) {
        val /= 1024.0;
        i++;
    }
    sprintf(output, "%.1f%s", val, suffix[i]);
    return output;
}

static void print_header(bool_t human)
{
    const char *size_col = human ? "Size" : "1K-blocks";
    printf("%-18s %10s %10s %10s %5s %s\n", "Filesystem", size_col, "Used", "Available", "Use%", "Mounted on");
}

static void print_mount_usage(const char *source, const char *mountpoint, bool_t human)
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

    if (human) {
        printf("%-18s %10s %10s %10s %4u%% %s\n", source, to_human_size((unsigned long)total_k * 1024UL), to_human_size((unsigned long)used_k * 1024UL), to_human_size((unsigned long)free_k * 1024UL), use_pct, mountpoint);
    } else {
        printf("%-18s %10u %10u %10u %4u%% %s\n", source, total_k, used_k, free_k, use_pct, mountpoint);
    }
}

int main(int argc, char **argv)
{
    // Open syslog connection.
    openlog("df", LOG_CONS | LOG_PID, LOG_USER);
    // Set log mask.
    setlogmask(LOG_UPTO(LOG_DEBUG));

    bool_t human = false;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0) {
            print_help(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--human-readable") == 0) {
            human = true;
        } else {
            printf("%s: invalid argument '%s'\n", argv[0], argv[i]);
            print_help(argv[0]);
            return 1;
        }
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

    print_header(human);

    char *save_line = NULL;
    char *line      = strtok_r(buffer, "\n", &save_line);
    while (line != NULL) {
        char source[PATH_MAX];
        char mountpoint[PATH_MAX];
        char fstype[64];
        char options[64];

        int parsed = sscanf(line, "%4095s %4095s %63s %63s %*s %*s", source, mountpoint, fstype, options);
        if (parsed == 4) {
            print_mount_usage(source, mountpoint, human);
        }

        line = strtok_r(NULL, "\n", &save_line);
    }

    return 0;
}
