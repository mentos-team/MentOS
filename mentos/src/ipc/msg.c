/// @file msg.c
/// @brief
/// @copyright (c) 2014-2023 This file is distributed under the MIT License.
/// See LICENSE.md for details.

// ============================================================================
// Setup the logging for this file (do this before any other include).
#include "sys/kernel_levels.h"           // Include kernel log levels.
#define __DEBUG_HEADER__ "[IPCmsg]"      ///< Change header.
#define __DEBUG_LEVEL__  LOGLEVEL_NOTICE ///< Set log level.
#include "io/debug.h"                    // Include debugging functions.
// ============================================================================

#include "process/process.h"
#include "system/panic.h"
#include "sys/errno.h"
#include "sys/msg.h"
#include "string.h"
#include "assert.h"
#include "stdio.h"

///@brief A value to compute the message queue ID.
static int __msq_id = 0;

/// @brief Message queue management structure.
typedef struct {
    /// @brief ID associated to the message queue.
    int id;
    /// @brief The message queue data strcutre.
    struct msqid_ds msqid;
    /// Pointer to the first message in the queue.
    struct msg *msg_base;
    /// Reference inside the list of message queue management structures.
    list_head list;
} msq_info_t;

/// @brief List of all current active Message queues.
list_head msq_list;

// ============================================================================
// MEMORY MANAGEMENT (Private)
// ============================================================================

// ============================================================================
// SEARCH FUNCTIONS (Private)
// ============================================================================

// ============================================================================
// SYSTEM FUNCTIONS
// ============================================================================

int msq_init()
{
    list_head_init(&msq_list);
    return 0;
}

long sys_msgget(key_t key, int msgflg)
{
    TODO("Not implemented");
    return 0;
}

long sys_msgsnd(int msqid, struct msgbuf *msgp, size_t msgsz, int msgflg)
{
    TODO("Not implemented");
    return 0;
}

long sys_msgrcv(int msqid, struct msgbuf *msgp, size_t msgsz, long msgtyp, int msgflg)
{
    TODO("Not implemented");
    return 0;
}

long sys_msgctl(int msqid, int cmd, struct msqid_ds *buf)
{
    TODO("Not implemented");
    return 0;
}

// ============================================================================
// PROCFS FUNCTIONS
// ============================================================================

ssize_t procipc_msg_read(vfs_file_t *file, char *buf, off_t offset, size_t nbyte)
{
    if (!file) {
        pr_err("Received a NULL file.\n");
        return -ENOENT;
    }
    size_t buffer_len = 0, read_pos = 0, write_count = 0, ret = 0;
    struct semid_ds *entry = NULL;
    char buffer[BUFSIZ];

    // Prepare a buffer.
    memset(buffer, 0, BUFSIZ);
    // Prepare the header.
    ret = sprintf(buffer, "key      msqid ...\n");

    // Implementation goes here...
    sprintf(buffer + ret, "\n");

    // Perform read.
    buffer_len = strlen(buffer);
    read_pos   = offset;
    if (read_pos < buffer_len) {
        while ((write_count < nbyte) && (read_pos < buffer_len)) {
            buf[write_count] = buffer[read_pos];
            // Move the pointers.
            ++read_pos, ++write_count;
        }
    }
    return write_count;
}
