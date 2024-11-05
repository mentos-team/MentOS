/// @file pipe.h
/// @brief PIPE functions and structures declaration.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "process/process.h"
#include "process/wait.h"
#include "klib/mutex.h"

/// @brief This constant specifies the size of the buffer allocated for each
/// pipe, and its value can affect the performance and capacity of pipes.
// #define PIPE_BUFFER_SIZE PAGE_SIZE
#define PIPE_BUFFER_SIZE 64

/// @brief The number of buffers.
#define PIPE_NUM_BUFFERS 5

/// @brief Represents a single buffer within a pipe. This structure manages the
/// data stored in the buffer, including its memory location, size, usage count,
/// and associated operations.
typedef struct pipe_buffer {
    /// @brief The buffer's data.
    char data[PIPE_BUFFER_SIZE];

    /// @brief Offset within the memory page where the buffer's data begins.
    /// This allows for partial usage of the page if the buffer does not occupy
    /// the entire page.
    unsigned int offset;

    /// @brief Length of the data currently stored in the buffer. This indicates
    /// the amount of data that the buffer holds and can be used during read and
    /// write operations.
    unsigned int len;

    /// @brief Pointer to a set of operations that can be performed on the
    /// buffer. These operations include functions for getting, releasing, and
    /// mapping the buffer, tailored to the specific needs of the buffer's data
    /// type.
    const struct pipe_buf_operations *ops;

} pipe_buffer_t;

/// @brief This structure represents a pipe in the kernel. It contains
/// information about the buffer used for the pipe, the readers and writers, and
/// synchronization details.
typedef struct pipe_inode_info {
    /// @brief Array of pipe buffers. Each buffer holds a portion of data for
    /// the pipe. For the time beeing I'm setting a fixed number of buffers.
    pipe_buffer_t bufs[PIPE_NUM_BUFFERS];

    /// @brief Number of buffers allocated for the pipe. This value determines
    /// the size of the `bufs` array and how many buffers are available for use.
    unsigned int numbuf;

    /// @brief Index for reading.
    size_t read_index;

    /// @brief Index for writing.
    size_t write_index;

    /// @brief The number of processes currently reading from the pipe.
    unsigned int readers;

    /// @brief The number of processes currently writing to the pipe.
    unsigned int writers;

    /// @brief Wait queue for processes that are blocked waiting to read from
    /// the pipe. This queue helps manage process scheduling and
    /// synchronization.
    wait_queue_head_t read_wait;

    /// @brief Wait queue for processes that are blocked waiting to write from
    /// the pipe. This queue helps manage process scheduling and
    /// synchronization.
    wait_queue_head_t write_wait;

    /// @brief Mutex for protecting access to the pipe structure and ensuring
    /// thread-safe operations. This prevents race conditions when multiple
    /// processes or threads interact with the pipe.
    mutex_t mutex;

    /// @brief List node for tracking this pipe in a process’s list of opened
    /// pipes.
    list_head list_node;
} pipe_inode_info_t;

/// @brief Structure defining operations for managing pipe buffers.
/// @details
/// This structure contains function pointers to operations that can be
/// performed on pipe buffers. These operations handle various aspects of buffer
/// management including validation, reference counting, buffer stealing, and
/// releasing. Each buffer in a pipe is associated with a set of these
/// operations, which allows the kernel to manage different types of buffers
/// efficiently and safely.
typedef struct pipe_buf_operations {
    /// @brief Ensures that the buffer is valid and ready to be used. This might
    /// involve checking if the buffer contains valid data or if any required
    /// synchronizations are in place.
    int (*confirm)(pipe_inode_info_t *, size_t);

} pipe_buf_operations_t;

/// @brief Updates readers and writers counts for pipes.
/// @param task Pointer to the new task's `task_struct`.
/// @param old_task Pointer to the old task's `task_struct`.
/// @return int 0 on success, 1 on failure.
int vfs_update_pipe_counts(task_struct *task, task_struct *old_task);
