/// @file pipe.c
/// @brief PIPE functions and structures implementation.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.
/// @details
/// This haiku is to celebrate the functioning pipes:
///     Data flows like streams,
///     Silent pipes breathe in and out,
///     Code sings clear and true.

// ============================================================================
// Setup the logging for this file (do this before any other include).
#include "sys/kernel_levels.h"           // Include kernel log levels.
#define __DEBUG_HEADER__ "[PIPE  ]"      ///< Change header.
#define __DEBUG_LEVEL__  LOGLEVEL_NOTICE ///< Set log level.
#include "io/debug.h"                    // Include debugging functions.
// ============================================================================

#include "fs/pipe.h"

#include "assert.h"
#include "mem/kheap.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "errno.h"
#include "strerror.h"
#include "list_head.h"
#include "fs/vfs.h"
#include "sys/stat.h"
#include "fcntl.h"
#include "time.h"
#include "system/syscall.h"

// ============================================================================
// Virtual FileSystem (VFS) Operaions
// ============================================================================

// static int pipe_buffer_confirm(pipe_inode_info_t *, size_t);
// static int pipe_buffer_empty(pipe_inode_info_t *pipe_info, size_t index);
// static size_t pipe_buffer_available(pipe_inode_info_t *pipe_info, size_t index);
// static size_t pipe_buffer_capacity(pipe_inode_info_t *pipe_info, size_t index);
// static ssize_t pipe_buffer_read(pipe_inode_info_t *pipe_info, size_t index, char *dest, size_t count);
// static int pipe_buffer_write(pipe_inode_info_t *pipe_info, size_t index, const char *src, size_t count);

static int pipe_stat(const char *path, stat_t *stat);

static vfs_file_t *pipe_open(const char *path, int flags, mode_t mode);
static int pipe_unlink(const char *path);
static int pipe_close(vfs_file_t *file);
static ssize_t pipe_read(vfs_file_t *file, char *buffer, off_t offset, size_t nbyte);
static ssize_t pipe_write(vfs_file_t *file, const void *buffer, off_t offset, size_t nbyte);
static off_t pipe_lseek(vfs_file_t *file, off_t offset, int whence);
static int pipe_fstat(vfs_file_t *file, stat_t *stat);
static long pipe_fcntl(vfs_file_t *file, unsigned int request, unsigned long data);

/// @brief Operations for managing pipe buffers in the kernel.
static struct pipe_buf_operations anonymous_pipe_ops = {
    .confirm   = NULL, // pipe_buffer_confirm,
    .empty     = NULL, // pipe_buffer_empty,
    .available = NULL, // pipe_buffer_available,
    .capacity  = NULL, // pipe_buffer_capacity,
    .read      = NULL, // pipe_buffer_read,
    .write     = NULL, // pipe_buffer_write,
};

/// @brief Operations for managing pipe buffers in the kernel.
static struct pipe_buf_operations named_pipe_ops = {
    .confirm   = NULL, //pipe_buffer_confirm,
    .empty     = NULL, //pipe_buffer_empty,
    .available = NULL, //pipe_buffer_available,
    .capacity  = NULL, //pipe_buffer_capacity,
    .read      = NULL, //pipe_buffer_read,
    .write     = NULL, //pipe_buffer_write,
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
    .fcntl_f    = pipe_fcntl,
    .getdents_f = NULL,
    .readlink_f = NULL,
};

// static list_head named_pipes;

// ============================================================================
// MEMORY MANAGEMENT (Private)
// ============================================================================

/// @brief Initializes a pipe buffer.
/// @param pipe_buffer Pointer to the `pipe_buffer_t` structure to initialize.
/// @param ops Pointer to the `pipe_buf_operations` structure defining buffer operations.
/// @return 0 on success, -ENOMEM if page allocation fails.
static inline int __pipe_buffer_init(pipe_buffer_t *pipe_buffer, struct pipe_buf_operations *ops)
{
    // Check if we received a valid pipe buffer.
    assert(pipe_buffer && "Received a null pipe buffer.");

    // Initialize the pipe buffer to zero.
    memset(pipe_buffer, 0, sizeof(pipe_buffer_t));

    // Initialize additional fields in the pipe_buffer_t structure.
    pipe_buffer->len    = 0;
    pipe_buffer->offset = 0;
    pipe_buffer->ops    = ops;

    return 0;
}

/// @brief De-initializes a pipe buffer.
/// @param pipe_buffer Pointer to the `pipe_buffer_t` structure to deinitialize.
/// This should not be NULL.
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
/// @param ops Pointer to the `pipe_buf_operations` structure for buffer operations.
/// @return Pointer to the allocated and initialized `pipe_inode_info_t`
/// structure, or NULL if allocation fails.
static inline pipe_inode_info_t *__pipe_inode_info_alloc(struct pipe_buf_operations *ops)
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
        if (__pipe_buffer_init(&pipe_info->bufs[i], ops)) {
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
// PIPE INFO AND BUFFER OPERATIONS (Private)
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

/// @brief Checks if the specified pipe has any data available in its buffers.
/// @param pipe_info Pointer to the pipe information structure.
/// @return 1 if data is available, 0 if all buffers are empty, -EINVAL on error.
static inline int pipe_info_has_data(pipe_inode_info_t *pipe_info)
{
    // Validate input parameter.
    if (!pipe_info) {
        pr_err("pipe_info is NULL.\n");
        return -EINVAL;
    }

    // Check if data is available in at least one buffer.
    for (unsigned int i = 0; i < pipe_info->numbuf; i++) {
        if (pipe_info->bufs[i].len > 0) {
            return 1;
        }
    }

    // No data available in any buffer.
    return 0;
}

/// @brief Checks if the specified pipe has available space in any of its buffers.
/// @param pipe_info Pointer to the pipe information structure.
/// @return 1 if space is available, 0 if all buffers are full, -EINVAL on error.
static inline int pipe_info_has_space(pipe_inode_info_t *pipe_info)
{
    // Validate input parameter.
    if (!pipe_info) {
        pr_err("pipe_info is NULL.\n");
        return -EINVAL;
    }

    // Check if at least one buffer has available space.
    for (unsigned int i = 0; i < pipe_info->numbuf; i++) {
        if (pipe_info->bufs[i].len < PIPE_BUFFER_SIZE) {
            return 1;
        }
    }

    // No space available in any buffer.
    return 0;
}

/// @brief Checks if the specified pipe buffer is empty.
/// @param pipe_buffer Pointer to the pipe buffer structure to check.
/// @return 1 if the buffer is empty (length is 0), or 0 if not or if pipe_buffer is NULL.
static int pipe_buffer_empty(pipe_buffer_t *pipe_buffer)
{
    // Validate input parameter.
    if (!pipe_buffer) {
        pr_err("pipe_buffer is NULL.\n");
        return 0; // Return 0 as there's no buffer to check.
    }

    // Buffer is empty if len is 0.
    return pipe_buffer->len == 0;
}

/// @brief Retrieves the number of bytes available (unread) in the specified pipe buffer.
/// @param pipe_buffer Pointer to the pipe buffer structure to check.
/// @return The number of bytes available in the buffer, or 0 if pipe_buffer is NULL.
static size_t pipe_buffer_available(pipe_buffer_t *pipe_buffer)
{
    // Validate input parameter.
    if (!pipe_buffer) {
        pr_err("pipe_buffer is NULL.\n");
        return 0; // Return 0 as there’s no buffer to check.
    }

    // Return the current length of the buffer, representing unread data.
    return pipe_buffer->len;
}

/// @brief Determines the remaining capacity for writing in the specified pipe buffer.
/// @param pipe_buffer Pointer to the pipe buffer structure to check.
/// @return The remaining write capacity in bytes, or 0 if pipe_buffer is NULL.
static inline size_t pipe_buffer_capacity(pipe_buffer_t *pipe_buffer)
{
    // Validate input parameter.
    if (!pipe_buffer) {
        pr_err("pipe_buffer is NULL.\n");
        return 0; // Return 0 as there's no buffer to check.
    }

    // Calculate available capacity by subtracting the offset + length from the total buffer size.
    return PIPE_BUFFER_SIZE - (pipe_buffer->offset + pipe_buffer->len);
}

/// @brief Determines the number of bytes that can be read from the pipe buffer.
/// @param pipe_buffer Pointer to the pipe buffer structure.
/// @param count The requested number of bytes to read.
/// @return The number of bytes that can be read on success, or a negative errno-style error code on failure.
static ssize_t pipe_calculate_bytes_to_read(pipe_buffer_t *pipe_buffer, size_t count)
{
    // Validate input parameters.
    if (!pipe_buffer) {
        pr_err("pipe_buffer is NULL.\n");
        return -EINVAL;
    }
    if (count == 0) {
        pr_err("Invalid read request of 0 bytes (must be positive).\n");
        return -EINVAL;
    }

    // Check if there is data available in the buffer.
    if (pipe_buffer_empty(pipe_buffer)) {
        pr_debug("No data available in buffer.\n");
        return -EAGAIN;
    }

    // Return the number of bytes to read based on the requested count and available data.
    return (count < pipe_buffer->len) ? count : pipe_buffer->len;
}

/// @brief Determines the number of bytes that can be safely written to the pipe buffer.
/// @param pipe_buffer Pointer to the specific pipe buffer.
/// @param count Number of bytes the caller wants to write.
/// @return The number of bytes that can be written on success, or a negative error code on failure.
static ssize_t pipe_calculate_bytes_to_write(pipe_buffer_t *pipe_buffer, size_t count)
{
    // Validate input parameters.
    if (!pipe_buffer) {
        pr_err("pipe_buffer is NULL.\n");
        return -EINVAL;
    }
    if (count == 0) {
        pr_err("Invalid write request of %u bytes (must be positive).\n", count);
        return -EINVAL;
    }

    // Calculate available space in the buffer from the current write position.
    size_t capacity = pipe_buffer_capacity(pipe_buffer);
    if (capacity == 0) {
        pr_debug("No space available in buffer for writing.\n");
        return -EAGAIN;
    }

    // Return the smaller of the requested write bytes or available space.
    return (count < capacity) ? count : capacity;
}

/// @brief Ensures that the pipe buffer is valid and ready to use.
/// @param pipe_buffer Pointer to the `pipe_buffer_t` structure to confirm.
/// @return 0 if the buffer is valid, or a non-zero error code if it is not.
static int pipe_buffer_confirm(pipe_buffer_t *pipe_buffer)
{
    // Validate input parameters.
    if (!pipe_buffer) {
        pr_err("pipe_buffer is NULL.\n");
        return -EINVAL;
    }

    // Ensure length and offset are within valid bounds.
    if ((pipe_buffer->len + pipe_buffer->offset) > PIPE_BUFFER_SIZE) {
        pr_err("Buffer length and offset exceed bounds: len = %u, offset = %u, PIPE_BUFFER_SIZE = %lu.\n",
               pipe_buffer->len, pipe_buffer->offset, PIPE_BUFFER_SIZE);
        return -EOVERFLOW;
    }

    // Ensure operations pointer is valid.
    if (!pipe_buffer->ops) {
        pr_err("Buffer operations pointer is NULL.\n");
        return -ENXIO;
    }

    return 0;
}

/// @brief Reads data from the specified pipe buffer into the provided buffer.
/// @param pipe_buffer Pointer to the `pipe_buffer_t` structure to read from.
/// @param dest Pointer to the destination buffer where data will be copied.
/// @param count Number of bytes to read from the pipe buffer.
/// @return Number of bytes read on success, or a negative error code on failure.
static ssize_t pipe_buffer_read(pipe_buffer_t *pipe_buffer, char *dest, size_t count)
{
    // Validate input parameters.
    if (!pipe_buffer) {
        pr_err("pipe_buffer is NULL.\n");
        return -EINVAL;
    }
    if (!dest) {
        pr_err("Destination buffer is NULL.\n");
        return -EINVAL;
    }

    ssize_t bytes_to_read = pipe_calculate_bytes_to_read(pipe_buffer, count);
    if (bytes_to_read < 0) {
        pr_debug("Failed to calculate bytes to read (error[%2d]: %s).\n", -bytes_to_read, strerror(-bytes_to_read));
        return bytes_to_read;
    }

    // Copy data from the pipe buffer's data at the specified offset.
    memcpy(dest, pipe_buffer->data + pipe_buffer->offset, bytes_to_read);

    // Adjust buffer's offset and length to reflect the data consumption.
    pipe_buffer->offset += bytes_to_read;
    pipe_buffer->len -= bytes_to_read;

    // If all data has been read, reset offset to 0 for reusability.
    if (pipe_buffer->len == 0) {
        pipe_buffer->offset = 0;
    }

    pr_debug("Read %3ld bytes from buffer (offset: %3u, length: %3u).\n",
             bytes_to_read, pipe_buffer->offset, pipe_buffer->len);

    return bytes_to_read;
}

/// @brief Writes data to the specified pipe buffer.
/// @param pipe_buffer Pointer to the pipe buffer structure where data will be written.
/// @param src Pointer to the source buffer containing data to write.
/// @param count The maximum number of bytes to write.
/// @return the number of written bytes on success, or a negative errno-style error code on failure.
static ssize_t pipe_buffer_write(pipe_buffer_t *pipe_buffer, const char *src, size_t count)
{
    // Validate input parameters.
    if (!pipe_buffer) {
        pr_err("pipe_buffer_write: pipe_buffer is NULL.\n");
        return -EINVAL;
    }
    if (!src) {
        pr_err("pipe_buffer_write: Source buffer is NULL.\n");
        return -EINVAL;
    }

    // Determine the actual number of bytes we can write into the buffer.
    ssize_t bytes_to_write = pipe_calculate_bytes_to_write(pipe_buffer, count);
    if (bytes_to_write < 0) {
        pr_debug("pipe_buffer_write: Failed to calculate bytes to write (error[%2d]: %s).\n", -bytes_to_write, strerror(-bytes_to_write));
        return bytes_to_write;
    }

    // Write data to the buffer's current write position (offset + length).
    memcpy(pipe_buffer->data + pipe_buffer->offset + pipe_buffer->len, src, bytes_to_write);

    // Update the buffer's length to reflect the newly added data.
    pipe_buffer->len += bytes_to_write;

    pr_debug("pipe_buffer_write: Wrote %3ld bytes to buffer (offset: %3u, length: %3u).\n",
             bytes_to_write, pipe_buffer->offset, pipe_buffer->len);

    return bytes_to_write;
}

// ============================================================================
// Wait Queue Functions
// ============================================================================

/// @brief Wake-up function for processes waiting to read from a pipe.
/// @param wait Pointer to the wait queue entry representing the sleeping process.
/// @param mode The mode to set the task to upon wake-up.
/// @param sync Synchronization flag (not used in this implementation).
/// @return 1 if the process is woken up, 0 if it remains in the wait queue.
int pipe_read_wake_function(wait_queue_entry_t *wait, unsigned mode, int sync)
{
    // Retrieve private data associated with this wait queue entry.
    pipe_inode_info_t *pipe_info = (pipe_inode_info_t *)wait->private;

    // Check the private data.
    if (pipe_info == NULL) {
        pr_err("The private data is not a pipe_inode_info_t.\n");
        return -1;
    }

    // Validate that data is available in the pipe for reading.
    if ((pipe_info_has_data(pipe_info) > 0) || (pipe_info->writers == 0)) {
        // Check if the task is in an appropriate sleep state to be woken up.
        if ((wait->task->state == TASK_UNINTERRUPTIBLE) || (wait->task->state == TASK_STOPPED)) {
            // Set the task's state to the specified wake-up mode.
            wait->task->state = mode;

            // Signal that the task has been woken up.
            pr_debug("Data available or no more writers, waking up reader %d.\n", wait->task->pid);
            return 1;
        } else {
            pr_debug("Reader %d not in the correct state for wake-up.\n", wait->task->pid);
        }
    } else {
        pr_debug("No data available, reader %d should continue waiting.\n", wait->task->pid);
    }

    // No wake-up action taken, continue waiting.
    return 0;
}

/// @brief Wake-up function for processes waiting to write to a pipe.
/// @param wait Pointer to the wait queue entry representing the sleeping process.
/// @param mode The mode to set the task to upon wake-up.
/// @param sync Synchronization flag (not used in this implementation).
/// @return 1 if the process is woken up, 0 if it remains in the wait queue.
int pipe_write_wake_function(wait_queue_entry_t *wait, unsigned mode, int sync)
{
    // Retrieve private data associated with this wait queue entry.
    pipe_inode_info_t *pipe_info = (pipe_inode_info_t *)wait->private;

    // Check the private data.
    if (pipe_info == NULL) {
        pr_err("The private data is not a pipe_inode_info_t.\n");
        return -1;
    }

    // Check if there is available space in the pipe for writing.
    if (pipe_info_has_space(pipe_info) > 0) {
        // Only tasks in the state TASK_UNINTERRUPTIBLE or TASK_STOPPED can be woken up.
        if ((wait->task->state == TASK_UNINTERRUPTIBLE) || (wait->task->state == TASK_STOPPED)) {
            // Set the wake-up mode for the task.
            wait->task->state = mode;

            // Signal that the task has been woken up.
            pr_debug("Space available, waking up writer %d.\n", wait->task->pid);
            return 1;
        } else {
            pr_debug("Writer %d not in the correct state for wake-up.\n", wait->task->pid);
        }
    } else {
        pr_debug("No space available, writer %d should continue waiting.\n", wait->task->pid);
    }

    // No wake-up action taken, continue waiting.
    return 0;
}

/// @brief Wakes up tasks in the specified wait queue if their wake-up condition is met.
/// @param wait_queue Pointer to the wait queue from which tasks should be woken up.
/// @param debug_msg Debug message describing the wake-up context.
static void pipe_wake_up_tasks(wait_queue_head_t *wait_queue, const char *debug_msg)
{
    // Validate input parameters.
    if (!wait_queue) {
        pr_err("pipe_wake_up_tasks: wait_queue is NULL.\n");
        return;
    }

    list_for_each_safe_decl(it, store, &wait_queue->task_list)
    {
        wait_queue_entry_t *wait_queue_entry = list_entry(it, wait_queue_entry_t, task_list);

        // Run the wakeup test function for the waiting task.
        if (wait_queue_entry->func(wait_queue_entry, TASK_RUNNING, 0)) {
            // Task is ready, remove from the wait queue.
            remove_wait_queue(wait_queue, wait_queue_entry);

            // Log and free the memory associated with the wait entry.
            pr_debug("%s: Process %d woken up.\n", debug_msg, wait_queue_entry->task->pid);
            wait_queue_entry_dealloc(wait_queue_entry);
        }
    }
}

/// @brief Puts the current process to sleep on the specified wait queue if blocking is needed.
/// @param pipe_info Pointer to the pipe information structure.
/// @param wait_queue Pointer to the wait queue on which to put the process to sleep.
/// @param wake_function Wake-up function associated with the wait queue entry.
/// @param debug_msg Debug message describing the block context.
/// @return 0 after scheduling the blocking behavior.
static int pipe_put_process_to_sleep(
    pipe_inode_info_t *pipe_info,
    wait_queue_head_t *wait_queue,
    int (*wake_function)(wait_queue_entry_t *, unsigned, int),
    const char *debug_msg)
{
    // Blocking behavior: Put the process to sleep until the condition is met.
    wait_queue_entry_t *wait_queue_entry = sleep_on(wait_queue);
    assert(wait_queue_entry && "Failed to allocate wait_queue_entry_t.");

    // Set the wake-up function and private data for the wait entry.
    wait_queue_entry->func    = wake_function;
    wait_queue_entry->private = pipe_info;

    // Indicate blocking behavior was scheduled.
    return 0;
}

// ============================================================================
// Virtual FileSystem (VFS) Functions
// ============================================================================

/// @brief Checks if the pipe is in blocking mode.
/// @param file Pointer to the vfs_file_t structure representing the pipe file.
/// @return 1 if the pipe is in blocking mode, 0 if it is in non-blocking mode.
static inline int pipe_is_blocking(vfs_file_t *file)
{
    // Ensure the file pointer is valid before dereferencing.
    if (!file) {
        pr_err("pipe_check_blocking: file pointer is NULL.\n");
        return 0; // Assume non-blocking mode if file is NULL.
    }

    // Check if the O_NONBLOCK flag is NOT set in the file's flags.
    // If NOT set, the pipe operates in blocking mode and the function returns 1.
    // If set, the pipe is in non-blocking mode, and the function returns 0.
    return (file->flags & O_NONBLOCK) == 0;
}

/// @brief Creates a VFS file structure for a pipe.
/// @param path Path to the file (not used here but may be needed elsewhere).
/// @param flags Open flags (e.g., O_RDONLY, O_WRONLY).
/// @param mode File permissions (e.g., 0666 for read/write for all).
/// @return Pointer to the newly created VFS file, or NULL on failure.
static inline vfs_file_t *pipe_create_file_struct(const char *path, int flags, mode_t mode)
{
    // Allocate memory for the VFS file structure.
    vfs_file_t *vfs_file = vfs_alloc_file();
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

/// @brief Opens a pipe file with the specified path, flags, and mode.
/// @param path The path of the pipe to open.
/// @param flags File status flags and access modes.
/// @param mode File mode bits to apply when creating a new pipe.
/// @return Pointer to the opened vfs_file_t structure, or NULL on failure.
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
    pipe_inode_info_t *pipe_info = __pipe_inode_info_alloc(&named_pipe_ops);
    if (!pipe_info) {
        pr_err("Failed to allocate memory for pipe_inode_info structure.\n");
        // Free allocated new_file structure before returning.
        kfree(new_file);
        return NULL;
    }

    // Link the pipe_info to the vfs_file structure.
    new_file->device = pipe_info;

    // Confirm each buffer in pipe_info is properly initialized.
    pipe_buffer_t *pipe_buffer;
    for (size_t buffer_index = 0; buffer_index < pipe_info->numbuf; ++buffer_index) {
        pipe_buffer = &pipe_info->bufs[buffer_index];

        int ret = pipe_buffer_confirm(pipe_buffer);
        if (ret < 0) {
            pr_err("Buffer confirmation failed for buffer %u (error[%2d]: %s).\n", buffer_index, -ret, strerror(-ret));
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
        pr_err("Invalid path - path is NULL.\n");
        return -1;
    }

    pr_debug("Attempting to unlink pipe with path: %s\n", path);

    // Retrieve the current task structure.
    task_struct *task = scheduler_get_current_process();
    assert(task && "Failed to retrieve current task.");

    // Iterate through the file descriptors in the task
    for (int fd = 0; fd < task->max_fd; fd++) {
        // Get the file.
        vfs_file_t *file = task->fd_list[fd].file_struct;

        // Check if the file name matches the specified path.
        if (file && strcmp(file->name, path) == 0) {
            pr_debug("Pipe file found for path %s at FD %d.\n", path, fd);

            // Remove from the task's pipe list.
            pr_debug("Removing file from task's pipe list.\n");
            list_head_remove(&file->siblings);

            // Free the associated pipe_inode_info and the vfs_file structure.
            if (file->device) {
                pr_debug("Deallocating pipe_inode_info for file.\n");
                __pipe_inode_info_dealloc((pipe_inode_info_t *)file->device);
            }

            // We can free the file now.
            pr_debug("Freeing vfs_file structure.\n");
            kfree(file);

            pr_info("Successfully unlinked pipe: %s\n", path);
            return 0;
        }
    }

    // Pipe not found, return an error.
    pr_err("Pipe unlink failed - no pipe found with path %s.\n", path);
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
        pr_err("Invalid argument - file is NULL.\n");
        return -EINVAL;
    }
    if (!file->device) {
        pr_err("Invalid file - file device is NULL.\n");
        return -EINVAL;
    }

    pipe_inode_info_t *pipe_info = (pipe_inode_info_t *)file->device;

    // Determine if this is a read or write end and decrement the appropriate count.
    if ((file->flags & O_ACCMODE) == O_WRONLY) {
        if (pipe_info->writers > 0) {
            pipe_info->writers--;
            pr_debug("Decremented writers (count: %u, readers: %u, writers: %u).\n",
                     file->count, pipe_info->readers, pipe_info->writers);
        } else {
            pr_warning("Writers count is already zero.\n");
        }
    } else if ((file->flags & O_ACCMODE) == O_RDONLY) {
        if (pipe_info->readers > 0) {
            pipe_info->readers--;
            pr_debug("Decremented readers (count: %u, readers: %u, writers: %u).\n",
                     file->count, pipe_info->readers, pipe_info->writers);
        } else {
            pr_warning("Readers count is already zero.\n");
        }
    } else {
        pr_warning("Unknown pipe file access mode, possibly incorrect flags.\n");
    }

    // If all writers have closed, wake up waiting readers.
    if (pipe_info->writers == 0) {
        pr_debug("All writers have closed the pipe. Waking up readers.\n");
        pipe_wake_up_tasks(&pipe_info->read_wait, "pipe_close");
    }

    // If both readers and writers are zero, free the pipe resources.
    if (--file->count == 0) {
        if ((pipe_info->readers == 0) && (pipe_info->writers == 0)) {
            pr_debug("Fully closing and deallocating pipe.\n");
            // Deallocate the pipe info.
            __pipe_inode_info_dealloc(pipe_info);
        } else {
            pr_debug("Closing and deallocating just the vfs file.\n");
        }

        // Remove the file from the list of opened files.
        list_head_remove(&file->siblings);

        // Free the file from cache.
        vfs_dealloc_file(file);
    }

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
        pr_err("Invalid argument - file is NULL.\n");
        return -1;
    }
    if (!buffer) {
        pr_err("Invalid argument - buffer is NULL.\n");
        return -1;
    }
    if (!file->device) {
        pr_err("Invalid file - file device is NULL.\n");
        return -1;
    }

    // Retrieve the current task structure.
    task_struct *task = scheduler_get_current_process();
    assert(task && "Failed to retrieve current task.");

    // Retrieve the pipe information structure.
    pipe_inode_info_t *pipe_info = (pipe_inode_info_t *)file->device;

    // Acquire the pipe mutex to ensure safe access.
    mutex_lock(&pipe_info->mutex, task->pid);

    // Return 0 if there are no writers left.
    if (pipe_info->writers == 0) {
        pr_debug("No writers left.\n");
        return 0;
    }

    ssize_t bytes_read = 0;

    if (pipe_info_has_data(pipe_info)) {
        // Loop to read data from the pipe until requested bytes are read or an error occurs.
        while (bytes_read < nbyte) {
            // Wrap read_index around when exceeding max buffer capacity.
            pipe_info->read_index %= (pipe_info->numbuf * PIPE_BUFFER_SIZE);

            // Calculate the buffer index for the current read position.
            size_t buffer_index        = pipe_linear_to_buffer_index(pipe_info->read_index);
            pipe_buffer_t *pipe_buffer = &pipe_info->bufs[buffer_index];

            // Confirm that the buffer is ready to be read.
            if (pipe_buffer_confirm(pipe_buffer) < 0) {
                pr_err("Failed to confirm readiness of buffer %u for reading.\n", buffer_index);
                break; // Stop if there’s no data to read.
            }

            // Calculate bytes to read in this iteration, considering the remaining requested bytes.
            ssize_t bytes_to_read = pipe_buffer_read(pipe_buffer, buffer + bytes_read, nbyte - bytes_read);
            if (bytes_to_read < 0) {
                pr_err("Error reading from pipe buffer (error[%2d]: %s).\n", -bytes_to_read, strerror(-bytes_to_read));
                bytes_read = -bytes_to_read;
                break;
            }

            // Update the total bytes read and the read index.
            bytes_read            = bytes_read + bytes_to_read;
            pipe_info->read_index = pipe_info->read_index + bytes_to_read;
        }
    } else {
        // If in blocking mode, put the process to sleep until data is available.
        if (pipe_is_blocking(file)) {
            pipe_put_process_to_sleep(pipe_info, &pipe_info->read_wait, pipe_read_wake_function, "pipe_read");
        }
        // TODO: We currently do not save kernel regs status, so we need a
        // work-around when putting processes to sleep.
        bytes_read = -EAGAIN;
    }

    // Release the mutex after reading.
    mutex_unlock(&pipe_info->mutex);

    // Wake up tasks that might be waiting to write to the pipe.
    if (bytes_read > 0) {
        pipe_wake_up_tasks(&pipe_info->write_wait, "pipe_read");
    }

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
        pr_err("Invalid argument - file is NULL.\n");
        return -1;
    }
    if (!buffer) {
        pr_err("Invalid argument - buffer is NULL.\n");
        return -1;
    }
    if (!file->device) {
        pr_err("Invalid file - file device is NULL.\n");
        return -1;
    }

    // Retrieve the current task structure.
    task_struct *task = scheduler_get_current_process();
    assert(task && "Failed to retrieve current task.");

    // Retrieve the pipe information structure.
    pipe_inode_info_t *pipe_info = (pipe_inode_info_t *)file->device;

    // Acquire the pipe mutex to ensure safe access.
    mutex_lock(&pipe_info->mutex, task->pid);

    ssize_t bytes_written = 0;

    // Check if there is available space in the pipe for writing.
    if (pipe_info_has_space(pipe_info)) {
        // Loop to write data to the pipe buffer until the requested number of bytes is written.
        while (bytes_written < nbyte) {
            // Wrap around write_index when it exceeds the max buffer capacity.
            pipe_info->write_index %= (pipe_info->numbuf * PIPE_BUFFER_SIZE);

            // Get the buffer index for the current write position.
            size_t buffer_index        = pipe_linear_to_buffer_index(pipe_info->write_index);
            pipe_buffer_t *pipe_buffer = &pipe_info->bufs[buffer_index];

            // Confirm the buffer is ready for writing.
            if (pipe_buffer_confirm(pipe_buffer) < 0) {
                pr_err("Failed to confirm readiness of buffer %u for writing.\n", buffer_index);
                bytes_written = -1;
                break;
            }

            // Attempt to write data into the pipe buffer.
            ssize_t bytes_to_write = pipe_buffer_write(pipe_buffer, (const char *)buffer + bytes_written, nbyte - bytes_written);
            if (bytes_to_write < 0) {
                // Other errors: Log and return immediately.
                pr_err("Error writing to pipe buffer (error[%2d]: %s).\n", -bytes_to_write, strerror(-bytes_to_write));
                bytes_written = -1;
                break;
            }

            // Update the total bytes written and the write index.
            bytes_written          = bytes_written + bytes_to_write;
            pipe_info->write_index = pipe_info->write_index + bytes_to_write;
        }
    } else {
        // Blocking behavior: Put the process to sleep until space is available.
        if (pipe_is_blocking(file)) {
            pipe_put_process_to_sleep(pipe_info, &pipe_info->write_wait, pipe_write_wake_function, "pipe_write");
        }
        // TODO: We currently do not save kernel regs status, so we need a
        // work-around when putting processes to sleep.
        bytes_written = -EAGAIN;
    }

    // Release the mutex after the write operation is complete.
    mutex_unlock(&pipe_info->mutex);

    // Wake up tasks waiting to read from the pipe.
    if (bytes_written > 0) {
        pipe_wake_up_tasks(&pipe_info->read_wait, "pipe_write");
    }

    return bytes_written;
}

/// @brief Performs a seek operation on a pipe, which is not supported.
/// @param file Pointer to the `vfs_file_t` structure representing the pipe.
/// @param offset The seek offset (unused for pipes).
/// @param whence The seek base (e.g., SEEK_SET, SEEK_CUR, SEEK_END; unused for pipes).
/// @return Always returns -1 as pipes do not support seeking.
static off_t pipe_lseek(vfs_file_t *file, off_t offset, int whence)
{
    return -1;
}

/// @brief Retrieves file status information for a pipe, which is not supported.
/// @param file Pointer to the `vfs_file_t` structure representing the pipe.
/// @param stat Pointer to the `stat_t` structure to hold file status information.
/// @return Always returns -1 as pipes do not support file status retrieval.
static int pipe_fstat(vfs_file_t *file, stat_t *stat)
{
    return -1;
}

/// @brief Performs a fcntl operation on a pipe file descriptor
/// @param file Pointer to the vfs_file_t structure representing the pipe file.
/// @param request The fcntl command (e.g., F_GETFL, F_SETFL)
/// @param data Additional argument for setting flags (used with F_SETFL)
/// @return On success, returns 0 for F_SETFL or current flags for F_GETFL; -1 on error with errno set.
static long pipe_fcntl(vfs_file_t *file, unsigned int request, unsigned long data)
{
    if (!file) {
        errno = EBADF;
        pr_err("Invalid file descriptor.\n");
        return -1;
    }

    switch (request) {
    case F_GETFL:
        pr_debug("Retrieving flags for pipe.\n");
        return file->flags; // Return the current flags for the file descriptor

    case F_SETFL:
        pr_debug("Setting flags for pipe.\n");
        // Only handle O_NONBLOCK for simplicity
        if (data & O_NONBLOCK) {
            file->flags |= O_NONBLOCK;
            pr_debug("Set O_NONBLOCK flag.\n");
        } else {
            file->flags &= ~O_NONBLOCK;
            pr_debug("Cleared O_NONBLOCK flag.\n");
        }
        return 0;

    default:
        errno = EINVAL;
        pr_err("Unsupported request %u.\n", request);
        return -1;
    }
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
        pr_err("pipe_info is NULL.\n");
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
                    pr_debug("Increased writers count for pipe associated with fd %d. New count: %d\n",
                             fd, pipe_info->writers);
                } else if ((file->flags & O_ACCMODE) == O_RDONLY) {
                    // Increment the readers count for the pipe.
                    ++pipe_info->readers;
                    pr_debug("Increased readers count for pipe associated with fd %d. New count: %d\n",
                             fd, pipe_info->readers);
                } else {
                    pr_warning("Unknown pipe file access mode, possibly incorrect flags.\n");
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
    pipe_inode_info_t *pipe_info = __pipe_inode_info_alloc(&anonymous_pipe_ops);
    if (!pipe_info) {
        pr_err("Failed to allocate memory for pipe_inode_info structure.\n");
        return -1;
    }

    // Confirm each buffer in pipe_info is properly initialized.
    pipe_buffer_t *pipe_buffer;
    for (size_t buffer_index = 0; buffer_index < pipe_info->numbuf; ++buffer_index) {
        pipe_buffer = &pipe_info->bufs[buffer_index];

        int ret = pipe_buffer_confirm(pipe_buffer);
        if (ret < 0) {
            pr_err("Buffer confirmation failed for buffer %u (error[%2d]: %s).\n", buffer_index, -ret, strerror(-ret));
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
