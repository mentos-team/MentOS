/// @file pipe.c
/// @brief PIPE functions and structures implementation.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

// ============================================================================
// Setup the logging for this file (do this before any other include).
#include "sys/kernel_levels.h"          // Include kernel log levels.
#define __DEBUG_HEADER__ "[PIPE  ]"     ///< Change header.
#define __DEBUG_LEVEL__  LOGLEVEL_DEBUG ///< Set log level.
#include "io/debug.h"                   // Include debugging functions.
// ============================================================================

#include "fs/pipe.h"

#include "assert.h"
#include "fcntl.h"
#include "mem/kheap.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "sys/errno.h"
#include "strerror.h"
#include "sys/list_head.h"
#include "fs/vfs.h"
#include "sys/stat.h"
#include "fcntl.h"
#include "time.h"
#include "klib/hashmap.h"
#include "system/syscall.h"

// ============================================================================
// Virtual FileSystem (VFS) Operaions
// ============================================================================

static int pipe_buffer_confirm(pipe_inode_info_t *, size_t);

static int pipe_stat(const char *path, stat_t *stat);

static vfs_file_t *pipe_open(const char *path, int flags, mode_t mode);
static int pipe_unlink(const char *path);
static int pipe_close(vfs_file_t *file);
static ssize_t pipe_read(vfs_file_t *file, char *buffer, off_t offset, size_t nbyte);
static ssize_t pipe_write(vfs_file_t *file, const void *buffer, off_t offset, size_t nbyte);
static off_t pipe_lseek(vfs_file_t *file, off_t offset, int whence);
static int pipe_fstat(vfs_file_t *file, stat_t *stat);

/// @brief Operations for managing pipe buffers in the kernel.
static struct pipe_buf_operations pipe_buf_ops = {
    .confirm = pipe_buffer_confirm,
};

/// @brief Filesystem-level operations for managing pipes.
static vfs_sys_operations_t pipe_sys_operations = {
    .mkdir_f   = NULL,
    .rmdir_f   = NULL,
    .stat_f    = pipe_stat,
    .creat_f   = NULL,
    .symlink_f = NULL,
};

/// @brief File operations for pipe handling in the filesystem.
static vfs_file_operations_t pipe_fs_operations = {
    .open_f     = pipe_open,
    .unlink_f   = pipe_unlink,
    .close_f    = pipe_close,
    .read_f     = pipe_read,
    .write_f    = pipe_write,
    .lseek_f    = pipe_lseek,
    .stat_f     = pipe_fstat,
    .ioctl_f    = NULL,
    .getdents_f = NULL,
    .readlink_f = NULL,
};

// static list_head named_pipes;

// ============================================================================
// MEMORY MANAGEMENT (Private)
// ============================================================================

/// @brief Initializes a pipe buffer.
/// @details Sets the initial values for a `pipe_buffer_t` structure and
/// allocates a memory page to hold the buffer's data.
/// @param pipe_buffer Pointer to the `pipe_buffer_t` structure to initialize.
/// @return 0 on success, -ENOMEM if page allocation fails.
static inline int __pipe_buffer_init(pipe_buffer_t *pipe_buffer)
{
    // Check if we received a valid pipe buffer.
    assert(pipe_buffer && "Received a null pipe buffer.");

    // Initialize the pipe buffer to zero.
    memset(pipe_buffer, 0, sizeof(pipe_buffer_t));

    // Initialize additional fields in the pipe_buffer_t structure.
    pipe_buffer->len    = 0;
    pipe_buffer->offset = 0;
    pipe_buffer->ops    = &pipe_buf_ops;

    return 0;
}

/// @brief De-initialize a pipe buffer.
/// @details Frees the memory pages used to store the buffer's data if the
/// reference count allows, and clears buffer fields.
/// @param pipe_buffer Pointer to the `pipe_buffer_t` structure to be
/// de-initialized. This should not be NULL.
static inline void __pipe_buffer_deinit(pipe_buffer_t *pipe_buffer)
{
    // Check if we received a valid pipe buffer.
    assert(pipe_buffer && "Received a null pipe buffer.");

    // Reset other fields for safety.
    pipe_buffer->len    = 0;
    pipe_buffer->offset = 0;
    pipe_buffer->ops    = NULL;
}

/// @brief Allocates and initializes a new `pipe_inode_info_t` structure.
/// @details Allocates memory for a `pipe_inode_info_t` structure and
/// initializes its fields, including synchronization objects (wait queue and
/// mutex) and an array of `pipe_buffer_t` buffers. The buffer array size is
/// based on the `INITIAL_NUM_BUFFERS` constant.
/// @return Pointer to the allocated and initialized `pipe_inode_info_t`
/// structure, or NULL if allocation fails.
static inline pipe_inode_info_t *__pipe_inode_info_alloc(void)
{
    // Allocate memory for the pipe_inode_info_t structure.
    pipe_inode_info_t *pipe_info = (pipe_inode_info_t *)kmalloc(sizeof(pipe_inode_info_t));
    if (!pipe_info) {
        pr_err("Failed to allocate memory for pipe_inode_info_t.\n");
        return NULL;
    }

    // Zero initialize the allocated memory.
    memset(pipe_info, 0, sizeof(pipe_inode_info_t));

    // Initialize the wait queues.
    wait_queue_head_init(&pipe_info->read_wait);
    wait_queue_head_init(&pipe_info->write_wait);

    // Initialize the mutex.
    mutex_unlock(&pipe_info->mutex);

    // Set the number of buffers.
    pipe_info->numbuf = PIPE_NUM_BUFFERS;

    // Initialize each buffer in the buffer array.
    for (unsigned int i = 0; i < pipe_info->numbuf; ++i) {
        if (__pipe_buffer_init(&pipe_info->bufs[i])) {
            pr_err("Failed to initialize pipe buffer %u.\n", i);

            // Deinitialize and free previously initialized buffers
            while (i > 0) {
                __pipe_buffer_deinit(&pipe_info->bufs[--i]);
            }
            kfree(pipe_info);
            return NULL;
        }
    }

    // Initialize remaining fields.
    pipe_info->read_index  = 0;
    pipe_info->write_index = 0;
    pipe_info->readers     = 0;
    pipe_info->writers     = 0;

    return pipe_info;
}

/// @brief Deallocates the memory used by a `pipe_inode_info_t` structure.
/// @details Frees memory associated with a `pipe_inode_info_t` structure,
/// including each pipe buffer in the buffer array and the structure itself.
/// @param pipe_info Pointer to the `pipe_inode_info_t` structure to be
/// deallocated. This should not be NULL.
static inline void __pipe_inode_info_dealloc(pipe_inode_info_t *pipe_info)
{
    // Ensure that the provided pointer is valid.
    assert(pipe_info && "Received a NULL pointer.");

    // Free each buffer in the array.
    for (unsigned int i = 0; i < pipe_info->numbuf; ++i) {
        __pipe_buffer_deinit(&pipe_info->bufs[i]);
    }

    // Free the memory used by the pipe_inode_info_t structure itself.
    kfree(pipe_info);
}

// ============================================================================
// BUFFER OPERATIONS (Private)
// ============================================================================

/// @brief Converts a linear index to the corresponding buffer index within the buffer limit.
/// @param index The linear index.
/// @return The buffer index within [0, PIPE_NUM_BUFFERS - 1].
static inline size_t pipe_linear_to_buffer_index(size_t index)
{
    return (index / PIPE_BUFFER_SIZE) % PIPE_NUM_BUFFERS;
}

/// @brief Calculates the offset within the specified buffer from a linear index.
/// @param index The linear index.
/// @return The offset within the buffer.
static inline size_t pipe_linear_to_offset(size_t index)
{
    return index % PIPE_BUFFER_SIZE;
}

/// @brief Ensures that the buffer at the specified index is valid and ready to be used.
/// @param pipe_info Pointer to the pipe information structure.
/// @param index The index of the buffer to confirm.
/// @return 0 if the buffer is valid, or a non-zero error code if it is not.
static int pipe_buffer_confirm(pipe_inode_info_t *pipe_info, size_t index)
{
    // Ensure the pipe_info and index are valid.
    if (!pipe_info) {
        pr_err("pipe_buffer_confirm: pipe_info is NULL.\n");
        return -EINVAL;
    }
    if (index >= PIPE_NUM_BUFFERS) {
        pr_err("pipe_buffer_confirm: index %u out of bounds.\n", index);
        return -EINVAL;
    }

    pipe_buffer_t *buf = pipe_info->bufs + index;

    // Ensure length and offset are within valid bounds.
    if ((buf->len + buf->offset) > PIPE_BUFFER_SIZE) {
        pr_err("pipe_buffer_confirm: Buffer at index %u length and offset exceed bounds: len = %u, offset = %u, PIPE_BUFFER_SIZE = %lu.\n",
               index, buf->len, buf->offset, PIPE_BUFFER_SIZE);
        return -EOVERFLOW;
    }

    // Ensure operations pointer is valid.
    if (!buf->ops) {
        pr_err("pipe_buffer_confirm: Buffer operations pointer (buf->ops) at index %u is NULL.\n", index);
        return -ENXIO;
    }

    return 0;
}

/// @brief Checks if the specified pipe buffer is empty.
/// @param pipe_info Pointer to the pipe inode information structure.
/// @param index Index of the pipe buffer to check.
/// @return 1 if the buffer is empty, 0 otherwise.
static int pipe_buffer_empty(pipe_inode_info_t *pipe_info, unsigned int index)
{
    // Check if the index is within bounds
    if (index >= pipe_info->numbuf) {
        pr_err("Buffer index out of range.\n");
        // Treat out-of-bounds as empty to avoid access.
        return 1;
    }

    // Buffer is empty if len is 0
    return (pipe_info->bufs[index].len == 0);
}

/// @brief Calculates the number of bytes available in the specified pipe buffer.
/// @param pipe_info Pointer to the pipe inode information structure.
/// @param index Index of the pipe buffer.
/// @return Number of bytes available for reading.
static size_t pipe_buffer_available(pipe_inode_info_t *pipe_info, unsigned int index)
{
    // Check if the index is within bounds
    if (index >= pipe_info->numbuf) {
        pr_err("Buffer index out of range.\n");
        // No bytes available for out-of-bounds index.
        return 0;
    }

    // Return the number of bytes available in the specified buffer
    return pipe_info->bufs[index].len;
}

/// @brief Gets the remaining write capacity of the specified pipe buffer.
/// @param pipe_info Pointer to the pipe inode information structure.
/// @param index Index of the pipe buffer to check.
/// @return Remaining capacity in bytes.
static inline size_t pipe_buffer_capacity(pipe_inode_info_t *pipe_info, unsigned int index)
{
    // Check if the index is within bounds
    if (index >= pipe_info->numbuf) {
        pr_err("Buffer index out of range.\n");
        // No bytes available for out-of-bounds index.
        return 0;
    }

    return PIPE_BUFFER_SIZE - pipe_info->bufs[index].len;
}

/// @brief Reads data from the specified pipe buffer into the provided buffer.
/// @param pipe_info Pointer to the pipe inode information structure.
/// @param index Index of the pipe buffer to read from.
/// @param dest Destination buffer where the data will be copied.
/// @param count Maximum number of bytes to read.
/// @return Number of bytes read, or -1 on error.
static ssize_t pipe_buffer_read(pipe_inode_info_t *pipe_info, unsigned int index, char *dest, size_t count)
{
    // Validate input parameters.
    if (!pipe_info) {
        pr_err("pipe_buffer_read: pipe_info is NULL.\n");
        return -1;
    }
    if (!dest) {
        pr_err("pipe_buffer_read: Destination buffer is NULL.\n");
        return -1;
    }
    if (index >= pipe_info->numbuf) {
        pr_err("pipe_buffer_read: Buffer index %u out of range (max: %u).\n", index, pipe_info->numbuf);
        return -1;
    }

    // Get the specified buffer
    pipe_buffer_t *buffer = pipe_info->bufs + index;

    // Ensure the buffer contains data to read.
    if (buffer->len == 0) {
        pr_debug("pipe_buffer_read: No data available in buffer %u.\n", index);
        return 0;
    }

    // Check if the buffer's offset and length are within valid bounds
    if (buffer->offset + buffer->len > PIPE_BUFFER_SIZE) {
        pr_err("pipe_buffer_read: Buffer offset %u and length %u exceed buffer size.\n", buffer->offset, buffer->len);
        return -1;
    }

    // Limit the read size to the available data or requested count, whichever is smaller.
    ssize_t to_read = (count < buffer->len) ? count : buffer->len;

    pr_debug("pipe_buffer_read: Reading %u bytes from buffer %u with offset %u (length: %u).\n",
             to_read, index, buffer->offset, buffer->len);

    // Copy data from the pipe buffer's data at the specified offset
    memcpy(dest, buffer->data + buffer->offset, to_read);

    // Adjust buffer's offset and length to reflect the data consumption.
    buffer->offset += to_read;
    buffer->len -= to_read;

    pr_debug("pipe_buffer_read: Updated buffer %u - new offset: %u, new length: %u.\n", index, buffer->offset, buffer->len);

    // Return the number of bytes read
    return to_read;
}

/// @brief Writes data to the specified pipe buffer.
/// @param pipe_info Pointer to the pipe inode information structure.
/// @param index Index of the pipe buffer to write to.
/// @param src Source buffer containing data to write.
/// @param count Maximum number of bytes to write.
/// @return Number of bytes written, or -1 on error.
static int pipe_buffer_write(pipe_inode_info_t *pipe_info, unsigned int index, const char *src, size_t count)
{
    // Check if the index is within bounds
    if (!pipe_info) {
        pr_err("pipe_buffer_write: pipe_info is NULL.\n");
        return -1;
    }
    if (!src) {
        pr_err("pipe_buffer_write: Source buffer is NULL.\n");
        return -1;
    }
    if (index >= pipe_info->numbuf) {
        pr_err("pipe_buffer_write: Buffer index %u out of range (max: %u).\n", index, pipe_info->numbuf);
        return -1;
    }

    // Get the specified buffer
    pipe_buffer_t *buffer = pipe_info->bufs + index;

    // Ensure the buffer offset and length are within valid bounds
    if ((buffer->offset + buffer->len) > PIPE_BUFFER_SIZE) {
        pr_err("pipe_buffer_write: Buffer overflow detected (offset: %u, length: %u).\n", buffer->offset, buffer->len);
        return -1;
    }

    // Calculate available space in the buffer
    ssize_t available_space = PIPE_BUFFER_SIZE - buffer->len - buffer->offset;
    if (available_space <= 0) {
        pr_debug("pipe_buffer_write: Buffer %u is full, no space available to write.\n", index);
        return 0; // No space to write, return 0 for non-blocking behavior
    }

    // Determine the number of bytes to write
    ssize_t to_write = (count < available_space) ? count : available_space;

    pr_debug("pipe_buffer_write: Writing %d bytes to buffer %u (offset: %u, length: %u, available space: %u).\n",
             to_write, index, buffer->offset, buffer->len, available_space);

    // Copy data to the pipe buffer's data array at the current offset + length
    memcpy(buffer->data + buffer->offset + buffer->len, src, to_write);

    // Update the buffer's length to reflect the added data
    buffer->len += to_write;

    pr_debug("pipe_buffer_write: Buffer %u updated - new length: %u.\n", index, buffer->len);

    // Return the number of bytes written
    return to_write;
}

// ============================================================================
// Virtual FileSystem (VFS) Functions
// ============================================================================

static inline int is_pipe_nonblocking(vfs_file_t *file)
{
    return (file->flags & O_NONBLOCK) != 0;
}

/// @brief Creates a VFS file structure for a pipe.
/// @param path Path to the file (not used here but may be needed elsewhere).
/// @param flags Open flags (e.g., O_RDONLY, O_WRONLY).
/// @param mode File permissions (e.g., 0666 for read/write for all).
/// @return Pointer to the newly created VFS file, or NULL on failure.
static inline vfs_file_t *pipe_create_file_struct(const char *path, int flags, mode_t mode)
{
    // Allocate memory for the VFS file structure.
    vfs_file_t *vfs_file = kmem_cache_alloc(vfs_file_cache, GFP_KERNEL);
    if (!vfs_file) {
        pr_err("Failed to allocate memory for VFS file!\n");
        return NULL;
    }

    // Initialize all fields to zero.
    memset(vfs_file, 0, sizeof(vfs_file_t));

    // Assign the path if necessary.
    if (path) {
        memcpy(vfs_file->name, path, strlen(path) + 1);
        vfs_file->name[strlen(path) + 1] = 0;
    } else {
        memset(vfs_file->name, 0, sizeof(vfs_file->name));
    }

    // Set the file type to FIFO and apply the flags provided by the caller.
    // Only keep relevant access mode bits.
    vfs_file->flags = S_IFIFO | (flags & O_ACCMODE);

    // Set the permissions mask with the effective mode. Set only permission
    // bits.
    vfs_file->mask = mode & 0777;

    // Set operations for system and filesystem functions specific to pipes.
    vfs_file->sys_operations = &pipe_sys_operations;
    vfs_file->fs_operations  = &pipe_fs_operations;

    // Initialize file metadata fields as required.
    vfs_file->refcount = 1; // Initial reference count.
    vfs_file->f_pos    = 0; // Start position for file operations.

    // Initialize the list head for siblings.
    list_head_init(&vfs_file->siblings);

    // Optionally, set other metadata fields (timestamps can be set here if needed).
    vfs_file->atime = sys_time(NULL); // Set access time.
    vfs_file->mtime = sys_time(NULL); // Set modification time.
    vfs_file->ctime = sys_time(NULL); // Set change time.

    // Set the count to opne.
    vfs_file->count = 1;

    return vfs_file;
}

/// @brief Retrieves statistics for a pipe file.
/// @param path Path to the pipe file.
/// @param stat Pointer to the stat structure where the statistics will be stored.
/// @return 0 on success, or a negative error code on failure.
static int pipe_stat(const char *path, stat_t *stat)
{
    // Validate the input parameters.
    if (!path || !stat) {
        return -EINVAL;
    }

    // Placeholder: Fetch file info based on the path.
    // In a real implementation, retrieve file info, file size, permissions, etc.
    memset(stat, 0, sizeof(stat_t)); // Initialize the stat structure.

    // Populate stat information (e.g., file type, permissions, size).
    stat->st_mode  = S_IFIFO | 0644; // Indicate a FIFO (pipe) with rw-r--r-- permissions.
    stat->st_size  = 0;              // Pipes typically have a size of 0.
    stat->st_nlink = 1;              // Single link count for pipes.

    return 0;
}

static vfs_file_t *pipe_open(const char *path, int flags, mode_t mode)
{
    // Validate the path parameter.
    if (!path) {
        pr_err("Invalid path: path is NULL.\n");
        return NULL;
    }

    // Retrieve the current task structure.
    task_struct *task = scheduler_get_current_process();
    assert(task && "Failed to retrieve current task.");

    // Iterate through the file descriptors in the task
    for (int fd = 0; fd < task->max_fd; fd++) {
        // Get the file.
        vfs_file_t *file = task->fd_list[fd].file_struct;
        // Check if the file descriptor is associated with a pipe.
        if (file && S_ISFIFO(file->flags) && (strcmp(file->name, path) == 0)) {
            // Check if the requested flags match existing file access mode.
            if ((flags & O_ACCMODE) != (file->flags & O_ACCMODE)) {
                pr_err("Requested flags do not match existing pipe access mode.\n");
                return NULL;
            }
            return file; // Return existing pipe file.
        }
    }

    // Allocate and initialize a new vfs_file_t structure for the pipe.
    vfs_file_t *new_file = pipe_create_file_struct(path, flags, mode);
    if (!new_file) {
        pr_err("Failed to allocate memory for vfs_file_t structure for path: %s.\n", path);
        return NULL;
    }

    // Allocate and initialize the pipe_inode_info structure.
    pipe_inode_info_t *pipe_info = __pipe_inode_info_alloc();
    if (!pipe_info) {
        pr_err("Failed to allocate memory for pipe_inode_info structure.\n");
        // Free allocated new_file structure before returning.
        kfree(new_file);
        return NULL;
    }

    // Link the pipe_info to the vfs_file structure.
    new_file->device = pipe_info;

    // Confirm each buffer in pipe_info is properly initialized.
    for (size_t index = 0; index < pipe_info->numbuf; ++index) {
        int ret = pipe_buffer_confirm(pipe_info, index);
        if (ret < 0) {
            pr_err("Buffer confirmation failed for buffer %u with error code %d: %s.\n", index, ret, strerror(-ret));
            // Free pipe info structure.
            __pipe_inode_info_dealloc(pipe_info);
            // Free allocated new_file structure before returning.
            kfree(new_file);
            return NULL;
        }
    }

    return new_file;
}

/// @brief Unlinks (closes) an opened pipe file with the specified path.
/// @param path Path of the pipe file to unlink.
/// @return 0 on success, -1 on failure if pipe is not found or other error occurs.
static int pipe_unlink(const char *path)
{
    // Validate the path parameter.
    if (!path) {
        pr_err("pipe_unlink: Invalid path - path is NULL.\n");
        return -1;
    }

    pr_debug("pipe_unlink: Attempting to unlink pipe with path: %s\n", path);

    // Retrieve the current task structure.
    task_struct *task = scheduler_get_current_process();
    assert(task && "Failed to retrieve current task.");

    // Iterate through the file descriptors in the task
    for (int fd = 0; fd < task->max_fd; fd++) {
        // Get the file.
        vfs_file_t *file = task->fd_list[fd].file_struct;

        // Check if the file name matches the specified path.
        if (file && strcmp(file->name, path) == 0) {
            pr_debug("pipe_unlink: Pipe file found for path %s at FD %d.\n", path, fd);

            // Remove from the task's pipe list.
            pr_debug("pipe_unlink: Removing file from task's pipe list.\n");
            list_head_remove(&file->siblings);

            // Free the associated pipe_inode_info and the vfs_file structure.
            if (file->device) {
                pr_debug("pipe_unlink: Deallocating pipe_inode_info for file.\n");
                __pipe_inode_info_dealloc((pipe_inode_info_t *)file->device);
            }

            // We can free the file now.
            pr_debug("pipe_unlink: Freeing vfs_file structure.\n");
            kfree(file);

            pr_info("pipe_unlink: Successfully unlinked pipe: %s\n", path);
            return 0;
        }
    }

    // Pipe not found, return an error.
    pr_err("pipe_unlink: Pipe unlink failed - no pipe found with path %s.\n", path);
    return -1;
}

/// @brief Closes the specified pipe file, adjusting reader/writer counts and
/// freeing resources if no references remain.
/// @param file Pointer to the `vfs_file_t` structure representing the pipe file.
/// @return 0 on success, -errno on failure.
static int pipe_close(vfs_file_t *file)
{
    // Validate input parameters.
    if (!file) {
        pr_err("pipe_close: Invalid argument - file is NULL.\n");
        return -EINVAL;
    }
    if (!file->device) {
        pr_err("pipe_close: Invalid file - file device is NULL.\n");
        return -EINVAL;
    }

    pipe_inode_info_t *pipe_info = (pipe_inode_info_t *)file->device;

    // Determine if this is a read or write end and decrement the appropriate count.
    // Determine if this is a read or write end and decrement the appropriate count.
    if ((file->flags & O_ACCMODE) == O_WRONLY) {
        if (pipe_info->writers > 0) {
            pipe_info->writers--;
            pr_debug("pipe_close: Decremented writers (count: %u, readers: %u, writers: %u).\n",
                     file->count, pipe_info->readers, pipe_info->writers);
        } else {
            pr_warning("pipe_close: Writers count is already zero.\n");
        }
    } else if ((file->flags & O_ACCMODE) == O_RDONLY) {
        if (pipe_info->readers > 0) {
            pipe_info->readers--;
            pr_debug("pipe_close: Decremented readers (count: %u, readers: %u, writers: %u).\n",
                     file->count, pipe_info->readers, pipe_info->writers);
        } else {
            pr_warning("pipe_close: Readers count is already zero.\n");
        }
    } else {
        pr_warning("pipe_close: Unknown pipe file access mode, possibly incorrect flags.\n");
    }

    // If both readers and writers are zero, free the pipe resources.
    if (--file->count == 0) {
        if ((pipe_info->readers == 0) && (pipe_info->writers == 0)) {
            pr_debug("pipe_close: Fully closing and deallocating pipe.\n");
            // Deallocate the pipe info.
            __pipe_inode_info_dealloc(pipe_info);
        } else {
            pr_debug("pipe_close: Closing and deallocating just the vfs file.\n");
        }

        // Remove the file from the list of opened files.
        list_head_remove(&file->siblings);

        // Free the file from cache.
        kmem_cache_free(file);
    }

    return 0;
}

int pipe_info_has_data(pipe_inode_info_t *pipe_info)
{
    // Check that data is available in at least one buffer.
    for (unsigned int i = 0; i < pipe_info->numbuf; i++) {
        if (pipe_info->bufs[i].len > 0) {
            return 1;
        }
    }
    // No data available in any buffer.
    return 0;
}

int pipe_info_has_space(pipe_inode_info_t *pipe_info)
{
    // Check if At least one buffer has available space.
    for (unsigned int i = 0; i < pipe_info->numbuf; i++) {
        if (pipe_info->bufs[i].len < PIPE_BUFFER_SIZE) {
            return 1;
        }
    }
    // No buffer has space.
    return 0;
}

int pipe_read_wake_function(wait_queue_entry_t *wait, unsigned mode, int sync)
{
    pipe_inode_info_t *pipe_info = wait->private;

    // Check if any data is available in the entire pipe, not just a specific buffer.
    if (pipe_info_has_data(pipe_info)) {
        // Signal that we should wake up the waiting process.
        pr_debug("pipe_read_wake_function: Data available, waking up reader.\n");
        return 1;
    }

    // No data available, so the reader should remain waiting.
    pr_debug("pipe_read_wake_function: No data available, reader should continue waiting.\n");
    return 0;
}

int pipe_write_wake_function(wait_queue_entry_t *wait, unsigned mode, int sync)
{
    pipe_inode_info_t *pipe_info = wait->private;

    // Check if there is available space in the entire pipe
    if (pipe_info_has_space(pipe_info)) {
        // Signal that the writer can proceed.
        pr_debug("pipe_write_wake_function: Space available, waking up writer.\n");
        return 1;
    }

    // No space available, so the writer should remain waiting.
    pr_debug("pipe_write_wake_function: No space available, writer should continue waiting.\n");
    return 0;
}

/// @brief Reads data from the specified pipe file into the provided buffer.
/// @param file Pointer to the `vfs_file_t` structure representing the pipe file.
/// @param buffer Buffer where the data will be stored.
/// @param offset Unused for pipes, but included for interface compatibility.
/// @param nbyte Maximum number of bytes to read.
/// @return Number of bytes read on success, 0 if no data available in non-blocking mode, or -1 on error.
static ssize_t pipe_read(vfs_file_t *file, char *buffer, off_t offset, size_t nbyte)
{
    // Validate input parameters.
    if (!file) {
        pr_err("pipe_read: Invalid argument - file is NULL.\n");
        return -1;
    }
    if (!buffer) {
        pr_err("pipe_read: Invalid argument - buffer is NULL.\n");
        return -1;
    }
    if (!file->device) {
        pr_err("pipe_read: Invalid file - file device is NULL.\n");
        return -1;
    }

    // Retrieve the current task structure.
    task_struct *task = scheduler_get_current_process();
    assert(task && "Failed to retrieve current task.");

    // Retrieve the pipe information structure.
    pipe_inode_info_t *pipe_info = (pipe_inode_info_t *)file->device;

    // Acquire the pipe mutex to ensure safe access.
    mutex_lock(&pipe_info->mutex, task->pid);
    pr_debug("pipe_read: Mutex locked by process %d for reading.\n", task->pid);

    ssize_t bytes_read = 0;
    int err;

    // Loop to read data from the pipe until requested bytes are read.
    while (bytes_read < nbyte) {
        // Calculate the linear index for reading.
        size_t read_pos = pipe_info->read_index;

        // Get the buffer index and offset for the current read position.
        size_t buffer_index = pipe_linear_to_buffer_index(read_pos);
        size_t buf_offset   = pipe_linear_to_offset(read_pos);

        // Access the current buffer.
        pipe_buffer_t *pipe_buffer = pipe_info->bufs + buffer_index;

        // Confirm the buffer is ready to be written to.
        if (pipe_buffer_confirm(pipe_info, buffer_index) < 0) {
            pr_err("pipe_read: Failed to confirm buffer %u readiness for reading.\n", buffer_index);
            break; // No data to read.
        }

        // Confirm that the current buffer has data.
        if (pipe_buffer_empty(pipe_info, buffer_index)) {
            // In non-blocking mode, return 0 immediately if no data was read.
            if (bytes_read == 0) {
                pr_debug("pipe_read: Buffer %u is empty, no data to read.\n", buffer_index);
                break; // No data to read.
            }
            pr_debug("pipe_read: Buffer %u is exhausted, stopping read.\n", buffer_index);
            break; // Exit the loop if data has been read.
        }

        // Calculate the number of bytes available in the current buffer.
        size_t available = pipe_buffer_available(pipe_info, buffer_index);
        size_t to_read   = (nbyte - bytes_read) < available ? (nbyte - bytes_read) : available;

        // Read data from the pipe buffer into the user-provided buffer.
        ssize_t ret = pipe_buffer_read(pipe_info, buffer_index, buffer + bytes_read, to_read);
        if (ret < 0) {
            pr_err("pipe_read: Error reading to pipe buffer.\n");
            bytes_read = -1;

            break;
        } else if (ret == 0) {
            pr_warning("pipe_read: Pipe buffer is full, no data written. Retrying or handling as needed.\n");

            // For a non-blocking pipe, return bytes_written so far.
            if (!is_pipe_nonblocking(file)) {
                wait_queue_entry_t *wait_queue_entry = sleep_on(&pipe_info->read_wait);
                if (wait_queue_entry) {
                    wait_queue_entry->func    = pipe_read_wake_function;
                    wait_queue_entry->private = pipe_info;
                }
                scheduler_run(get_current_interrupt_stack_frame());
            }

            break;
        }

        pr_debug("pipe_read: Read %d bytes from buffer %u.\n", ret, buffer_index);

        // Increment bytes_read by the actual number of bytes read.
        bytes_read += ret;

        // Update the linear read index.
        pipe_info->read_index += ret;

        list_for_each_safe_decl(it, store, &pipe_info->write_wait.task_list)
        {
            wait_queue_entry_t *wait_queue_entry = list_entry(it, wait_queue_entry_t, task_list);

            // Run the wakeup test function for the waiting task.
            if (wait_queue_entry->func(wait_queue_entry, TASK_RUNNING, 0)) {
                // Task is ready, remove from the wait queue.
                remove_wait_queue(&pipe_info->write_wait, wait_queue_entry);

                // Log and free the memory associated with the wait entry.
                pr_debug("pipe_read: Process %d woken up from waiting for read.\n", wait_queue_entry->task->pid);
                wait_queue_entry_dealloc(wait_queue_entry);
            }
        }

        // Check if the current buffer is now empty.
        if (pipe_buffer_empty(pipe_info, buffer_index)) {
            pr_debug("pipe_read: Buffer %u is now empty. Advancing to next buffer.\n", buffer_index);
            // Advance the read index to the next buffer.
            pipe_info->read_index++;
        }
    }

    // Release the mutex.
    mutex_unlock(&pipe_info->mutex);
    pr_debug("pipe_read: Mutex unlocked by process %d.\n", task->pid);

    return bytes_read;
}

/// @brief Writes data to the specified pipe file from the provided buffer.
/// @param file Pointer to the `vfs_file_t` structure representing the pipe file.
/// @param buffer Buffer containing the data to write.
/// @param offset Unused for pipes, but included for interface compatibility.
/// @param nbyte Maximum number of bytes to write.
/// @return Number of bytes written on success, or -1 on error.
static ssize_t pipe_write(vfs_file_t *file, const void *buffer, off_t offset, size_t nbyte)
{
    // Validate input parameters.
    if (!file) {
        pr_err("pipe_write: Invalid argument - file is NULL.\n");
        return -1;
    }
    if (!buffer) {
        pr_err("pipe_write: Invalid argument - buffer is NULL.\n");
        return -1;
    }
    if (!file->device) {
        pr_err("pipe_write: Invalid file - file device is NULL.\n");
        return -1;
    }

    // Retrieve the current task structure.
    task_struct *task = scheduler_get_current_process();
    assert(task && "Failed to retrieve current task.");

    // Retrieve the pipe information structure.
    pipe_inode_info_t *pipe_info = (pipe_inode_info_t *)file->device;

    // Acquire the pipe mutex to ensure safe access.
    mutex_lock(&pipe_info->mutex, task->pid);
    pr_debug("pipe_write: Mutex locked by process %d for writing.\n", task->pid);

    ssize_t bytes_written = 0;
    int err;

    // Loop to write data to the pipe buffer until all requested bytes are written.
    while (bytes_written < nbyte) {
        // Calculate the linear index for writing.
        size_t write_pos = pipe_info->write_index;

        // Get the buffer index and offset for the current write position.
        size_t buffer_index = pipe_linear_to_buffer_index(write_pos);
        size_t buf_offset   = pipe_linear_to_offset(write_pos);

        // Access the current pipe buffer.
        pipe_buffer_t *pipe_buffer = &pipe_info->bufs[buffer_index];
        pr_debug("pipe_write: Accessing buffer %u for writing at offset %u.\n", buffer_index, buf_offset);

        // Confirm the buffer is ready to be written to.
        if (pipe_buffer_confirm(pipe_info, buffer_index) < 0) {
            pr_err("pipe_write: Failed to confirm buffer readiness for writing.\n");
            bytes_written = -1;
            break;
        }

        // Calculate available space in the buffer.
        size_t available = pipe_buffer_capacity(pipe_info, buffer_index) - pipe_buffer->len;
        if (available == 0) {
            pr_debug("pipe_write: Current buffer %u is full. Moving to next buffer.\n", buffer_index);
            // Advance the write index to the next buffer.
            pipe_info->write_index++;
            continue; // Move to the next buffer if current one is full.
        }

        // Determine the amount of data to write in this iteration.
        size_t to_write = (nbyte - bytes_written) < available ? (nbyte - bytes_written) : available;

        // Write data to the pipe buffer at the current offset.
        int ret = pipe_buffer_write(pipe_info, buffer_index, (const char *)buffer + bytes_written, to_write);
        if (ret < 0) {
            pr_err("pipe_write: Error writing to pipe buffer.\n");
            bytes_written = -1;

            break;
        } else if (ret == 0) {
            pr_warning("pipe_write: Pipe buffer is full, no data written. Retrying or handling as needed.\n");

            // For a non-blocking pipe, return bytes_written so far.
            if (!is_pipe_nonblocking(file)) {
                wait_queue_entry_t *wait_queue_entry = sleep_on(&pipe_info->write_wait);
                if (wait_queue_entry) {
                    wait_queue_entry->func    = pipe_write_wake_function;
                    wait_queue_entry->private = pipe_info;
                }
                scheduler_run(get_current_interrupt_stack_frame());
            }

            break;
        }

        pr_debug("pipe_write: Wrote %d bytes to buffer %u.\n", ret, buffer_index);

        // Increment bytes_written by the actual number of bytes written.
        bytes_written += ret;

        // Update the linear write index.
        pipe_info->write_index += ret;

        list_for_each_safe_decl(it, store, &pipe_info->read_wait.task_list)
        {
            wait_queue_entry_t *wait_queue_entry = list_entry(it, wait_queue_entry_t, task_list);

            // Run the wakeup test function for the waiting task.
            if (wait_queue_entry->func(wait_queue_entry, TASK_RUNNING, 0)) {
                // Task is ready, remove from the wait queue.
                remove_wait_queue(&pipe_info->read_wait, wait_queue_entry);

                // Log and free the memory associated with the wait entry.
                pr_debug("pipe_write: Process %d woken up from waiting for read.\n", wait_queue_entry->task->pid);
                wait_queue_entry_dealloc(wait_queue_entry);
            }
        }

        // Advance the write index to the next buffer if necessary.
        if (pipe_buffer->len >= pipe_buffer_capacity(pipe_info, buffer_index)) {
            pr_debug("pipe_write: Buffer %u is now full. Advancing to next buffer.\n", buffer_index);
            pipe_info->write_index++;
        }
    }

    // Release the mutex after writing.
    mutex_unlock(&pipe_info->mutex);
    pr_debug("pipe_write: Mutex unlocked by process %d.\n", task->pid);

    return bytes_written;
}

static off_t pipe_lseek(vfs_file_t *file, off_t offset, int whence)
{
    return -1;
}

static int pipe_fstat(vfs_file_t *file, stat_t *stat)
{
    return -1;
}

/// @brief Creates a file descriptor for one end of a pipe.
/// @param pipe_info Pointer to the pipe inode information structure.
/// @param flags Open mode for the pipe (e.g., O_RDONLY or O_WRONLY).
/// @param mode Permission mode for the new pipe file if creating.
/// @return The created file descriptor on success, or -1 on error.
static inline int create_pipe_fd(pipe_inode_info_t *pipe_info, int flags, mode_t mode)
{
    // Validate the pipe_info parameter.
    if (!pipe_info) {
        pr_err("Invalid pipe_info: pipe_info is NULL.\n");
        return -1;
    }

    // Retrieve the current task structure.
    task_struct *task = scheduler_get_current_process();
    assert(task && "Failed to retrieve current task.");

    // Allocate and initialize a new vfs_file_t structure for the pipe.
    vfs_file_t *file = pipe_create_file_struct(NULL, flags, mode);
    if (!file) {
        pr_err("Failed to allocate memory for vfs_file_t structure.\n");
        return -1;
    }

    // Link the pipe_info to the vfs_file structure.
    file->device = pipe_info;

    // Get an unused file descriptor.
    int fd = get_unused_fd();
    if (fd < 0) {
        pr_err("Failed to allocate file descriptor.\n");
        // Close the file if fd allocation fails.
        pipe_close(file);
        return -1;
    }

    // Register the file descriptor in the process's file descriptor table.
    task->fd_list[fd].file_struct = file;
    task->fd_list[fd].flags_mask  = file->flags;

    // Return the created file descriptor.
    return fd;
}

int vfs_update_pipe_counts(task_struct *task, task_struct *old_task)
{
    // Iterate through the file descriptors in the task
    for (int fd = 0; fd < task->max_fd; fd++) {
        // Get the file.
        vfs_file_t *file = task->fd_list[fd].file_struct;
        // Check if the file descriptor is associated with a pipe.
        if (file && S_ISFIFO(file->flags)) {
            // Assume file_struct has a member pipe_info that points to pipe_inode_info_t.
            pipe_inode_info_t *pipe_info = (pipe_inode_info_t *)file->device;

            // Increase the readers or writers count.
            if (pipe_info) {
                // Increment readers or writers count based on the file descriptor's direction.
                if ((file->flags & O_ACCMODE) == O_WRONLY) {
                    // Increment the writers count for the pipe.
                    ++pipe_info->writers;
                    pr_debug("vfs_update_pipe_counts: Increased writers count for pipe associated with fd %d. New count: %d\n",
                             fd, pipe_info->writers);
                } else if ((file->flags & O_ACCMODE) == O_RDONLY) {
                    // Increment the readers count for the pipe.
                    ++pipe_info->readers;
                    pr_debug("vfs_update_pipe_counts: Increased readers count for pipe associated with fd %d. New count: %d\n",
                             fd, pipe_info->readers);
                } else {
                    pr_warning("vfs_update_pipe_counts: Unknown pipe file access mode, possibly incorrect flags.\n");
                }
            }
        }
    }
    return 0;
}

/// @brief System call to create a new pipe.
/// @param fds Array to store read and write file descriptors.
/// @return 0 on success, or -1 on error.
int sys_pipe(int fds[2])
{
    // Validate input pointer
    if (!fds) {
        pr_err("Invalid argument: fds is NULL.\n");
        return -1;
    }

    // Allocate and initialize the pipe_inode_info structure.
    pipe_inode_info_t *pipe_info = __pipe_inode_info_alloc();
    if (!pipe_info) {
        pr_err("Failed to allocate memory for pipe_inode_info structure.\n");
        return -1;
    }

    // Confirm each buffer in pipe_info is properly initialized.
    for (size_t index = 0; index < pipe_info->numbuf; ++index) {
        int ret = pipe_buffer_confirm(pipe_info, index);
        if (ret < 0) {
            pr_err("Buffer confirmation failed for buffer %u with error code %d: %s.\n", index, -ret, strerror(-ret));
            // Free pipe info structure.
            __pipe_inode_info_dealloc(pipe_info);
            // Free allocated file structure before returning.
            return -1;
        }
    }

    // Create file descriptors for reading and writing
    int fd_read  = create_pipe_fd(pipe_info, O_RDONLY, 0);
    int fd_write = create_pipe_fd(pipe_info, O_WRONLY, 0);
    if (fd_read < 0 || fd_write < 0) {
        pr_err("Failed to create file descriptors.\n");
        if (fd_read >= 0) { sys_close(fd_read); }
        if (fd_write >= 0) { sys_close(fd_write); }
        __pipe_inode_info_dealloc(pipe_info);
        return -1;
    }

    pipe_info->readers = 1;
    pipe_info->writers = 1;

    // Assign file descriptors to output array
    fds[0] = fd_read;
    fds[1] = fd_write;

    return 0; // Success
}
