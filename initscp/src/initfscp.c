/// @file   initfscp.c
/// @brief

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <libgen.h>
#include <dirent.h>
#include <assert.h>
#include <time.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#define INITFSCP_VER "0.3.0"

#define INITRD_NAME_MAX    255
#define INITRD_MAX_FILES   128
#define INITRD_MAX_FS_SIZE 1048576

#define RESET   "\033[00m"
#define BLACK   "\033[30m"
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define BLUE    "\033[34m"
#define MAGENTA "\033[35m"
#define CYAN    "\033[36m"
#define WHITE   "\033[37m"

static const char dt_char_array[] = {
    '?', // DT_UNKNOWN = 0,
    'p', //DT_FIFO = 1,
    'c', //DT_CHR  = 2,
    '*',
    'd', //DT_DIR  = 4,
    '*',
    'b', //DT_BLK  = 6,
    '*',
    '-', //DT_REG  = 8,
    '*',
    'l', //DT_LNK  = 10,
    '*',
    's', //DT_SOCK = 12,
    '*',
    '?', //DT_WHT  = 14
};

/// @brief Information concerning a file.
typedef struct initrd_file_t {
    /// Number used as delimiter, it must be set to 0xBF.
    int magic;
    /// The inode of the file.
    unsigned int inode;
    /// The name of the file.
    char name[INITRD_NAME_MAX];
    /// The type of the file.
    short int file_type;
    /// The permissions mask.
    unsigned int mask;
    /// The id of the owner.
    unsigned int uid;
    /// The id of the group.
    unsigned int gid;
    /// Time of last access.
    unsigned int atime;
    /// Time of last data modification.
    unsigned int mtime;
    /// Time of last status change.
    unsigned int ctime;
    /// Offset of the starting address.
    unsigned int offset;
    /// Dimension of the file.
    unsigned int length;
} __attribute__((aligned(16))) initrd_file_t;

/// @brief The details regarding the filesystem.
/// @brief Contains the number of files inside the initrd filesystem.
typedef struct initrd_t {
    /// Number of files.
    unsigned int nfiles;
    /// List of headers.
    initrd_file_t headers[INITRD_MAX_FILES];
} __attribute__((aligned(16))) initrd_t;

// Prepare a variable to keep track of the offsets inside the FS. By
//  default skip the initial space for the FS details structure.
static int fs_offset = sizeof(initrd_t);

static inline void usage(char *prgname)
{
    printf("Usage:\n");
    printf(" %s --help        For this screen\n", prgname);
    printf(" %s --source [-s] The source directory.\n", prgname);
    printf(" %s --target [-t] The target file for the initrd.\n", prgname);
}

static inline void version(char *prgname)
{
    printf("%s version: %s\n", prgname, INITFSCP_VER);
}

static inline int is_option_mount_point(char *arg)
{
    return ((strcmp(arg, "-m") == 0) || (strcmp(arg, "--mountpoint") == 0));
}

static inline int is_option_source(char *arg)
{
    return ((strcmp(arg, "-s") == 0) || (strcmp(arg, "--source") == 0));
}

static inline int is_option_target(char *arg)
{
    return ((strcmp(arg, "-t") == 0) || (strcmp(arg, "--target") == 0));
}

static int open_target_fs(int argc, char *argv[])
{
    printf("%-64s", "Opening target filesystem...");
    int fd = -1;
    for (int i = 1; i < argc; ++i) {
        if (is_option_target(argv[i]) && ((i + 1) < argc)) {
            fd = open(argv[i + 1], O_CREAT | O_RDWR, S_IRWXU | S_IRWXG | S_IRWXO);
            break;
        }
    }
    if (fd == -1)
        printf("[" RED "FAILED" RESET "]\n\n");
    else
        printf("[" GREEN "DONE" RESET "]\n\n");
    return fd;
}

static void init_headers(initrd_t *initrd)
{
    printf("%-64s", "Initializing headers structures...");
    for (size_t i = 0; i < INITRD_MAX_FILES; i++) {
        initrd->headers[i].magic     = 0xBF;
        initrd->headers[i].inode     = i;
        initrd->headers[i].file_type = 0;
        initrd->headers[i].mask      = 0;
        initrd->headers[i].uid       = 0;
        initrd->headers[i].gid       = 0;
        initrd->headers[i].atime     = time(NULL);
        initrd->headers[i].mtime     = time(NULL);
        initrd->headers[i].ctime     = time(NULL);
        initrd->headers[i].offset    = 0;
        initrd->headers[i].length    = 0;

        memset(initrd->headers[i].name, 0, INITRD_NAME_MAX);
    }
    printf("[" GREEN "DONE" RESET "]\n\n");
}

static int create_file_headers(initrd_t *initrd, char *mountpoint, char *directory)
{
    assert(mountpoint && "Received null mountpoint.");
    assert(directory && "Received null directory.");

    // ------------------------------------------------------------------------
    char directory_path_abs[PATH_MAX], entry_path_abs[PATH_MAX], fs_abs_path[PATH_MAX];
    memset(directory_path_abs, 0, PATH_MAX);
    memset(entry_path_abs, 0, PATH_MAX);
    memset(fs_abs_path, 0, PATH_MAX);
    strcpy(directory_path_abs, mountpoint);
    strcat(directory_path_abs, directory);

    int dir_fd = open(directory_path_abs, O_RDONLY);
    if (dir_fd == -1) {
        printf("[" RED "FAILED" RESET "]\n");
        printf("Could not open source directory %s.\n", directory_path_abs);
        return 0;
    }

    // ------------------------------------------------------------------------
    char buffer[BUFSIZ], *ebuf;
    off_t basep;
    int nbytes;
    while ((nbytes = getdirentries(dir_fd, buffer, BUFSIZ, &basep)) > 0) {
        ebuf     = buffer + nbytes;
        char *cp = buffer;
        while (cp < ebuf) {
            struct dirent *entry = (struct dirent *)cp;
            if ((strcmp(entry->d_name, ".") == 0) || (strcmp(entry->d_name, "..") == 0)) {
                cp += entry->d_reclen;
                continue;
            }
            // Create the host machine absolute path.
            strcpy(entry_path_abs, directory_path_abs);
            strcat(entry_path_abs, "/");
            strcat(entry_path_abs, entry->d_name);
            // Create the filesystem absolute path.
            strcpy(fs_abs_path, directory);
            strcat(fs_abs_path, "/");
            strcat(fs_abs_path, entry->d_name);
            // Print the entry.
            printf("[%3d] ENTRY: %-52s", initrd->nfiles, fs_abs_path);
            // Stat the file.
            struct stat _stat;
            if (stat(entry_path_abs, &_stat) == -1) {
                printf("[" RED "FAILED" RESET "]\n");
                printf("Error while executing stat on file : %s\n", entry_path_abs);
                return 0;
            }
            printf("[%c%c%c%c%c%c%c%c%c%c]",
                   dt_char_array[entry->d_type],
                   (_stat.st_mode & S_IRUSR) != 0 ? 'r' : '-',
                   (_stat.st_mode & S_IWUSR) != 0 ? 'w' : '-',
                   (_stat.st_mode & S_IXUSR) != 0 ? 'x' : '-',
                   (_stat.st_mode & S_IRGRP) != 0 ? 'r' : '-',
                   (_stat.st_mode & S_IWGRP) != 0 ? 'w' : '-',
                   (_stat.st_mode & S_IXGRP) != 0 ? 'x' : '-',
                   (_stat.st_mode & S_IROTH) != 0 ? 'r' : '-',
                   (_stat.st_mode & S_IWOTH) != 0 ? 'w' : '-',
                   (_stat.st_mode & S_IXOTH) != 0 ? 'x' : '-');

            if (entry->d_type != DT_DIR) {
                // ----------------------------------------------------------------
                // Copy the name and the main info.
                strcpy(initrd->headers[initrd->nfiles].name, fs_abs_path);
                initrd->headers[initrd->nfiles].file_type = DT_REG;
                initrd->headers[initrd->nfiles].length    = _stat.st_size;
                initrd->headers[initrd->nfiles].offset    = fs_offset;
                initrd->headers[initrd->nfiles].mask      = _stat.st_mode;
                initrd->headers[initrd->nfiles].uid       = _stat.st_uid;
                initrd->headers[initrd->nfiles].gid       = _stat.st_gid;
                initrd->headers[initrd->nfiles].atime     = _stat.st_atim.tv_sec;
                initrd->headers[initrd->nfiles].mtime     = _stat.st_mtim.tv_sec;
                initrd->headers[initrd->nfiles].ctime     = _stat.st_ctim.tv_sec;

                // ----------------------------------------------------------------
                printf("[OFFSET:%6d]", initrd->headers[initrd->nfiles].offset);
                printf("[SIZE:%6d]", initrd->headers[initrd->nfiles].length);
                printf("[" GREEN "DONE" RESET "]\n");

                fs_offset += initrd->headers[initrd->nfiles].length;
                ++initrd->nfiles;
            } else {
                // ----------------------------------------------------------------
                strcpy(initrd->headers[initrd->nfiles].name, fs_abs_path);
                initrd->headers[initrd->nfiles].file_type = DT_DIR;
                initrd->headers[initrd->nfiles].length    = _stat.st_size;
                initrd->headers[initrd->nfiles].offset    = fs_offset;
                initrd->headers[initrd->nfiles].mask      = _stat.st_mode;
                initrd->headers[initrd->nfiles].uid       = _stat.st_uid;
                initrd->headers[initrd->nfiles].gid       = _stat.st_gid;
                initrd->headers[initrd->nfiles].atime     = _stat.st_atim.tv_sec;
                initrd->headers[initrd->nfiles].mtime     = _stat.st_mtim.tv_sec;
                initrd->headers[initrd->nfiles].ctime     = _stat.st_ctim.tv_sec;

                // ----------------------------------------------------------------
                printf("[OFFSET:%6d]", initrd->headers[initrd->nfiles].offset);
                printf("[SIZE:%6d]", initrd->headers[initrd->nfiles].length);
                printf("[" GREEN "DONE" RESET "]\n");

                // ----------------------------------------------------------------
                ++initrd->nfiles;

                // ----------------------------------------------------------------
                if (!create_file_headers(initrd, mountpoint, fs_abs_path)) {
                    break;
                }
            }
            cp += entry->d_reclen;
        }
    }
    close(dir_fd);
    return 1;
}

static int write_file_system(initrd_t *initrd, int target_fd, char *mountpoint)
{
    printf("Copying data to filesystem...\n");
    for (int i = 0; i < INITRD_MAX_FILES; i++) {
        // --------------------------------------------------------------------
        char absolute_path[256];
        memset(absolute_path, 0, 256);
        strcpy(absolute_path, mountpoint);
        strcat(absolute_path, initrd->headers[i].name);

        // --------------------------------------------------------------------
        if (initrd->headers[i].file_type == DT_REG) {
            FILE *fd2 = fopen(absolute_path, "r+");
            if (fd2 == NULL) {
                continue;
            }
            printf("[%3d] FILE: %-92s", i, absolute_path);
            char *buffer = (char *)malloc(initrd->headers[i].length);
            fread(buffer, 1, initrd->headers[i].length, fd2);
            // Write the content.
            write(target_fd, buffer, initrd->headers[i].length);

            printf("[" GREEN "DONE" RESET "]\n");
            fclose(fd2);
            free(buffer);
        }
    }
    printf("[" GREEN "DONE" RESET "]\n\n");
    return 1;
}

int main(int argc, char *argv[])
{
    printf("Welcome to MentOS initrd file copier tool\n\n");
    if (argc <= 1) {
        if (argv[0][0] == '.') {
            version(argv[0] + 2);
        }
        usage(argv[0]);
        return EXIT_FAILURE;
    }
    if (!strcmp(argv[1], "--version") || !strcmp(argv[1], "-v")) {
        version(argv[0] + 2);
        return EXIT_SUCCESS;
    }
    if (!strcmp(argv[1], "--help") || !strcmp(argv[1], "-h")) {
        usage(argv[0]);
        return EXIT_SUCCESS;
    }

    // Create the filesystem details structure.
    initrd_t initrd;
    // Target fs file descriptor.
    int target_fd;

    // ------------------------------------------------------------------------
    // Open the fs.
    if ((target_fd = open_target_fs(argc, argv)) == -1) {
        printf("Could not open target FileSystem.\n");
        return EXIT_FAILURE;
    }

    // ------------------------------------------------------------------------
    // Initialize the headers.
    init_headers(&initrd);

    // ------------------------------------------------------------------------
    // Create file headers.
    printf("Creating headers...\n");
    for (unsigned int i = 1; i < argc; ++i) {
        if (is_option_source(argv[i]) && ((i + 1) < argc)) {
            // ----------------------------------------------------------------
            strcpy(initrd.headers[initrd.nfiles].name, "/");
            initrd.headers[initrd.nfiles].file_type = DT_DIR;
            initrd.headers[initrd.nfiles].length    = 0;
            initrd.headers[initrd.nfiles].offset    = fs_offset;
            initrd.headers[initrd.nfiles].mask      = S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;
            initrd.headers[initrd.nfiles].uid       = 0;
            initrd.headers[initrd.nfiles].gid       = 0;
            initrd.headers[initrd.nfiles].atime     = time(NULL);
            initrd.headers[initrd.nfiles].mtime     = time(NULL);
            initrd.headers[initrd.nfiles].ctime     = time(NULL);

            printf("[%3d] DIR : %-52s", initrd.nfiles, initrd.headers[initrd.nfiles].name);
            printf("[" BLUE "OPEN" RESET "]");
            printf("[OFFSET:%6d]", initrd.headers[initrd.nfiles].offset);
            printf("[SIZE:%6d]", initrd.headers[initrd.nfiles].length);
            printf("[" GREEN "DONE" RESET "]\n");
            ++initrd.nfiles;

            // ----------------------------------------------------------------
            if (!create_file_headers(&initrd, argv[i + 1], "")) {
                printf("Could not create file headers.\n");
                close(target_fd);
                return EXIT_FAILURE;
            }
        }
    }
    printf("[" GREEN "DONE" RESET "]\n\n");

    // ------------------------------------------------------------------------
    // Copying information about headers on filesystem.
    printf("%-64s", "Copying the filesystem's details structure...");
    write(target_fd, &initrd, sizeof(initrd_t));
    printf("[" GREEN "DONE" RESET "]\n\n");

    // ------------------------------------------------------------------------
    // Write headers on filesystem.
    for (unsigned int i = 1; i < argc; ++i) {
        if (is_option_source(argv[i]) && ((i + 1) < argc)) {
            if (!write_file_system(&initrd, target_fd, argv[i + 1])) {
                printf("Could not write on filesystem.\n");
                close(target_fd);
                return EXIT_FAILURE;
            }
        }
    }

    // ------------------------------------------------------------------------
    lseek(target_fd, 0L, SEEK_SET);
    size_t fs_size = lseek(target_fd, 0L, SEEK_END);
    printf("FS size : %ld\n", fs_size);
    if (fs_size < INITRD_MAX_FS_SIZE - 1) {
        printf("Extend FS size to : %d\n", INITRD_MAX_FS_SIZE - 1);
        lseek(target_fd, INITRD_MAX_FS_SIZE - 1, SEEK_SET);
        write(target_fd, 0, 1);
    }
    close(target_fd);
    return 0;
}
