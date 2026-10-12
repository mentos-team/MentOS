/// @file msg.c
/// @brief
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

// ============================================================================
// Setup the logging for this file (do this before any other include).
#include "sys/kernel_levels.h"           // Include kernel log levels.
#define __DEBUG_HEADER__ "[IPCmsg]"      ///< Change header.
#define __DEBUG_LEVEL__  LOGLEVEL_NOTICE ///< Set log level.
#include "io/debug.h"                    // Include debugging functions.
// ============================================================================

#include "assert.h"
#include "errno.h"
#include "fcntl.h"
#include "mem/uaccess.h"
#include "klib/stdatomic.h"
#include "klib/spinlock.h"
#include "process/process.h"
#include "process/scheduler.h"
#include "process/wait.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "sys/msg.h"
#include "system/panic.h"

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
    /// Serializes queue contents, metadata, and removal state.
    spinlock_t lock;
    /// Tasks waiting for either space or a matching message.
    wait_queue_head_t wait_queue;
    /// References held by active syscalls.
    atomic_t references;
    /// Set after IPC_RMID; the queue is no longer discoverable by id/key.
    int removed;
    /// Reference inside the list of message queue management structures.
    list_head_t list;
} msq_info_t;

/// @brief List of all current active Message queues.
list_head_t msq_list;
static spinlock_t msq_list_lock;

// ============================================================================
// MEMORY MANAGEMENT (Private)
// ============================================================================

/// @brief Allocates the memory for message queue structure.
/// @param key IPC_KEY associated with message queue.
/// @param msqflg flags used to create message queue.
/// @return a pointer to the allocated message queue structure.
static inline msq_info_t *__msq_info_alloc(key_t key, int msqflg)
{
    // Allocate the memory.
    msq_info_t *msq_info = (msq_info_t *)kmalloc(sizeof(msq_info_t));
    // Check the allocated memory.
    assert(msq_info && "Failed to allocate memory for a msqaphore management structure.");
    // Clean the memory.
    memset(msq_info, 0, sizeof(msq_info_t));
    spinlock_init(&msq_info->lock);
    wait_queue_head_init(&msq_info->wait_queue);
    atomic_set(&msq_info->references, 1);
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
    struct msg *message = msq_info->msg_first;
    struct msg *next    = NULL;
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
    list_for_each_decl (it, &msq_list) {
        // Get the current entry.
        msq_info = list_entry(it, msq_info_t, list);
        // If message queue set is valid, check the id.
        if (msq_info && (msq_info->id == msqid)) {
            return msq_info;
        }
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
    list_for_each_decl (it, &msq_list) {
        // Get the current entry.
        msq_info = list_entry(it, msq_info_t, list);
        // If message queue set is valid, check the id.
        if (msq_info && (msq_info->msqid.msg_perm.key == key)) {
            return msq_info;
        }
    }
    return NULL;
}

/// @brief Adds the structure to the global list.
/// @param msq_info the structure to add.
static inline void __list_add_msq_info(msq_info_t *msq_info)
{
    assert(msq_info && "Received a NULL pointer.");
    // Add the new msq_info at the end.
    list_head_insert_before(&msq_info->list, &msq_list);
}

/// @brief Removes the structure from the global list.
/// @param msq_info the structure to remove.
static inline void __list_remove_msq_info(msq_info_t *msq_info)
{
    assert(msq_info && "Received a NULL pointer.");
    // Delete the msq_info from the list.
    list_head_remove(&msq_info->list);
}

/// @brief Pushes a messages inside the message queue.
/// @param msq_info the structure that will contain the message.
/// @param message the message to push.
static inline void __msq_info_push_message(msq_info_t *msq_info, struct msg *message)
{
    assert(msq_info && "Received a NULL pointer.");
    assert(message && "Received a NULL pointer.");
    // If there is no first message set, set it.
    if (msq_info->msg_first == NULL) {
        msq_info->msg_first = message;
    }
    // If there is no last message set, set it.
    if (msq_info->msg_last == NULL) {
        msq_info->msg_last = message;
    } else { // Otherwise, append after the last.
        msq_info->msg_last->msg_next = message;
        msq_info->msg_last           = message;
    }
}

/// @brief Removes the message from the message queue.
/// @param msq_info the structure that contains the message.
/// @param message the message to remove.
static inline void __msq_info_remove_message(msq_info_t *msq_info, struct msg *message)
{
    assert(msq_info && "Received a NULL pointer.");
    assert(message && "Received a NULL pointer.");
    // If the message is the first of the queue, next will become the first.
    if (msq_info->msg_first == message) {
        msq_info->msg_first = message->msg_next;
    } else {
        // Otherwise, we need to find the element before it, and connect the
        // previous one with the next one.
        for (struct msg *it = msq_info->msg_first; it; it = it->msg_next) {
            if (it->msg_next == message) {
                it->msg_next = message->msg_next;
                break;
            }
        }
    }
    // If the removed message was the tail, locate its predecessor.
    if (msq_info->msg_last == message) {
        msq_info->msg_last = msq_info->msg_first;
        while (msq_info->msg_last && msq_info->msg_last->msg_next) {
            msq_info->msg_last = msq_info->msg_last->msg_next;
        }
    }
    // Clear the pointer in message.
    message->msg_next = NULL;
}

// ============================================================================
// SYSTEM FUNCTIONS
// ============================================================================

/// @brief Iinitializes the message queue system.
/// @return 0 on success, 1 on failure.
int msq_init(void)
{
    list_head_init(&msq_list);
    spinlock_init(&msq_list_lock);
    return 0;
}

/// @brief Acquire a reference to a queue still present in the namespace.
static msq_info_t *__msq_get_ref(int msqid)
{
    msq_info_t *msq_info;
    spinlock_lock(&msq_list_lock);
    msq_info = __list_find_msq_info_by_id(msqid);
    if (msq_info && !msq_info->removed) {
        atomic_inc(&msq_info->references);
    } else {
        msq_info = NULL;
    }
    spinlock_unlock(&msq_list_lock);
    return msq_info;
}

/// @brief Acquire a reference to a queue found by key.
static msq_info_t *__msq_get_ref_by_key(key_t key)
{
    msq_info_t *msq_info;
    spinlock_lock(&msq_list_lock);
    msq_info = __list_find_msq_info_by_key(key);
    if (msq_info && !msq_info->removed) {
        atomic_inc(&msq_info->references);
    } else {
        msq_info = NULL;
    }
    spinlock_unlock(&msq_list_lock);
    return msq_info;
}

/// @brief Release a syscall reference and reclaim a removed queue if unused.
static void __msq_put_ref(msq_info_t *msq_info)
{
    if (atomic_dec_and_test(&msq_info->references) && msq_info->removed) {
        __msq_info_dealloc(msq_info);
    }
}

int sys_msgget(key_t key, int msgflg)
{
    msq_info_t *msq_info = NULL;
    const int private_key = (key == IPC_PRIVATE);
    if (key != IPC_PRIVATE) {
        msq_info = __msq_get_ref_by_key(key);
        if (!msq_info && !(msgflg & IPC_CREAT)) {
            pr_err("No message queue exists for the given key "
                   "and msgflg did not specify IPC_CREAT.\n");
            return -ENOENT;
        }
        if (msq_info && (msgflg & IPC_CREAT) && (msgflg & IPC_EXCL)) {
            pr_err("IPC_CREAT and IPC_EXCL were specified in msgflg, "
                   "but a message queue already exists for key.\n");
            __msq_put_ref(msq_info);
            return -EEXIST;
        }
        if (msq_info) {
            spinlock_lock(&msq_info->lock);
            int permitted = ipc_valid_mode((mode_t)msgflg, &msq_info->msqid.msg_perm);
            int id = msq_info->id;
            spinlock_unlock(&msq_info->lock);
            __msq_put_ref(msq_info);
            if (!permitted) {
                pr_err("The message queue exists for the given key, "
                       "but the calling process does not have permission to access "
                       "the set.\n");
                return -EACCES;
            }
            return id;
        }
    }

    // Allocate without the namespace lock: allocation may sleep.  Recheck
    // under the lock before publishing so concurrent creators converge on one
    // queue instead of creating duplicate objects for the same key.
    if (private_key) {
        do {
            key = -rand();
            msq_info = __msq_get_ref_by_key(key);
            if (msq_info) {
                __msq_put_ref(msq_info);
            }
        } while (msq_info);
    }
    msq_info_t *candidate = __msq_info_alloc(key, msgflg);
    if (!candidate) {
        return -ENOMEM;
    }
    spinlock_lock(&msq_list_lock);
    msq_info = __list_find_msq_info_by_key(key);
    if (msq_info && !msq_info->removed) {
        atomic_inc(&msq_info->references);
        spinlock_unlock(&msq_list_lock);
        __msq_info_dealloc(candidate);
        if (private_key) {
            // IPC_PRIVATE must always create a distinct queue.  A random key
            // collision is rare, but retrying preserves that contract.
            __msq_put_ref(msq_info);
            return sys_msgget(IPC_PRIVATE, msgflg);
        }
        if (msgflg & IPC_EXCL) {
            __msq_put_ref(msq_info);
            return -EEXIST;
        }
        spinlock_lock(&msq_info->lock);
        int permitted = ipc_valid_mode((mode_t)msgflg, &msq_info->msqid.msg_perm);
        int id = msq_info->id;
        spinlock_unlock(&msq_info->lock);
        __msq_put_ref(msq_info);
        return permitted ? id : -EACCES;
    }
    size_t object_count = 0;
    list_for_each_decl (it, &msq_list) {
        ++object_count;
    }
    if (object_count >= IPC_MAX_OBJECTS) {
        spinlock_unlock(&msq_list_lock);
        __msq_info_dealloc(candidate);
        return -ENOSPC;
    }
    __list_add_msq_info(candidate);
    spinlock_unlock(&msq_list_lock);
    return candidate->id;
}

int sys_msgsnd(int msqid, const void *msgp, size_t msgsz, int msgflg)
{
    msq_info_t *msq_info;
    struct msg *message;
    long msg_type;
    wait_queue_entry_t wait_entry;
    int result = 0;
    // The msqid is less than zero.
    if (msqid < 0) {
        pr_err("The msqid is less than zero.\n");
        return -EINVAL;
    }
    // The value of msgsz is negative.
    if (msgsz > MSGMAX) {
        pr_err("The value of msgsz is above the maximum allowed size.\n");
        return -EINVAL;
    }
    // The header and the text are read out of the caller's memory: the
    // computed length covers both, and cannot wrap past MSGMAX (#191).
    // Read direction only — msgsnd never writes through msgp.
    if (!access_ok(USER_READ, msgp, sizeof(long) + msgsz)) {
        return -EFAULT;
    }
    // Use the template to acess the message.
    memcpy(&msg_type, msgp, sizeof(msg_type));
    if (msg_type <= 0) {
        return -EINVAL;
    }
    message = (struct msg *)kmalloc(sizeof(*message));
    if (!message) {
        return -ENOMEM;
    }
    message->msg_ptr = msgsz ? (char *)kmalloc(msgsz) : NULL;
    if (msgsz && !message->msg_ptr) {
        kfree(message);
        return -ENOMEM;
    }
    if (msgsz) {
        memcpy(message->msg_ptr, (const char *)msgp + sizeof(long), msgsz);
    }
    message->msg_type = msg_type;
    message->msg_size = msgsz;
    message->msg_next = NULL;

    msq_info = __msq_get_ref(msqid);
    // The message queue doesn't exist.
    if (!msq_info) {
        pr_err("The message queue does not exist.\n");
        kfree(message->msg_ptr);
        kfree(message);
        return -EIDRM;
    }
    // Check if the message queue exists for the given key, but the calling
    // process does not have permission to access the set.
    if (msq_info && !ipc_valid_permissions(IPC_ACCESS_READ | IPC_ACCESS_WRITE, &msq_info->msqid.msg_perm)) {
        pr_err("The message queue exists for the given key, but the "
               "calling process does not have permission to access the set.\n");
        result = -EACCES;
        goto send_out;
    }
    wait_queue_entry_init(&wait_entry, scheduler_get_current_process());
    for (;;) {
        prepare_to_wait(&msq_info->wait_queue, &wait_entry, TASK_INTERRUPTIBLE);
        spinlock_lock(&msq_info->lock);
        if (msq_info->removed) {
            result = -EIDRM;
            spinlock_unlock(&msq_info->lock);
            finish_wait(&msq_info->wait_queue, &wait_entry);
            break;
        }
        if (msq_info->msqid.msg_cbytes + msgsz <= msq_info->msqid.msg_qbytes) {
            __msq_info_push_message(msq_info, message);
            msq_info->msqid.msg_stime = sys_time(NULL);
            msq_info->msqid.msg_lspid = sys_getpid();
            msq_info->msqid.msg_cbytes += msgsz;
            msq_info->msqid.msg_qnum += 1;
            spinlock_unlock(&msq_info->lock);
            finish_wait(&msq_info->wait_queue, &wait_entry);
            wake_up_all(&msq_info->wait_queue);
            message = NULL;
            break;
        }
        spinlock_unlock(&msq_info->lock);
        if (msgflg & IPC_NOWAIT) {
            result = -EAGAIN;
            finish_wait(&msq_info->wait_queue, &wait_entry);
            break;
        }
        schedule();
    }

send_out:
    if (message) {
        kfree(message->msg_ptr);
        kfree(message);
    }
    __msq_put_ref(msq_info);
    return result;
}

ssize_t sys_msgrcv(int msqid, void *msgp, size_t msgsz, long msgtyp, int msgflg)
{
    msq_info_t *msq_info;
    wait_queue_entry_t wait_entry;
    ssize_t result = 0;
    // The msqid is less than zero.
    if (msqid < 0) {
        pr_err("The msqid is less than zero.\n");
        return -EINVAL;
    }
    // The value of msgsz is negative.
    if (msgsz > MSGMAX) {
        pr_err("The value of msgsz is above the maximum allowed size.\n");
        return -EINVAL;
    }
    // The header and the text are written into the caller's memory: the
    // computed length covers both, and cannot wrap past MSGMAX (#191).
    if (!access_ok(USER_WRITE, msgp, sizeof(long) + msgsz)) {
        return -EFAULT;
    }
    msq_info = __msq_get_ref(msqid);
    // The message queue doesn't exist.
    if (!msq_info) {
        pr_err("The message queue does not exist.\n");
        return -EIDRM;
    }
    // Check if the message queue exists for the given key, but the calling
    // process does not have permission to access the set.
    if (!ipc_valid_permissions(IPC_ACCESS_READ, &msq_info->msqid.msg_perm)) {
        pr_err("The message queue exists for the given key, but the "
               "calling process does not have read permission to access the "
               "set.\n");
        result = -EACCES;
        __msq_put_ref(msq_info);
        return result;
    }
    wait_queue_entry_init(&wait_entry, scheduler_get_current_process());
    for (;;) {
    struct msg *message = NULL;
    prepare_to_wait(&msq_info->wait_queue, &wait_entry, TASK_INTERRUPTIBLE);
    spinlock_lock(&msq_info->lock);
    if (msq_info->removed) {
        result = -EIDRM;
        spinlock_unlock(&msq_info->lock);
        finish_wait(&msq_info->wait_queue, &wait_entry);
        break;
    }
    // If msgtyp is 0, then the first message in the queue is read.
    if (msgtyp == 0) {
        // Get the first message.
        message = msq_info->msg_first;
    }
    // If msgtyp is greater than 0, then the first message in the queue of type
    // msgtyp is read.
    else if (msgtyp > 0) {
        for (struct msg *it = msq_info->msg_first; it; it = it->msg_next) {
            if (it->msg_type == msgtyp) {
                message = it;
                break;
            }
        }
    }
    // If msgtyp is less than 0, then the first message in the queue with the
    // lowest type less than or equal to the absolute value of msgtyp will be
    // read.
    else {
        long lowest_type = LONG_MAX;
        for (struct msg *it = msq_info->msg_first; it; it = it->msg_next) {
            if ((it->msg_type <= -msgtyp) && (it->msg_type < lowest_type)) {
                lowest_type = it->msg_type;
                message = it;
            }
        }
    }
    if (message == NULL) {
        spinlock_unlock(&msq_info->lock);
        if (msgflg & IPC_NOWAIT) {
            result = -ENOMSG;
            finish_wait(&msq_info->wait_queue, &wait_entry);
            break;
        }
        schedule();
        continue;
    }
    // Check if the message is longer than msgsz.
    if (message->msg_size > msgsz) {
        // If we have the MSG_NOERROR flag, we return E2BIG and leave the
        // message on the queue.
        if (!(msgflg & MSG_NOERROR)) {
            pr_err("The message we are trying to retrieve is too big.\n");
            result = -E2BIG;
            spinlock_unlock(&msq_info->lock);
            finish_wait(&msq_info->wait_queue, &wait_entry);
            break;
        }
        // Otherwise, we truncate the message to msgsz.
    }
    // The number of bytes actually copied.
    ssize_t actual_size = min(message->msg_size, msgsz);
    // Copy the type and content while the message is still owned by the queue.
    memcpy(msgp, &message->msg_type, sizeof(message->msg_type));
    if (actual_size) {
        memcpy((char *)msgp + sizeof(long), message->msg_ptr, actual_size);
    }

    // Update last receive time.
    msq_info->msqid.msg_rtime = sys_time(NULL);
    // Update pid of last process who issued a receive.
    msq_info->msqid.msg_lrpid = sys_getpid();
    // Update the total consumed space of the message queue.
    msq_info->msqid.msg_cbytes -= message->msg_size;
    // Decrement the number of messages in the message queue.
    msq_info->msqid.msg_qnum -= 1;

    // Remove the message to the queue.
    __msq_info_remove_message(msq_info, message);

    spinlock_unlock(&msq_info->lock);
    finish_wait(&msq_info->wait_queue, &wait_entry);
    wake_up_all(&msq_info->wait_queue);
    // Free the memory of the message.
    kfree(message->msg_ptr);
    // Free the memory of the data structure.
    kfree(message);

    result = actual_size;
    break;
    }
    __msq_put_ref(msq_info);
    return result;
}

int sys_msgctl(int msqid, int cmd, struct msqid_ds *buf)
{
    msq_info_t *msq_info = NULL;
    task_struct *task    = NULL;
    struct msqid_ds snapshot;
    // The msqid is less than zero.
    if (msqid < 0) {
        pr_err("The msqid is less than zero.\n");
        return -EINVAL;
    }
    // Hold a syscall reference while inspecting or removing the queue.  The
    // namespace reference is dropped separately by IPC_RMID below.
    msq_info = __msq_get_ref(msqid);
    // The message queue doesn't exist.
    if (!msq_info) {
        pr_err("The message queue does not exist.\n");
        return -EIDRM;
    }
    // Get the calling task.
    task = scheduler_get_current_process();
    assert(task && "Failed to get the current running process.");

    if (cmd == IPC_RMID) {
        // Remove the message queue; any processes blocked is awakened (errno set to
        // EIDRM); no argument required.

        spinlock_lock(&msq_info->lock);
        int can_remove = (msq_info->msqid.msg_perm.uid == task->uid) ||
                         (msq_info->msqid.msg_perm.cuid == task->uid);
        if (!can_remove) {
            spinlock_unlock(&msq_info->lock);
            pr_err("The calling process is not the creator or the owner of the "
                   "queue.\n");
            __msq_put_ref(msq_info);
            return -EPERM;
        }
        // Tombstone before waking waiters.  A blocked sender/receiver holds a
        // reference and will release it after observing EIDRM; freeing the
        // queue here would make that continuation dereference stale memory.
        msq_info->removed = 1;
        spinlock_unlock(&msq_info->lock);
        spinlock_lock(&msq_list_lock);
        __list_remove_msq_info(msq_info);
        spinlock_unlock(&msq_list_lock);
        __msq_put_ref(msq_info); // drop the namespace reference
        wake_up_all(&msq_info->wait_queue);
    } else if (cmd == IPC_STAT) {
        // Place a copy of the msqid_ds data structure in the buffer pointed to
        // by buf: the caller's own, writable memory or nowhere (#191).
        if (!access_ok(USER_WRITE, buf, sizeof(*buf))) {
            __msq_put_ref(msq_info);
            return -EFAULT;
        }
        // Check permissions.
        spinlock_lock(&msq_info->lock);
        int can_read = ipc_valid_permissions(IPC_ACCESS_READ, &msq_info->msqid.msg_perm);
        if (can_read) {
            snapshot = msq_info->msqid;
        }
        spinlock_unlock(&msq_info->lock);
        if (!can_read) {
            pr_err("The calling process does not have read permission to "
                   "access the queue.\n");
            __msq_put_ref(msq_info);
            return -EACCES;
        }
        // Copying all the data.
        memcpy(buf, &snapshot, sizeof(snapshot));
    }
    __msq_put_ref(msq_info);
    return 0;
}

// ============================================================================
// PROCFS FUNCTIONS
// ============================================================================

/// @brief Read function for the proc system.
/// @param file The file.
/// @param buf Buffer where the read content must be placed.
/// @param offset Offset from which we start reading from the file.
/// @param nbyte The number of bytes to read.
/// @return The number of red bytes.
ssize_t procipc_msg_read(vfs_file_t *file, char *buf, off_t offset, size_t nbyte)
{
    if (!file) {
        pr_err("Received a NULL file.\n");
        return -ENOENT;
    }
    size_t buffer_len    = 0;
    size_t read_pos      = 0;
    size_t ret           = 0;
    ssize_t write_count  = 0;
    msq_info_t *msq_info = NULL;
    char buffer[BUFSIZ];

    // Prepare a buffer.
    memset(buffer, 0, BUFSIZ);
    // Prepare the header.
    ret = sprintf(
        buffer, "       key      msqid perms      cbytes       qnum lspid lrpid   uid  "
                " gid  cuid  cgid      stime      rtime      ctime\n");

    spinlock_lock(&msq_list_lock);
    list_for_each_decl (it, &msq_list) {
        // Get the current entry.
        msq_info = list_entry(it, msq_info_t, list);
        // Add the line.
        ret += sprintf(
            buffer + ret, "%10d %11d %6d %12d %11d %6d %6d %6d %6d %6d %6d %11d %11d %11d\n",
            abs(msq_info->msqid.msg_perm.key), msq_info->id, msq_info->msqid.msg_perm.mode, msq_info->msqid.msg_cbytes,
            msq_info->msqid.msg_qnum, msq_info->msqid.msg_lspid, msq_info->msqid.msg_lrpid,
            msq_info->msqid.msg_perm.uid, msq_info->msqid.msg_perm.gid, msq_info->msqid.msg_perm.cuid,
            msq_info->msqid.msg_perm.cgid, msq_info->msqid.msg_stime, msq_info->msqid.msg_rtime,
            msq_info->msqid.msg_ctime);
    }
    spinlock_unlock(&msq_list_lock);
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
