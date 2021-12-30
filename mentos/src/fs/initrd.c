/// @file initrd.c
/// @brief Headers of functions for initrd filesystem.
/// @copyright (c) 2014-2021 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "assert.h"
#include "system/syscall.h"
#include "sys/module.h"
#include "system/panic.h"
#include "fs/vfs.h"
#include "sys/errno.h"
#include "io/debug.h"
#include "mem/kheap.h"
#include "fcntl.h"
#include "sys/bitops.h"
#include "fs/initrd.h"
#include "string.h"
#include "stdio.h"
#include "libgen.h"
#include "fcntl.h"

/// Maximum length of name in INITRD.
#define INITRD_NAME_MAX 255U
/// Maximum number of files in INITRD.
#define INITRD_MAX_FILES 128U
/// Maximum size of files in INITRD.
#define INITRD_MAX_FS_SIZE 1048576

/// @brief Information concerning a file.
typedef struct initrd_file_t {
    /// Number used as delimiter, it must be set to 0xBF.
    int magic;
    /// The inode of the file.
    unsigned int inode;
    /// The name of the file.
    char fileName[INITRD_NAME_MAX];
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
static struct initrd_t {
    /// Number of files.
    unsigned int nfiles;
    /// List of headers.
    initrd_file_t headers[INITRD_MAX_FILES];
} fs_specs __attribute__((aligned(16)));

static vfs_file_t *initrd_create_file_struct(
    ino_t ino,
    const char *name,
    size_t size,
    int flags);

/// @brief Searches for the file at the given path.
/// @param path The path where to search the file.
/// @return The file if found, NULL otherwise.
static inline initrd_file_t *initrd_find_file(const char *path)
{
    for (unsigned int i = 0; i < INITRD_MAX_FILES; ++i)
        if (strcmp(path, fs_specs.headers[i].fileName) == 0)
            return &(fs_specs.headers[i]);
    return NULL;
}

/// @brief Searches for the file at the given path.
/// @param path The path where to search the file.
/// @return The file if found, NULL otherwise.
static inline ino_t initrd_find_inode(const char *path)
{
    for (unsigned int i = 0; i < INITRD_MAX_FILES; ++i)
        if (strcmp(path, fs_specs.headers[i].fileName) == 0)
            return fs_specs.headers[i].inode;
    return -1;
}

static inline initrd_file_t *get_free_header()
{
    for (size_t i = 0; i < INITRD_MAX_FILES; ++i)
        if (fs_specs.headers[i].file_type == 0)
            return &(fs_specs.headers[i]);
    return NULL;
}

static inline bool_t check_if_occupied(size_t offset)
{
    for (size_t i = 0; i < INITRD_MAX_FILES; ++i) {
        initrd_file_t *h = &fs_specs.headers[i];
        if ((h->file_type != 0) && (offset >= h->offset) && (offset <= (h->offset + h->length))) {
            return true;
        }
    }
    return false;
}

static inline int get_free_slot_offset()
{
    int offset = sizeof(struct initrd_t);
    for (size_t i = 0; i < INITRD_MAX_FILES; ++i) {
        initrd_file_t *h = &fs_specs.headers[i];
        if ((h->file_type != 0) && (offset >= h->offset) && (offset <= (h->offset + h->length))) {
            offset = (int)(h->offset + h->length);
            continue;
        }
        return offset;
    }
    return -1;
}

// TODO: doxygen comment.
static void dump_initrd_fs(void)
{
    for (size_t i = 0; i < INITRD_MAX_FILES; ++i) {
        initrd_file_t *file = &fs_specs.headers[i];
        pr_debug("[%3d][%c%c%c%c%c%c%c%c%c%c] %s\n",
                 i,
                 dt_char_array[file->file_type],
                 (file->mask & S_IRUSR) != 0 ? 'r' : '-',
                 (file->mask & S_IWUSR) != 0 ? 'w' : '-',
                 (file->mask & S_IXUSR) != 0 ? 'x' : '-',
                 (file->mask & S_IRGRP) != 0 ? 'r' : '-',
                 (file->mask & S_IWGRP) != 0 ? 'w' : '-',
                 (file->mask & S_IXGRP) != 0 ? 'x' : '-',
                 (file->mask & S_IROTH) != 0 ? 'r' : '-',
                 (file->mask & S_IWOTH) != 0 ? 'w' : '-',
                 (file->mask & S_IXOTH) != 0 ? 'x' : '-',
                 file->fileName);
    }
}

/// @brief Reads contents of the directories to a dirent buffer, updating
///        the offset and returning the number of written bytes in the buffer,
///        it assumes that all paths are well-formed.
/// @param file  The directory handler.
/// @param dirp  The buffer where the data should be written.
/// @param doff  The offset inside the buffer where the data should be written.
/// @param count The maximum length of the buffer.
/// @return The number of written bytes in the buffer.
static int initrd_getdents(vfs_file_t *file, dirent_t *dirp, off_t doff, size_t count)
{
    if (file->ino >= INITRD_MAX_FILES) {
        return -1;
    }

    initrd_file_t *tdir = &fs_specs.headers[file->ino];
    int len             = strlen(tdir->fileName);
    size_t written      = 0;
    off_t current       = 0;

    char *parent = NULL;
    for (off_t it = 0; (it < INITRD_MAX_FILES) && (written < count); ++it) {
        initrd_file_t *entry = &fs_specs.headers[it];
        if (entry->fileName[0] == '\0') {
            continue;
        }
        // If the entry is the directory itself, skip.
        if (strcmp(tdir->fileName, entry->fileName) == 0) {
            continue;
        }
        // Get the parent directory.
        parent = dirname(entry->fileName);
        // Check if the entry is inside the directory.
        if (strcmp(tdir->fileName, parent) != 0) {
            continue;
        }
        // Skip if already provided.
        current += sizeof(dirent_t);
        if (current <= doff) {
            continue;
        }

        if (*(entry->fileName + len) == '/')
            ++len;
        // Write on current dirp.
        dirp->d_ino  = it;
        dirp->d_type = entry->file_type;
        strcpy(dirp->d_name, entry->fileName + len);
        dirp->d_off    = sizeof(dirent_t);
        dirp->d_reclen = sizeof(dirent_t);
        // Increment the written counter.
        written += sizeof(dirent_t);
        // Move to next writing position.
        dirp += 1;
    }
    return written;
}

/// @brief      Creates a new directory.
/// @param path The path to the new directory.
/// @param mode The file mode.
/// @return 0   if success.
static int initrd_mkdir(const char *path, mode_t mode)
{
    if ((strcmp(path, ".") == 0) || (strcmp(path, "..") == 0)) {
        return -EPERM;
    }
    initrd_file_t *direntry = initrd_find_file(path);
    if (direntry != NULL) {
        return -EEXIST;
    }
    // Check if the directories before it exist.
    char *parent = dirname(path);
    if ((strcmp(parent, ".") != 0) && (strcmp(parent, "/") != 0)) {
        direntry = initrd_find_file(parent);
        if (direntry == NULL) {
            return -ENOENT;
        }
        if (direntry->file_type != DT_DIR) {
            return -ENOTDIR;
        }
    }
    // Get a free header.
    initrd_file_t *initrd_file = get_free_header();
    if (!initrd_file) {
        pr_err("Cannot create initrd_file for `%s`...\n", path);
        return -ENFILE;
    }
    int offset = get_free_slot_offset();
    if (offset < 0) {
        pr_err("There are no free slot available for `%s`...\n", path);
        return -ENFILE;
    }
    // Create the file.
    initrd_file->magic = 0xBF;
    strcpy(initrd_file->fileName, path);
    initrd_file->file_type = DT_DIR;
    initrd_file->uid       = scheduler_get_current_process()->uid;
    initrd_file->gid       = scheduler_get_current_process()->uid;
    initrd_file->offset    = offset;
    initrd_file->length    = 0;
    initrd_file->atime     = sys_time(NULL);
    initrd_file->mtime     = sys_time(NULL);
    initrd_file->ctime     = sys_time(NULL);
    // Increase the number of files.
    ++fs_specs.nfiles;
    return 0;
}

/// @brief Removes a directory.
/// @param path The path to the directory.
/// @return 0 if success.
static int initrd_rmdir(const char *path)
{
    if ((strcmp(path, ".") == 0) || (strcmp(path, "..") == 0)) {
        pr_err("initrd_rmdir(%s): Cannot remove `.` or `..`.\n", path);
        return -EPERM;
    }
    // Check if the directory exists.
    initrd_file_t *direntry = initrd_find_file(path);
    if (direntry == NULL) {
        pr_err("initrd_rmdir(%s): Cannot find the directory.\n", path);
        return -ENOENT;
    }
    // Check the type.
    if (direntry->file_type != DT_DIR) {
        pr_err("initrd_rmdir(%s): The entry is not a directory.\n", path);
        return -ENOTDIR;
    }
    for (int i = 0; i < INITRD_MAX_FILES; ++i) {
        initrd_file_t *entry = &fs_specs.headers[i];
        if (entry->fileName[0] == '\0') {
            continue;
        }
        // Get the directory of the file.
        char *filedir = dirname(entry->fileName);
        // Check if directory path and file directory are the same.
        if (strcmp(direntry->fileName, filedir) == 0) {
            pr_err("initrd_rmdir(%s): The directory is not empty.\n", path);
            return -ENOTEMPTY;
        }
    }
    // Remove the directory.
    direntry->magic = 0;
    memset(direntry->fileName, 0, NAME_MAX);
    direntry->file_type = 0;
    direntry->uid       = 0;
    direntry->offset    = 0;
    direntry->length    = 0;
    // Decrease the number of files.
    --fs_specs.nfiles;
    return 0;
}

/// @brief Open the file at the given path and returns its file descriptor.
/// @param path  The path to the file.
/// @param flags The flags used to determine the behavior of the function.
/// @param mode  The mode with which we open the file.
/// @return The file descriptor of the opened file, otherwise returns -1.
static vfs_file_t *initrd_open(const char *path, int flags, mode_t mode)
{
    initrd_file_t *initrd_file = initrd_find_file(path);
    if (initrd_file != NULL) {
        // Check if it is a directory.
        if (flags == (O_RDONLY | O_DIRECTORY)) {
            if (initrd_file->file_type != DT_DIR) {
                pr_err("Is not a directory `%s`...\n", path);
                errno = ENOTDIR;
                return NULL;
            }
            // Create the file structure.
            vfs_file_t *vfs_file = initrd_create_file_struct(
                initrd_file->inode,
                initrd_file->fileName,
                initrd_file->length,
                DT_DIR);
            if (!vfs_file) {
                pr_err("Cannot create vfs file for opening directory `%s`...\n", path);
                errno = ENOMEM;
                return NULL;
            }
            // Update file access.
            initrd_file->atime = sys_time(NULL);
            return vfs_file;
        } else if (initrd_file->file_type == DT_DIR) {
            pr_err("Is a directory `%s`...\n", path);
            errno = EISDIR;
            return NULL;
        }
        // Check if the open has to create.
        if (flags & O_CREAT) {
            pr_err("Cannot create, it exists `%s`...\n", path);
            errno = EEXIST;
            return NULL;
        }
        // Create the file structure.
        vfs_file_t *vfs_file = initrd_create_file_struct(
            initrd_file->inode,
            initrd_file->fileName,
            initrd_file->length,
            DT_REG);
        if (!vfs_file) {
            pr_err("Cannot create vfs file for opening file `%s`...\n", path);
            errno = ENOMEM;
            return NULL;
        }
        // Update file access.
        initrd_file->atime = sys_time(NULL);
        return vfs_file;
    }
    if (flags & O_CREAT) {
        // Check if the parent directory exists.
        char *dir = dirname(path);
        if ((strcmp(dir, ".") != 0) && (strcmp(dir, "/") != 0)) {
            if (initrd_find_file(dir) == NULL) {
                errno = ENOENT;
                return NULL;
            }
        }
        // Get a free header.
        initrd_file = get_free_header();
        if (!initrd_file) {
            pr_err("Cannot create initrd_file for `%s`...\n", path);
            errno = ENFILE;
            return NULL;
        }
        int offset = get_free_slot_offset();
        if (offset < 0) {
            pr_err("There are no free slot available for `%s`...\n", path);
            errno = ENFILE;
            return NULL;
        }
        // Create the file.
        initrd_file->magic = 0xBF;
        strcpy(initrd_file->fileName, path);
        initrd_file->file_type = DT_REG;
        initrd_file->mask      = S_IRWXU;
        initrd_file->uid       = scheduler_get_current_process()->uid;
        initrd_file->gid       = scheduler_get_current_process()->uid;
        initrd_file->offset    = offset;
        initrd_file->length    = 0;
        initrd_file->atime     = sys_time(NULL);
        initrd_file->mtime     = sys_time(NULL);
        initrd_file->ctime     = sys_time(NULL);
        // Increase the number of files.
        ++fs_specs.nfiles;
        // Create the file structure.
        vfs_file_t *vfs_file = initrd_create_file_struct(
            initrd_file->inode,
            initrd_file->fileName,
            initrd_file->length,
            DT_REG);
        if (!vfs_file) {
            pr_err("Cannot create vfs file for opening file `%s`...\n", path);
            errno = ENOMEM;
            return NULL;
        }
        return vfs_file;
    }
    errno = ENOENT;
    return NULL;
}

/// @brief      Deletes the file at the given path.
/// @param path The path to the file.
/// @return     On success, zero is returned. On error, -1 is returned.
static int initrd_unlink(const char *path)
{
    if ((strcmp(path, ".") == 0) || (strcmp(path, "..") == 0)) {
        return -EPERM;
    }
    // Check if the directory exists.
    initrd_file_t *file = initrd_find_file(path);
    if (file == NULL) {
        pr_err("initrd_unlink(%s): Cannot find the file.\n", path);
        return -ENOENT;
    }
    if (file->file_type != DT_REG) {
        if (file->file_type == DT_DIR) {
            pr_err("initrd_unlink(%s): The file is a directory.\n", path);
            return -EISDIR;
        }
        pr_err("initrd_unlink(%s): The file is not a regular file.\n", path);
        return -EACCES;
    }
    // Remove the directory.
    file->magic = 0;
    memset(file->fileName, 0, NAME_MAX);
    file->file_type = 0;
    file->uid       = 0;
    file->offset    = 0;
    file->length    = 0;
    // Decrease the number of files.
    --fs_specs.nfiles;
    return 0;
}

/// @brief Reads from the file identified by the file descriptor.
/// @param file   The file.
/// @param buf    Buffer where the read content must be placed.
/// @param offset Offset from which we start reading from the file.
/// @param nbyte  The number of bytes to read.
/// @return The number of red bytes.
static ssize_t initrd_read(vfs_file_t *file, char *buf, off_t offset, size_t nbyte)
{
    // If the number of byte to read is zero, skip.
    if (nbyte == 0) {
        return 0;
    }
    // Get the file descriptor of the file.
    int lfd = file->ino;
    // Get the current position.
    int read_pos = offset;
    // Get the length of the file.
    int file_size = (int)fs_specs.headers[lfd].length;
    // If we have reached the end of the file, return.
    if (read_pos == file_size) {
        return EOF;
    }
    // Get the begin of the file.
    char *file_start = (char *)(modules[0].mod_start + fs_specs.headers[lfd].offset);
    // Declare an iterator, used afterward to return the number of bytes read.
    ssize_t it = 0;
    while ((it < nbyte) && (read_pos < file_size)) {
        *buf++ = file_start[read_pos];
        ++read_pos;
        ++it;
    }
    return it;
}

static int _initrd_stat(const initrd_file_t *file, stat_t *stat)
{
    stat->st_dev   = 0;
    stat->st_ino   = file - fs_specs.headers;
    stat->st_mode  = file->mask;
    stat->st_uid   = file->uid;
    stat->st_gid   = file->gid;
    stat->st_atime = file->atime;
    stat->st_mtime = file->mtime;
    stat->st_ctime = file->ctime;
    stat->st_size  = file->length;
    return 0;
}

/// @brief Retrieves information concerning the file at the given position.
/// @param file The file struct.
/// @param stat The structure where the information are stored.
/// @return 0 if success.
static int initrd_fstat(vfs_file_t *file, stat_t *stat)
{
    return _initrd_stat(&fs_specs.headers[file->ino], stat);
}

/// @brief Retrieves information concerning the file at the given position.
/// @param path The path where the file resides.
/// @param stat The structure where the information are stored.
/// @return 0 if success.
static int initrd_stat(const char *path, stat_t *stat)
{
    int i;
    i = 0;

    while (i < INITRD_MAX_FILES) {
        if (!strcmp(path, fs_specs.headers[i].fileName)) {
            stat->st_uid  = fs_specs.headers[i].uid;
            stat->st_size = fs_specs.headers[i].length;
            break;
        }
        i++;
    }

    if (i == INITRD_MAX_FILES) {
        return -ENOENT;
    } else {
        return _initrd_stat(&fs_specs.headers[i], stat);
    }
}

/// @brief Writes the given content inside the file.
/// @param file   The file descriptor of the file.
/// @param buf    The content to write.
/// @param offset Offset from which we start writing in the file.
/// @param nbyte  The number of bytes to write.
/// @return The number of written bytes.
static ssize_t initrd_write(vfs_file_t *file, const void *buf, off_t offset, size_t nbyte)
{
    // Get the header.
    initrd_file_t *header = &fs_specs.headers[file->ino];
    // If the number of byte to write is zero, skip.
    if (nbyte == 0) {
        return 0;
    }
    if (check_if_occupied(offset + nbyte)) {
        pr_emerg("We need to move the file.\n");
        TODO("Implement file movement.");
    }
    // Prepare pointers to the contents.
    char *dest = (char *)(&fs_specs + header->offset + offset), *src = (char *)buf;
    // Copy the content.
    int num = 0;
    while ((num < nbyte) && (*dest++ = *src++)) {
        ++num;
    }
    dest[num]     = '\0';
    dest[num + 1] = EOF;
    // Increment the length of the file.
    header->length += num;
    return num;
}

/// @brief Repositions the file offset inside a file.
/// @param file the file we are working with.
/// @param offset the offest to use for the operation. 
/// @param whence the type of operation.
/// @return  Upon successful completion, returns the resulting offset
/// location as measured in bytes from the beginning of the file. On
/// error, the value (off_t) -1 is returned and errno is set to
/// indicate the error.
static off_t initrd_lseek(vfs_file_t *file, off_t offset, int whence)
{
    // Get the header.
    initrd_file_t *header = &fs_specs.headers[file->ino];

    switch (whence) {
    case SEEK_END:
        offset += header->length;
        break;
    case SEEK_CUR:
        if (offset == 0) {
            return file->f_pos;
        }
        offset += file->f_pos;
        break;
    case SEEK_SET:
        break;
    default:
        return -EINVAL;
    }
    if (offset >= 0) {
        if (offset != file->f_pos) {
            file->f_pos = offset;
        }
        return offset;
    }
    return -EINVAL;
}

/// @brief Closes the given file.
/// @param file The file structure.
static int initrd_close(vfs_file_t *file)
{
    assert(file && "Received null file.");
    // Remove the file from the list of the corresponding entry inside the `header_files`.
    list_head_del(&file->siblings);
    // Free the memory of the file.
    kmem_cache_free(file);
    return 0;
}

/// Filesystem general operations.
static vfs_sys_operations_t initrd_sys_operations = {
    .mkdir_f = initrd_mkdir,
    .rmdir_f = initrd_rmdir,
    .stat_f  = initrd_stat
};

/// Filesystem file operations.
static vfs_file_operations_t initrd_fs_operations = {
    .open_f     = initrd_open,
    .unlink_f   = initrd_unlink,
    .close_f    = initrd_close,
    .read_f     = initrd_read,
    .write_f    = initrd_write,
    .lseek_f    = initrd_lseek,
    .stat_f     = initrd_fstat,
    .ioctl_f    = NULL,
    .getdents_f = initrd_getdents
};

static vfs_file_t *initrd_create_file_struct(
    ino_t ino,
    const char *name,
    size_t size,
    int flags)
{
    vfs_file_t *file = kmem_cache_alloc(vfs_file_cache, GFP_KERNEL);
    if (!file) {
        pr_err("Failed to allocation memory for the file.");
        return NULL;
    }
    memset(file, 0, sizeof(vfs_file_t));

    strcpy(file->name, name);
    file->device         = (void *)file;
    file->ino            = ino;
    file->uid            = 0;
    file->gid            = 0;
    file->mask           = S_IRUSR | S_IRGRP | S_IROTH;
    file->length         = size;
    file->flags          = flags;
    file->sys_operations = &initrd_sys_operations;
    file->fs_operations  = &initrd_fs_operations;

    return file;
}

static vfs_file_t *initrd_mount_callback(const char *path, const char *device)
{
    dump_initrd_fs();
    // Create the associated file.
    vfs_file_t *vfs_file = initrd_create_file_struct(0, path, 0, DT_DIR);
    assert(vfs_file && "Failed to create vfs_file.");
    // Initialize the proc_root.
    return vfs_file;
}

/// Filesystem information.
static file_system_type initrd_file_system_type = {
    .name     = "initrd",
    .fs_flags = 0,
    .mount    = initrd_mount_callback
};

int initrd_init_module(void)
{
    for (int i = 0; i < MAX_MODULES; ++i) {
        if (strcmp((char *)modules[i].cmdline, "initrd") == 0) {
            assert(sizeof(struct initrd_t) <= (modules[i].mod_end - modules[i].mod_start));
            // Copy the FS specification.
            memcpy(&fs_specs, (void *)modules[i].mod_start, sizeof(struct initrd_t));
            // Register the filesystem.
            vfs_register_filesystem(&initrd_file_system_type);
        }
    }
    return 0;
}

int initrd_cleanup_module(void)
{
    vfs_unregister_filesystem(&initrd_file_system_type);
    return 0;
}