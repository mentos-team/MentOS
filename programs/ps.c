/// @file ps.c
/// @brief Report a snapshot of the current processes.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#define FORMAT_S "%5s %5s %6s %s\n"
#define FORMAT   "%5d %5d %6c %s\n"

static inline int __is_number(const char *str)
{
    // Check for NULL pointer.
    if (str == NULL) { return 0; }
    // Skip leading whitespace.
    while (isspace((unsigned char)*str)) { str++; }
    // Check if there is a sign at the beginning.
    if (*str == '-' || *str == '+') { str++; }
    // Check if the string is empty or only contains a sign.
    if (*str == '\0') { return 0; }
    // Check each character to ensure it's a digit
    while (*str) {
        if (!isdigit((unsigned char)*str)) { return 0; }
        str++;
    }
    // If we reach here, all characters were digits.
    return 1;
}

static inline void __iterate_proc_dirs(int proc_fd)
{
    char absolute_path[PATH_MAX] = "/proc/";
    // Holds the file descriptor of the stat file.
    int stat_fd;
    // Buffer used to read the stat file.
    char stat_buffer[BUFSIZ] = { 0 };
    // Variables used to read the stat file.
    // (1) pid   %d
    // (2) comm  %s
    // (3) state %c
    // (4) ppid  %d
    pid_t pid;
    char comm[BUFSIZ] = { 0 };
    char state;
    pid_t ppid;
    // The directory entry.
    dirent_t dent;
    // Holds the number of bytes read.
    ssize_t read_bytes;
    do {
        // Read an entry.
        read_bytes = getdents(proc_fd, &dent, sizeof(dirent_t));
        // We reached the end of the folder.
        if (read_bytes == 0) {
            break;
        }
        // We encountered an error.
        if (read_bytes == -1) {
            perror("Failed to read entry in `/proc` folder");
            exit(EXIT_FAILURE);
        }
        // Skip non-directories.
        if (dent.d_type != DT_DIR) {
            continue;
        }
        // Skip directories that are not PIDs.
        if (!__is_number(dent.d_name)) {
            continue;
        }
        // Build the path to the stat file (i.e., `/proc/<pid>/stat`).
        strcpy(absolute_path + 6, dent.d_name);
        strcat(absolute_path, "/stat");
        // Open the `/proc/<pid>/stat` file.
        stat_fd = open(absolute_path, O_RDONLY, 0);
        if (stat_fd == -1) {
            continue;
        }
        // Reset the stat buffer.
        memset(stat_buffer, 0, BUFSIZ);
        // Read the content of the stat file.
        read_bytes = read(stat_fd, stat_buffer, BUFSIZ);
        // Check if the read failed.
        if (read_bytes <= 0) {
            printf("Cannot read `%s`", absolute_path);
            perror(NULL);
            putchar('\n');
            close(stat_fd);
            continue;
        }
        // Reset the comm buffer.
        memset(comm, 0, BUFSIZ);
        // Parse the content of the stat file.
        sscanf(stat_buffer, "%d %s %c %d", &pid, comm, &state, &ppid);
        // Print the stats concerning the process.
        printf(FORMAT, pid, ppid, state, comm);
        // Closing stat FD.
        close(stat_fd);
    } while (1);
}

int main(int argc, char **argv)
{
    int fd = open("/proc", O_RDONLY | O_DIRECTORY, 0);
    if (fd == -1) {
        perror("ps: cannot access '/proc' folder");
        return EXIT_FAILURE;
    }
    printf(FORMAT_S, "PID", "PPID", "STATUS", "CMD");
    __iterate_proc_dirs(fd);
    close(fd);
    putchar('\n');
    return EXIT_SUCCESS;
}
