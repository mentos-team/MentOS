/// @file ps.c
/// @brief Report a snapshot of the current processes.
/// @copyright (c) 2014-2023 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <sys/unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define FORMAT_S "%5s %5s %6s %s\n"
#define FORMAT   "%5d %5d %6c %s\n"

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
        if (dent.d_type != DT_DIR)
            continue;
        // Build the path to the stat file (i.e., `/proc/<pid>/stat`).
        strcpy(absolute_path + 6, dent.d_name);
        strcat(absolute_path, "/stat");
        // Open the `/proc/<pid>/stat` file.
        if ((stat_fd = open(absolute_path, O_RDONLY, 0)) == -1) {
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
    } else {
        printf(FORMAT_S, "PID", "PPID", "STATUS", "CMD");
        __iterate_proc_dirs(fd);
        close(fd);
    }

#if 0

    DIR *dir;
    struct dirent_t *ent;
    int i, fd_self, fd;
    unsigned long time, stime;
    char flag, *tty;
    char cmd[256], tty_self[256], path[256], time_s[256];
    FILE *file;

    dir     = opendir("/proc");
    fd_self = open("/proc/self/fd/0", O_RDONLY);
    sprintf(tty_self, "%s", ttyname(fd_self));
    printf(FORMAT, "PID", "TTY", "TIME", "CMD");

    while ((ent = readdir(dir)) != NULL) {
        flag = 1;
        for (i = 0; ent->d_name[i]; i++)
            if (!isdigit(ent->d_name[i])) {
                flag = 0;
                break;
            }

        if (flag) {
            sprintf(path, "/proc/%s/fd/0", ent->d_name);
            fd  = open(path, O_RDONLY);
            tty = ttyname(fd);

            if (tty && strcmp(tty, tty_self) == 0) {
                sprintf(path, "/proc/%s/stat", ent->d_name);
                file = fopen(path, "r");
                fscanf(file, "%d%s%c%c%c", &i, cmd, &flag, &flag, &flag);
                cmd[strlen(cmd) - 1] = '\0';

                for (i = 0; i < 11; i++)
                    fscanf(file, "%lu", &time);
                fscanf(file, "%lu", &stime);
                time = (int)((double)(time + stime) / sysconf(_SC_CLK_TCK));
                sprintf(time_s, "%02lu:%02lu:%02lu",
                        (time / 3600) % 3600, (time / 60) % 60, time % 60);

                printf(FORMAT, ent->d_name, tty + 5, time_s, cmd + 1);
                fclose(file);
            }
            close(fd);
        }
    }
    close(fd_self);
#endif
    return EXIT_SUCCESS;
}
