/// @file msg.c
/// @brief
/// @copyright (c) 2014-2023 This file is distributed under the MIT License.
/// See LICENSE.md for details.

// ============================================================================
// Setup the logging for this file (do this before any other include).
#include "klib/list_head.h"
#include "mem/slab.h"
#include "stddef.h"
#include "sys/kernel_levels.h" // Include kernel log levels.
#include "system/syscall.h"
#define __DEBUG_HEADER__ "[IPCmsg]"      ///< Change header.
#define __DEBUG_LEVEL__  LOGLEVEL_NOTICE ///< Set log level.
#include "io/debug.h"                    // Include debugging functions.
// ============================================================================

#include "process/process.h"
#include "system/panic.h"
#include "sys/errno.h"
#include "sys/msg.h"
#include "stdlib.h"
#include "string.h"
#include "assert.h"
#include "stdio.h"
#include "fcntl.h"

#include "ipc/ipc.h"

///@brief A value to compute the message queue ID.
static int __msq_id = 0;

/// @brief Message queue management structure.
typedef struct {
    /// @brief ID associated to the message queue.
    int id;
    /// @brief The message queue data strcutre.
    struct msqid_ds msqid;
    /// Pointer to the first message in the queue.
    struct msg *msg_first;
    /// Pointer to the last message in the queue.
    struct msg *msg_last;
    /// Reference inside the list of message queue management structures.
    list_head list;
} msq_info_t;

/// @brief List of all current active Message queues.
list_head msq_list;

// ============================================================================
// MEMORY MANAGEMENT (Private)
// ============================================================================

/// @brief Allocates the memory for message queue structure.
/// @param key IPC_KEY associated with message queue.
/// @param msgflg flags used to create message queue.
/// @return a pointer to the allocated message queue structure.
static inline msq_info_t *__msq_info_alloc(key_t key, int msqflg)
{
    // Allocate the memory.
    msq_info_t *msq_info = (msq_info_t *)kmalloc(sizeof(msq_info_t));
    // Check the allocated memory.
    assert(msq_info && "Failed to allocate memory for a msqaphore management structure.");
    // Clean the memory.
    memset(msq_info, 0, sizeof(msq_info_t));
    // Initialize it.
    msq_info->id        = ++__msq_id;
    msq_info->msg_first = NULL;
    msq_info->msg_last  = NULL;
    list_head_init(&msq_info->list);
    // Initialize the internal data structure.
    msq_info->msqid.msg_perm   = register_ipc(key, msqflg & 0x1FF);
    msq_info->msqid.msg_stime  = 0;
    msq_info->msqid.msg_rtime  = 0;
    msq_info->msqid.msg_ctime  = sys_time(NULL);
    msq_info->msqid.msg_cbytes = 0;
    msq_info->msqid.msg_qnum   = 0;
    msq_info->msqid.msg_qbytes = MSGMNB;
    msq_info->msqid.msg_lspid  = 0;
    msq_info->msqid.msg_lrpid  = 0;
    // Return the message queue management structure.
    return msq_info;
}

/// @brief Frees the memory of a message queue management structure.
/// @param msq_info pointer to the message queue management structure.
static inline void __msq_info_dealloc(msq_info_t *msq_info)
{
    assert(msq_info && "Received a NULL pointer.");
    // Free the memory of all the messages.
    struct msg *message = msq_info->msg_first, *next = NULL;
    while (message) {
        next = message->msg_next;
        // Free the memory of the message.
        kfree(message->msg_ptr);
        // Fre the memory of the msg structure.
        kfree(message);
        // Move to the next.
        message = next;
    }
    // Deallocate the memory.
    kfree(msq_info);
}

// ============================================================================
// LIST MANAGEMENT/SEARCH FUNCTIONS (Private)
// ============================================================================

/// @brief Searches for the message queue with the given id.
/// @param msqid the id we are searching.
/// @return the message queue with the given id.
static inline msq_info_t *__list_find_msq_info_by_id(int msqid)
{
    msq_info_t *msq_info;
    // Iterate through the list of message queue set.
    list_for_each_decl(it, &msq_list)
    {
        // Get the current entry.
        msq_info = list_entry(it, msq_info_t, list);
        // If message queue set is valid, check the id.
        if (msq_info && (msq_info->id == msqid))
            return msq_info;
    }
    return NULL;
}

/// @brief Searches for the message queue with the given key.
/// @param key the key we are searching.
/// @return the message queue with the given key.
static inline msq_info_t *__list_find_msq_info_by_key(key_t key)
{
    msq_info_t *msq_info;
    // Iterate through the list of message queue set.
    list_for_each_decl(it, &msq_list)
    {
        // Get the current entry.
        msq_info = list_entry(it, msq_info_t, list);
        // If message queue set is valid, check the id.
        if (msq_info && (msq_info->msqid.msg_perm.key == key))
            return msq_info;
    }
    return NULL;
}

static inline void __list_add_msq_info(msq_info_t *msq_info)
{
    assert(msq_info && "Received a NULL pointer.");
    // Add the new msq_info at the end.
    list_head_insert_before(&msq_info->list, &msq_list);
}

static inline void __list_remove_msq_info(msq_info_t *msq_info)
{
    assert(msq_info && "Received a NULL pointer.");
    // Delete the msq_info from the list.
    list_head_remove(&msq_info->list);
}

static inline void __msq_info_push_message(msq_info_t *msq_info, struct msg *message)
{
    assert(msq_info && "Received a NULL pointer.");
    assert(message && "Received a NULL pointer.");
    // If there is no first message set, set it.
    if (!msq_info->msg_first)
        msq_info->msg_first = message;
    // If there is no last message set, set it.
    if (!msq_info->msg_last)
        msq_info->msg_last = message;
    else // Otherwise, append after the last.
        msq_info->msg_last->msg_next = message;
}

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
    msq_info_t *msq_info = NULL;
    // Need to find a unique key.
    if (key == IPC_PRIVATE) {
        // Exit when i find a unique key.
        do {
            key = -rand();
        } while (__list_find_msq_info_by_key(key));
        // We have a unique key, create the message queue.
        msq_info = __msq_info_alloc(key, msgflg);
        // Add the message queue to the list.
        __list_add_msq_info(msq_info);
    } else {
        // Get the message queue if it exists.
        msq_info = __list_find_msq_info_by_key(key);

        // Check if no message queue exists for the given key and msgflg did not
        // specify IPC_CREAT.
        if (!msq_info && !(msgflg & IPC_CREAT)) {
            pr_err("No message queue exists for the given key "
                   "and msgflg did not specify IPC_CREAT.\n");
            return -ENOENT;
        }

        // Check if IPC_CREAT and IPC_EXCL were specified in msgflg, but a
        // message queue already exists for key.
        if (msq_info && (msgflg & IPC_CREAT) && (msgflg & IPC_EXCL)) {
            pr_err("IPC_CREAT and IPC_EXCL were specified in msgflg, "
                   "but a message queue already exists for key.\n");
            return -EEXIST;
        }

        // Check if the message queue exists for the given key, but the calling
        // process does not have permission to access the set.
        if (msq_info && !ipc_valid_permissions(msgflg, &msq_info->msqid.msg_perm)) {
            pr_err("The message queue exists for the given key, "
                   "but the calling process does not have permission to access the set.\n");
            return -EACCES;
        }
        // If the message queue does not exist we need to create a new one.
        if (msq_info == NULL) {
            // Create the message queue.
            msq_info = __msq_info_alloc(key, msgflg);
            // Add the message queue to the list.
            __list_add_msq_info(msq_info);
        }
    }
    // Return the id of the message queue.
    return msq_info->id;
}

int sys_msgsnd(int msqid, struct msgbuf *msgp, size_t msgsz, int msgflg)
{
    msq_info_t *msq_info = NULL;
    // The msqid is less than zero.
    if (msqid < 0) {
        pr_err("The msqid is less than zero.\n");
        return -EINVAL;
    }
    // The pointer to the caller-defined structure is NULL.
    if (!msgp) {
        pr_err("The pointer to the caller-defined structure is NULL.\n");
        return -EINVAL;
    }
    // The value of msgsz is negative.
    if (msgsz <= 0) {
        pr_err("The value of msgsz is negative.\n");
        return -EINVAL;
    }
    // The value of msgsz is above the maximum size.
    if (msgsz >= MSGMAX) {
        pr_err("The value of msgsz above the maximum allowed size.\n");
        return -EINVAL;
    }
    // Search for the message queue.
    msq_info = __list_find_msq_info_by_id(msqid);
    // The message queue doesn't exist.
    if (!msq_info) {
        pr_err("The message queue does not exist.\n");
        return -EINVAL;
    }
    // Check if the message queue exists for the given key, but the calling
    // process does not have permission to access the set.
    if (msq_info && !ipc_valid_permissions(O_RDWR, &msq_info->msqid.msg_perm)) {
        pr_err("The message queue exists for the given key, but the "
               "calling process does not have permission to access the set.\n");
        return -EACCES;
    }
    if (((msq_info->msqid.msg_cbytes + msgsz) >= msq_info->msqid.msg_qbytes)) {
        pr_err("The message can't be sent due to the message queue limit.\n");
        return -EFAULT;
    }
    // Allocate the memory for the message.
    struct msg *message = (struct msg *)kmalloc(sizeof(struct msg));
    if (message == NULL) {
        pr_err("We failed to allocate the memory for the message.\n");
        return -ENOMEM;
    }
    // Initialize the pointer to the next.
    message->msg_next = NULL;
    // Copy the type of message.
    message->msg_type = msgp->mtype;
    // Allocate the memory for the content of the message.
    message->msg_ptr = (char *)kmalloc(msgsz);
    if (message->msg_ptr == NULL) {
        pr_err("We failed to allocate the memory for the message.\n");
        kfree(message);
        return -ENOMEM;
    }
    // Copy the content of the message.
    memcpy(message->msg_ptr, msgp->mtext, msgsz);
    // The length of the message.
    message->msg_size = msgsz;
    // Add the message to the queue.
    __msq_info_push_message(msq_info, message);

    // Update last send time.
    msq_info->msqid.msg_stime = sys_time(NULL);
    // Update pid of last process who issued a send.
    msq_info->msqid.msg_lspid = sys_getpid();
    // Update the total consumed space of the message queue.
    msq_info->msqid.msg_cbytes += msgsz;
    // Increment the number of messages in the message queue.
    msq_info->msqid.msg_qnum += 1;

    return 0;
}

ssize_t sys_msgrcv(int msqid, struct msgbuf *msgp, size_t msgsz, long msgtyp, int msgflg)
{
    msq_info_t *msq_info = NULL;
    // The msqid is less than zero.
    if (msqid < 0) {
        pr_err("The msqid is less than zero.\n");
        return -EINVAL;
    }
    // The pointer to the caller-defined structure is NULL.
    if (!msgp) {
        pr_err("The pointer to the caller-defined structure is NULL.\n");
        return -EINVAL;
    }
    // The value of msgsz is negative.
    if (msgsz <= 0) {
        pr_err("The value of msgsz is negative.\n");
        return -EINVAL;
    }
    // The value of msgsz is above the maximum size.
    if (msgsz >= MSGMAX) {
        pr_err("The value of msgsz above the maximum allowed size.\n");
        return -EINVAL;
    }
    // Search for the message queue.
    msq_info = __list_find_msq_info_by_id(msqid);
    // The message queue doesn't exist.
    if (!msq_info) {
        pr_err("The message queue does not exist.\n");
        return -EINVAL;
    }
    // Check if the message queue exists for the given key, but the calling
    // process does not have permission to access the set.
    if (msq_info && !ipc_valid_permissions(O_RDONLY, &msq_info->msqid.msg_perm)) {
        pr_err("The message queue exists for the given key, but the "
               "calling process does not have read permission to access the set.\n");
        return -EACCES;
    }
    // Update last receive time.
    msq_info->msqid.msg_rtime = sys_time(NULL);
    // Update pid of last process who issued a receive.
    msq_info->msqid.msg_lrpid = sys_getpid();

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
    struct msqid_ds *entry = NULL;
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
