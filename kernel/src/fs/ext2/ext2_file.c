/// @file ext2_file.c
/// @brief EXT2 operations on an open file.
/// @copyright (c) 2014-2026 This file is distributed under the MIT License.
/// See LICENSE.md for details.

// Setup the logging for this file (do this before any other include).
#include "sys/kernel_levels.h"           // Include kernel log levels.
#define __DEBUG_HEADER__ "[EXT2  ]"      ///< Change header.
#define __DEBUG_LEVEL__  LOGLEVEL_NOTICE ///< Set log level.
#include "io/debug.h"                    // Include debugging functions.

#include "ext2_internal.h"

/// @brief Creates a new file or rewrite an existing one.
/// @param path path to the file.
/// @param mode mode for file creation.
/// @return file descriptor number, -1 otherwise and errno is set to indicate the error.
/// @details
/// It is equivalent to: open(path, O_WRONLY|O_CREAT|O_TRUNC, mode)
vfs_file_t *ext2_creat(const char *path, mode_t mode)
{
    pr_debug("ext2_creat(path: `%s`, mode: %d)\n", path, mode);
    // Get the name of the directory.
    char parent_path[PATH_MAX];
    if (!dirname(path, parent_path, sizeof(parent_path))) {
        errno = ENAMETOOLONG;
        return NULL;
    }
    const char *file_name = basename(path);
    if (strcmp(parent_path, path) == 0) {
        // A path that is its own parent directory is the root, and creating
        // the root means creating a directory: EISDIR is what POSIX asks for.
        errno = EISDIR;
        return NULL;
    }
    // Get the parent VFS node.
    vfs_file_t *parent = vfs_open(parent_path, O_WRONLY, 0);
    if (parent == NULL) {
        errno = ENOENT;
        return NULL;
    }
    // What has to be undone when a later step fails. The inode is allocated
    // in the bitmap before anything points at it, and its name is placed
    // before the VFS file exists, so a failure in between used to leave the
    // inode marked used with no name, or the name on disk while creat
    // reported failure (#305).
    int inode_index      = -1;
    int has_direntry     = 0;
    vfs_file_t *new_file = NULL;
    // The error to report, as a negative errno. Every path out of here used
    // to return NULL without touching errno, leaving the caller to read
    // whatever the last unrelated failure had set.
    int failure          = 0;
    // The file to hand back on success.
    vfs_file_t *result   = NULL;

    // flags = O_WRONLY | O_CREAT | O_TRUNC
    // Get the filesystem.
    ext2_filesystem_t *fs = (ext2_filesystem_t *)parent->device;
    if (fs == NULL) {
        pr_err("The parent does not belong to an EXT2 filesystem `%s`.\n", parent->name);
        failure = -ENODEV;
        goto rollback;
    }
    // Prepare an inode, it will come in handy either way.
    ext2_inode_t inode;
    // Prepare the structure for the search.
    ext2_direntry_search_t search;
    memset(&search, 0, sizeof(ext2_direntry_search_t));
    // Search if the entry already exists.
    if (!ext2_find_direntry(fs, parent->ino, file_name, &search)) {
        if (ext2_read_inode(fs, &inode, search.direntry.inode) == -1) {
            pr_err("Failed to read the inode of `%s`.\n", search.direntry.name);
            failure = -EIO;
            goto rollback;
        }
        vfs_file_t *file = ext2_find_vfs_file_with_inode(fs, search.direntry.inode);
        if (file == NULL) {
            // Allocate the memory for the file.
            file = vfs_alloc_file();
            if (file == NULL) {
                pr_err("Failed to allocate memory for the EXT2 file.\n");
                failure = -ENOMEM;
                goto rollback;
            }
            // From here the file is ours to release if the next step fails.
            new_file = file;
            if (ext2_init_vfs_file(
                    fs, file, &inode, search.direntry.inode, search.direntry.name, search.direntry.name_len) == -1) {
                pr_err("Failed to properly set the VFS file.\n");
                failure = -EIO;
                goto rollback;
            }
            // Add the vfs_file to the list of associated files.
            list_head_insert_before(&file->siblings, &fs->opened_files);
            new_file = NULL;
        } else {
            // The list of open files is keyed by inode number, and the entry
            // may describe a file that was deleted while its number got
            // reused: refresh it from the inode we just read (#297).
            __ext2_set_vfs_file_properties(
                fs, file, &inode, search.direntry.inode, search.direntry.name, search.direntry.name_len);
        }
        result = file;
        goto done;
    }
    // Set the inode mode.
    mode                 = S_IFREG | (0xFFF & mode);
    // Get the group index of the parent.
    uint32_t group_index = ext2_inode_index_to_group_index(fs, parent->ino);
    // Create and initialize the new inode.
    inode_index          = ext2_create_inode(fs, &inode, mode, group_index);
    if (inode_index == -1) {
        pr_err("Failed to create a new inode inside `%s` (group index: %d).\n", parent->name, group_index);
        failure = -ENOSPC;
        goto rollback;
    }
    // Write the inode.
    if (ext2_write_inode(fs, &inode, inode_index) == -1) {
        pr_err("Failed to write the newly created inode.\n");
        failure = -EIO;
        goto rollback;
    }

    // Clean the content of the newly created file.
    if (ext2_clean_inode_content(fs, &inode, inode_index) < 0) {
        pr_err("Failed to clean the content of the newly created inode.\n");
        failure = -EIO;
        goto rollback;
    }

    // Initialize the file.
    if (ext2_allocate_direntry(fs, parent->ino, inode_index, file_name, ext2_file_type_regular_file) == -1) {
        pr_err("Failed to allocate a new direntry for the inode.\n");
        failure = -ENOSPC;
        goto rollback;
    }
    has_direntry = 1;
    // Allocate the memory for the file.
    new_file     = vfs_alloc_file();
    if (new_file == NULL) {
        pr_err("Failed to allocate memory for the EXT2 file.\n");
        failure = -ENOMEM;
        goto rollback;
    }
    if (ext2_init_vfs_file(fs, new_file, &inode, inode_index, file_name, strlen(file_name)) == -1) {
        pr_err("Failed to properly set the VFS file.\n");
        failure = -EIO;
        goto rollback;
    }
    result   = new_file;
    new_file = NULL;
done:
    // The reference taken on the parent is released on the way out, on this
    // path as much as on the failing one (#323).
    vfs_close(parent);
    return result;
rollback:
    // Take the name out of the parent before the inode goes away, so that no
    // entry is left pointing at a free inode.
    if (has_direntry && (__ext2_clear_direntry_for_path(fs, path) < 0)) {
        pr_err("ext2_creat(path: %s): Failed to remove the incomplete directory entry.\n", path);
    }
    // Give the inode back. Re-read it, because cleaning its content may have
    // given it blocks that have to be released along with it.
    if (inode_index >= 0) {
        ext2_inode_t stale;
        if (ext2_read_inode(fs, &stale, (uint32_t)inode_index) != -1) {
            stale.links_count = 0;
            stale.dtime       = sys_time(NULL);
            if (ext2_write_inode(fs, &stale, (uint32_t)inode_index) == -1) {
                pr_err("ext2_creat(path: %s): Failed to update the inode being freed.\n", path);
            }
            if (ext2_free_inode(fs, &stale, (uint32_t)inode_index) < 0) {
                pr_err("ext2_creat(path: %s): Failed to free the inode.\n", path);
            }
        } else {
            pr_err("ext2_creat(path: %s): Failed to re-read the inode to free it.\n", path);
        }
    }
    // Release the VFS file only when it is one we allocated and never handed
    // to anybody.
    if (new_file != NULL) {
        vfs_dealloc_file(new_file);
    }
    vfs_close(parent);
    errno = -failure;
    return NULL;
}

/// @brief Open the file at the given path and returns its file descriptor.
/// @param path  The path to the file.
/// @param flags The flags used to determine the behavior of the function.
/// @param mode  The mode with which we open the file.
/// @return The file descriptor of the opened file, otherwise returns -1.
vfs_file_t *ext2_open(const char *path, int flags, mode_t mode)
{
    pr_debug("ext2_open(path: '%s', flags: %d, mode: %d)\n", path, flags, mode);
    // Get the EXT2 filesystem.
    ext2_filesystem_t *fs = get_ext2_filesystem(path);
    if (fs == NULL) {
        pr_err(
            "ext2_open(path: '%s', flags: %d, mode: %d): Failed to get the "
            "EXT2 filesystem.\n",
            path, flags, mode);
        return NULL;
    }
    // Prepare the structure for the search.
    ext2_direntry_search_t search;
    memset(&search, 0, sizeof(ext2_direntry_search_t));
    // First check, if a file with the given name already exists.
    if (!ext2_resolve_path(fs->root, path, &search)) {
        if (bitmask_check(flags, O_CREAT) && bitmask_check(flags, O_EXCL)) {
            pr_err(
                "ext2_open(path: '%s', flags: %d, mode: %d): A file or "
                "directory already exists (O_CREAT | O_EXCL).\n",
                path, flags, mode);
            return NULL;
        }
        if (bitmask_check(flags, O_DIRECTORY) && (search.direntry.file_type != ext2_file_type_directory)) {
            pr_err(
                "ext2_open(path: '%s', flags: %d, mode: %d): Directory entry "
                "`%s` is not a directory.\n",
                path, flags, mode, search.direntry.name);
            errno = ENOTDIR;
            return NULL;
        }
    } else {
        // If we need to create it, it's ok if it does not exist.
        if (bitmask_check(flags, O_CREAT)) {
            return ext2_creat(path, mode);
        }
        pr_err(
            "ext2_open(path: '%s', flags: %d, mode: %d): The file does not "
            "exist.\n",
            path, flags, mode);
        errno = ENOENT;
        return NULL;
    }
    // Prepare the structure for the inode.
    ext2_inode_t inode;
    memset(&inode, 0, sizeof(ext2_inode_t));
    // Get the inode associated with the directory entry.
    if (ext2_read_inode(fs, &inode, search.direntry.inode) == -1) {
        pr_err(
            "ext2_open(path: '%s', flags: %d, mode: %d): Failed to read the "
            "inode.\n",
            path, flags, mode);
        return NULL;
    }

    if (!vfs_valid_open_permissions(flags, inode.mode, inode.uid, inode.gid)) {
        pr_err(
            "ext2_open(path: '%s', flags: %d, mode: %d): Task does not have "
            "access permission.\n",
            path, flags, mode);
        errno = EACCES;
        return NULL;
    }

    // Check if the file is a regular file, and the user wants to write and truncate.
    if (bitmask_exact(inode.mode, S_IFREG) &&
        (bitmask_exact(flags, O_RDWR | O_TRUNC) || bitmask_exact(flags, O_RDONLY | O_TRUNC))) {
        // Clean the content of the newly created file.
        if (ext2_clean_inode_content(fs, &inode, search.direntry.inode) < 0) {
            pr_err(
                "ext2_open(path: '%s', flags: %d, mode: %d): Failed to clean "
                "the content of the newly created inode.\n",
                path, flags, mode);
            return NULL;
        }
    }

    vfs_file_t *file = ext2_find_vfs_file_with_inode(fs, search.direntry.inode);
    if (file != NULL) {
        // The list of open files is keyed by inode number, and the entry may
        // describe a file that was deleted while its number got reused:
        // refresh it from the inode we just read (#297).
        __ext2_set_vfs_file_properties(
            fs, file, &inode, search.direntry.inode, search.direntry.name, search.direntry.name_len);
    }
    if (file == NULL) {
        // Allocate the memory for the file.
        file = vfs_alloc_file();
        if (file == NULL) {
            pr_err(
                "ext2_open(path: '%s', flags: %d, mode: %d): Failed to "
                "allocate memory for the EXT2 file.\n",
                path, flags, mode);
            return NULL;
        }
        if (ext2_init_vfs_file(
                fs, file, &inode, search.direntry.inode, search.direntry.name, search.direntry.name_len) == -1) {
            pr_err(
                "ext2_open(path: '%s', flags: %d, mode: %d): Failed to "
                "properly set the VFS file.\n",
                path, flags, mode);
            return NULL;
        }
        // Add the vfs_file to the list of associated files.
        list_head_insert_before(&file->siblings, &fs->opened_files);
    }
    return file;
}

/// @brief Closes the given file.
/// @param file The file structure.
/// @return 0 on success, -errno on failure.
int ext2_close(vfs_file_t *file)
{
    // Validate the file pointer.
    if (file == NULL) {
        pr_err("ext2_close: Invalid file pointer (NULL).\n");
        return -EINVAL; // Invalid argument error
    }

    // Get the filesystem from the device.
    ext2_filesystem_t *fs = (ext2_filesystem_t *)file->device;
    if (fs == NULL) {
        pr_err(
            "ext2_close: File `%s` does not belong to a valid EXT2 "
            "filesystem.\n",
            file->name);
        return -EINVAL;
    }

    pr_debug("ext2_close(ino: %d, file: \"%s\", count: %d)\n", file->ino, file->name, file->count - 1);

    // Decrement the reference count for the file and close if last reference.
    if (--file->count == 0) {
        // Ensure we are not trying to free the memory of the root.
        if (file == fs->root) {
            pr_warning("ext2_close: Attempted to close the root file `%s`.\n", file->name);
            return -EPERM;
        }

        pr_debug("ext2_close: Closing file `%s` (ino: %d).\n", file->name, file->ino);

        // Remove the file from the list of opened files.
        list_head_remove(&file->siblings);
        pr_debug("ext2_close: Removed file `%s` from the opened file list.\n", file->name);

        // Free the file from cache.
        pr_debug("ext2_close: Freeing memory for file `%s`.\n", file->name);
        vfs_dealloc_file(file);
    }

    return 0;
}

/// @brief Reads from the file identified by the file descriptor.
/// @param file The file.
/// @param buffer Buffer where the read content must be placed.
/// @param offset Offset from which we start reading from the file.
/// @param nbyte The number of bytes to read.
/// @return The number of red bytes.
ssize_t ext2_read(vfs_file_t *file, char *buffer, off_t offset, size_t nbyte)
{
#ifdef EXT2_FULL_DEBUG
    pr_debug("ext2_read(file: %s, offset: %4u, nbyte: %4u)\n", file->name, offset, nbyte);
#endif
    // Get the filesystem.
    ext2_filesystem_t *fs = (ext2_filesystem_t *)file->device;
    if (fs == NULL) {
        pr_err("The file does not belong to an EXT2 filesystem `%s`.\n", file->name);
        return -1;
    }
    // Get the inode associated with the file.
    ext2_inode_t inode;
    if (ext2_read_inode(fs, &inode, file->ino) == -1) {
        pr_err("Failed to read the inode `%s`.\n", file->name);
        return -1;
    }
    // Disallow reading directories using read
    if ((inode.mode & S_IFDIR) == S_IFDIR) {
        pr_err("Reading a directory `%s` is not allowed.\n", file->name);
        return -EISDIR;
    }
    return ext2_read_inode_data(fs, &inode, file->ino, offset, nbyte, buffer);
}

/// @brief Writes the given content inside the file.
/// @param file The file descriptor of the file.
/// @param buffer The content to write.
/// @param offset Offset from which we start writing in the file.
/// @param nbyte The number of bytes to write.
/// @return The number of written bytes.
ssize_t ext2_write(vfs_file_t *file, const void *buffer, off_t offset, size_t nbyte)
{
#ifdef EXT2_FULL_DEBUG
    pr_debug("ext2_write(file: %s, offset: %4u, nbyte: %4u)\n", file->name, offset, nbyte);
#endif
    // Get the filesystem.
    ext2_filesystem_t *fs = (ext2_filesystem_t *)file->device;
    if (fs == NULL) {
        pr_err("The file does not belong to an EXT2 filesystem `%s`.\n", file->name);
        return -1;
    }
    // Get the inode associated with the file.
    ext2_inode_t inode;
    if (ext2_read_inode(fs, &inode, file->ino) == -1) {
        pr_err("Failed to read the inode `%s`.\n", file->name);
        return -1;
    }
    ssize_t written = ext2_write_inode_data(fs, &inode, file->ino, offset, nbyte, (char *)buffer);
    if (written < 0) {
        pr_err("Failed to write on file %s.\n", file->name);
    } else {
        // Update the file length.
        file->length = inode.size;
    }
    return written;
}

/// @brief Repositions the file offset inside a file.
/// @param file the file we are working with.
/// @param offset the offest to use for the operation.
/// @param whence the type of operation.
/// @return  Upon successful completion, returns the resulting offset
/// location as measured in bytes from the beginning of the file. On
/// error, the value (off_t) -1 is returned and errno is set to
/// indicate the error.
off_t ext2_lseek(vfs_file_t *file, off_t offset, int whence)
{
    pr_debug("ext2_lseek(file: %s, offset: %4u, whence: %4u)\n", file->name, offset, whence);
    // Get the filesystem.
    ext2_filesystem_t *fs = (ext2_filesystem_t *)file->device;
    if (fs == NULL) {
        pr_err("The file does not belong to an EXT2 filesystem `%s`.\n", file->name);
        return -EPERM;
    }
    // Get the inode associated with the file.
    ext2_inode_t inode;
    if (ext2_read_inode(fs, &inode, file->ino) == -1) {
        pr_err("Failed to read the inode `%s`.\n", file->name);
        return -ENOENT;
    }
    // Deal with the specific whence.
    switch (whence) {
    case SEEK_END:
        offset += inode.size;
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

/// @brief Perform the I/O control operation specified by REQUEST on FD. One
/// argument may follow; its presence and type depend on REQUEST.
/// @param file the file on which we perform the operations.
/// @param request the device-dependent request code
/// @param data an untyped pointer to memory.
/// @return Return value depends on REQUEST. Usually -1 indicates error.
long ext2_ioctl(vfs_file_t *file, unsigned int request, unsigned long data) { return -1; }

/// @brief Reads contents of the directories to a dirent buffer, updating
///        the offset and returning the number of written bytes in the buffer,
///        it assumes that all paths are well-formed.
/// @param file  The directory handler.
/// @param dirp  The buffer where the data should be written.
/// @param doff  The offset inside the buffer where the data should be written.
/// @param count The maximum length of the buffer.
/// @return The number of written bytes in the buffer.
ssize_t ext2_getdents(vfs_file_t *file, dirent_t *dirp, off_t doff, size_t count)
{
    pr_debug("ext2_getdents(file: %s, doff: %4u, count: %4u)\n", file->name, doff, count);
    // Get the filesystem.
    ext2_filesystem_t *fs = (ext2_filesystem_t *)file->device;
    if (fs == NULL) {
        pr_err("The file does not belong to an EXT2 filesystem `%s`.\n", file->name);
        return -ENOENT;
    }
    // Get the inode associated with the file.
    ext2_inode_t inode;
    if (ext2_read_inode(fs, &inode, file->ino) == -1) {
        pr_err("Failed to read the inode (%d).\n", file->ino);
        return -ENOENT;
    }
    uint32_t current = 0;
    ssize_t written  = 0;
    // Allocate the cache.
    uint8_t *cache   = ext2_alloc_cache(fs);

    // Initialize the iterator.
    ext2_direntry_iterator_t it = ext2_direntry_iterator_begin(fs, cache, &inode);
    for (; ext2_direntry_iterator_valid(&it) && (written < count); ext2_direntry_iterator_next(&it)) {
        // Skip unused inode.
        if (it.direntry->inode == 0) {
            continue;
        }
        // Skip if already provided.
        current += sizeof(dirent_t);
        if (current <= doff) {
            continue;
        }
        // Write on current directory entry data.
        dirp->d_ino  = it.direntry->inode;
        dirp->d_type = ext2_file_type_to_vfs_file_type(it.direntry->file_type);
        memset(dirp->d_name, 0, NAME_MAX);
        strncpy(dirp->d_name, it.direntry->name, it.direntry->name_len);
        dirp->d_off    = it.direntry->rec_len;
        dirp->d_reclen = it.direntry->rec_len;
        // Increment the amount written.
        written += sizeof(dirent_t);
        // Move to next writing position.
        ++dirp;
    }
    // Free the cache.
    ext2_dealloc_cache(cache);
    return written;
}

/// @brief Read the symbolic link, if present.
/// @param path The path to the file for which we want to read the symbolic link information.
/// @param buffer The buffer where we will store the symbolic link path.
/// @param bufsize The size of the buffer.
/// @return The number of read characters on success, -1 on failure, with errno set to indicate the error.
ssize_t ext2_readlink(const char *path, char *buffer, size_t bufsize)
{
    // Check if path and buffer are valid
    if (!path || !buffer || bufsize == 0) {
        pr_err("Invalid arguments: path or buffer is NULL or bufsize is zero.\n");
        return -EINVAL;
    }

    pr_debug("ext2_readlink(path: %s)\n", path);

    // Get the EXT2 filesystem associated with the given path
    ext2_filesystem_t *fs = get_ext2_filesystem(path);
    if (!fs) {
        pr_err("ext2_readlink(path: %s): Failed to get the EXT2 filesystem.\n", path);
        return -ENOENT;
    }

    // Initialize the structure used for searching the directory entry.
    ext2_direntry_search_t search;
    memset(&search, 0, sizeof(ext2_direntry_search_t));

    // Resolve the given path to find the associated directory entry.
    if (ext2_resolve_path(fs->root, path, &search) != 0) {
        pr_err("ext2_readlink(path: %s): Failed to resolve path.\n", path);
        return -ENOENT;
    }

    // Read the inode associated with the directory entry.
    ext2_inode_t inode;
    if (ext2_read_inode(fs, &inode, search.direntry.inode) == -1) {
        pr_err("ext2_readlink(path: %s): Failed to read the inode (%d).\n", path, search.direntry.inode);
        return -ENOENT;
    }

    // Check if the inode represents a symbolic link.
    if (!S_ISLNK(inode.mode)) {
        pr_err("ext2_readlink(path: %s): The file is not a symbolic link.\n", path);
        return -EINVAL;
    }

    // Determine the number of characters to read (symlink length or buffer
    // size, whichever is smaller).
    ssize_t ret = min(strlen(inode.data.symlink), bufsize);

    // Copy the symbolic link content to the buffer.
    strncpy(buffer, inode.data.symlink, ret);

    // Null-terminate the buffer if there's space.
    if (ret < bufsize) {
        buffer[ret] = '\0';
    }

    // Return the number of characters read.
    return ret;
}
