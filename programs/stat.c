/// @file stat.c
/// @brief display file status
/// @copyright (c) 2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <fcntl.h>
#include <io/debug.h>
#include <stdio.h>
#include <strerror.h>
#include <string.h>
#include <sys/bitops.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <sys/unistd.h>

// Copied from mentos/src/fs/ext2.c
// File types.
#define S_IFMT   0xF000 ///< Format mask
#define S_IFSOCK 0xC000 ///< Socket
#define S_IFLNK  0xA000 ///< Symbolic link
#define S_IFREG  0x8000 ///< Regular file
#define S_IFBLK  0x6000 ///< Block device
#define S_IFDIR  0x4000 ///< Directory
#define S_IFCHR  0x2000 ///< Character device
#define S_IFIFO  0x1000 ///< Fifo

void __print_time(const char* prefix, time_t *time) {
    tm_t *timeinfo = localtime(time);
    printf("%s%d-%d-%d %d:%d:%d\n",
        prefix,
        timeinfo->tm_year,
        timeinfo->tm_mon,
        timeinfo->tm_mday,
        timeinfo->tm_hour,
        timeinfo->tm_min,
        timeinfo->tm_sec);
}

int main(int argc, char** argv)
{
    if (argc != 2) {
        printf("%s: missing operand.\n", argv[0]);
        printf("Try '%s --help' for more information.\n", argv[0]);
        exit(1);
    }
    if (strcmp(argv[1], "--help") == 0) {
        printf("Usage: %s FILE\n", argv[0]);
        printf( "Display file status.\n");
        exit(0);
    }
    stat_t statbuf;
    if(stat(argv[1], &statbuf) == -1) {
        printf("%s: cannot stat '%s': %s\n", argv[0], argv[1], strerror(errno));
        exit(1);
    }


    printf("File: %s\n", argv[1]);
    printf("Size: %s\n", to_human_size(statbuf.st_size));
    printf("File type: ");
    switch (statbuf.st_mode & S_IFMT) {
        case S_IFBLK:  printf("block device\n");            break;
        case S_IFCHR:  printf("character device\n");        break;
        case S_IFDIR:  printf("directory\n");               break;
        case S_IFIFO:  printf("FIFO/pipe\n");               break;
        case S_IFLNK:  printf("symlink\n");                 break;
        case S_IFREG:  printf("regular file\n");            break;
        case S_IFSOCK: printf("socket\n");                  break;
        default:       printf("unknown?\n");                break;
    }
    printf("Access: (%.4o/", statbuf.st_mode & 0xFFF);
    // Print the access permissions.
    putchar(bitmask_check(statbuf.st_mode, S_IRUSR) ? 'r' : '-');
    putchar(bitmask_check(statbuf.st_mode, S_IWUSR) ? 'w' : '-');
    putchar(bitmask_check(statbuf.st_mode, S_IXUSR) ? 'x' : '-');
    putchar(bitmask_check(statbuf.st_mode, S_IRGRP) ? 'r' : '-');
    putchar(bitmask_check(statbuf.st_mode, S_IWGRP) ? 'w' : '-');
    putchar(bitmask_check(statbuf.st_mode, S_IXGRP) ? 'x' : '-');
    putchar(bitmask_check(statbuf.st_mode, S_IROTH) ? 'r' : '-');
    putchar(bitmask_check(statbuf.st_mode, S_IWOTH) ? 'w' : '-');
    putchar(bitmask_check(statbuf.st_mode, S_IXOTH) ? 'x' : '-');
    printf(") Uid: (%d) Gid: (%d)\n", statbuf.st_uid, statbuf.st_gid);
    __print_time("Access: ", &statbuf.st_atime);
    __print_time("Modify: ", &statbuf.st_mtime);
    __print_time("Change: ", &statbuf.st_ctime);
    return 0;
}
