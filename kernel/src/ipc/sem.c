/// @file sem.c
/// @brief
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.
/// @details
/// # 03/04/2023
/// At the moment we have various functions with their description (see
/// comments). The first time that the function semget is called, we are going
/// to generate the list and the first semaphore set with the assumption that we
/// are not given a IPC_PRIVATE key (temporary ofc).
/// We are able to create new semaphores set and generate unique keys
/// (IPC_PRIVATE) with a counter that searches for the first possible key to
/// assign.
/// Temporary idea:
/// - IPC_CREAT flag, it should be working properly, it creates a semaphore set
///   with the given key.
/// - IPC_EXCL flag,  it should be working properly, it returns -1 and sets the
///   errno if the key is already used.
///
/// # 11/04/2023
/// Right now we have a first version of working semaphores in MentOS.
/// We have completed the semctl function and we have implemented the first
/// version of the semop function both user and kernel side.
/// The way it works is pretty straightforward, the user tries to perform an
/// operation and based on the value of the semaphore the kernel returns certain
/// values. If the operation cannot be performed then the user will stay in a
/// while loop. The cycle ends with a positive return value (the operation has
/// been taken care of) or in case of errors.
/// For testing purposes -> you can try the t_semget and the t_sem1 tests. They
/// both use semaphores and blocking / non blocking operations. t_sem1 is also
/// an exercise that was assingned by Professor Drago in the OS course.

// ============================================================================
// Setup the logging for this file (do this before any other include).
#include "sys/kernel_levels.h"           // Include kernel log levels.
#define __DEBUG_HEADER__ "[IPCsem]"      ///< Change header.
#define __DEBUG_LEVEL__  LOGLEVEL_NOTICE ///< Set log level.
#include "io/debug.h"                    // Include debugging functions.
// ============================================================================

#include "ipc/ipc.h"
#include "ipc/trace.h"
#include "sys/sem.h"

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

///@brief A value to compute the semid value.
int __sem_id = 0;

/// @brief Semaphore management structure.
typedef struct {
    /// @brief Semaphore ID associated to the semaphore set.
    int id;
    /// @brief The semaphore data strcutre.
    struct semid_ds semid;
    /// @brief List of all the semaphores.
    struct sem *sem_base;
    /// Serializes values, metadata, and removal state.
    spinlock_t lock;
    /// Tasks waiting for a semaphore predicate to become true.
    wait_queue_head_t wait_queue;
    /// References held by active syscalls, including blocked continuations.
    atomic_t references;
    /// Set after IPC_RMID; the namespace reference has been removed.
    int removed;
    /// Reference inside the list of semaphore management structures.
    list_head_t list;
} sem_info_t;

/// @brief List of all current active semaphores.
list_head_t semaphores_list;
static spinlock_t semaphores_list_lock;

// ============================================================================
// MEMORY MANAGEMENT (Private)
// ============================================================================

/// @brief Allocates the memory for semaphore management structure.
/// @param key IPC_KEY associated with the set of semaphores.
/// @param nsems number of semaphores to initialize.
/// @param semflg flags used to create the set of semaphores.
/// @return a pointer to the allocated semaphore management structure.
static inline sem_info_t *__sem_info_alloc(key_t key, int nsems, int semflg)
{
    // Allocate the memory.
    sem_info_t *sem_info = (sem_info_t *)kmalloc(sizeof(sem_info_t));
    // Check the allocated memory.
    if (!sem_info) {
        pr_err("Failed to allocate memory for a semaphore management structure.\n");
        return NULL;
    }
    // Clean the memory.
    memset(sem_info, 0, sizeof(sem_info_t));
    spinlock_init(&sem_info->lock);
    wait_queue_head_init(&sem_info->wait_queue);
    atomic_set(&sem_info->references, 1);
    // Allocate the memory for semaphores.
    sem_info->sem_base = (struct sem *)kmalloc(sizeof(struct sem) * nsems);
    // Check the allocated memory.
    if (!sem_info->sem_base) {
        pr_err("Failed to allocate memory for a set of semaphores.\n");
        kfree(sem_info);
        return NULL;
    }
    // Clean the memory.
    memset(sem_info->sem_base, 0, sizeof(struct sem) * nsems);
    // Initialize its values.
    sem_info->id              = ++__sem_id;
    sem_info->semid.sem_perm  = register_ipc(key, semflg & 0x1FF);
    sem_info->semid.sem_otime = 0;
    sem_info->semid.sem_ctime = 0;
    sem_info->semid.sem_nsems = nsems;
    for (int i = 0; i < nsems; i++) {
        sem_info->sem_base[i].sem_pid  = sys_getpid();
        sem_info->sem_base[i].sem_val  = 0;
        sem_info->sem_base[i].sem_ncnt = 0;
        sem_info->sem_base[i].sem_zcnt = 0;
    }
    // Return the semaphore management structure.
    return sem_info;
}

/// @brief Frees the memory of a semaphore management structure.
/// @param sem_info pointer to the semaphore management structure.
static inline void __sem_info_dealloc(sem_info_t *sem_info)
{
    assert(sem_info && "Received a NULL pointer.");
    // Deallocate the array of semaphores.
    kfree(sem_info->sem_base);
    // Deallocate the semid memory.
    kfree(sem_info);
}

// ============================================================================
// LIST MANAGEMENT/SEARCH FUNCTIONS (Private)
// ============================================================================

/// @brief Searches for the semaphore with the given id.
/// @param semid the id we are searching.
/// @return the semaphore with the given id.
static inline sem_info_t *__list_find_sem_info_by_id(int semid)
{
    sem_info_t *sem_info;
    // Iterate through the list of semaphore set.
    list_for_each_decl (it, &semaphores_list) {
        // Get the current entry.
        sem_info = list_entry(it, sem_info_t, list);
        // If semaphore set is valid, check the id.
        if (sem_info && (sem_info->id == semid)) {
            return sem_info;
        }
    }
    return NULL;
}

/// @brief Searches for the semaphore with the given key.
/// @param key the key we are searching.
/// @return the semaphore with the given key.
static inline sem_info_t *__list_find_sem_info_by_key(key_t key)
{
    sem_info_t *sem_info;
    // Iterate through the list of semaphore set.
    list_for_each_decl (it, &semaphores_list) {
        // Get the current entry.
        sem_info = list_entry(it, sem_info_t, list);
        // If semaphore set is valid, check the id.
        if (sem_info && (sem_info->semid.sem_perm.key == key)) {
            return sem_info;
        }
    }
    return NULL;
}

/// @brief Adds the structure to the global list.
/// @param sem_info the structure to add.
static inline void __list_add_sem_info(sem_info_t *sem_info)
{
    assert(sem_info && "Received a NULL pointer.");
    // Add the new sem_info at the end.
    list_head_insert_before(&sem_info->list, &semaphores_list);
}

/// @brief Removes the structure from the global list.
/// @param sem_info the structure to remove.
static inline void __list_remove_sem_info(sem_info_t *sem_info)
{
    assert(sem_info && "Received a NULL pointer.");
    // Delete the sem_info from the list.
    list_head_remove(&sem_info->list);
}

// ============================================================================
// SYSTEM FUNCTIONS
// ============================================================================

/// @brief Iinitializes the semaphore system.
/// @return 0 on success, 1 on failure.
int sem_init(void)
{
    list_head_init(&semaphores_list);
    spinlock_init(&semaphores_list_lock);
    return 0;
}

/// @brief Acquire a reference to a semaphore set still present in the namespace.
static sem_info_t *__sem_get_ref(int semid)
{
    sem_info_t *sem_info;
    spinlock_lock(&semaphores_list_lock);
    sem_info = __list_find_sem_info_by_id(semid);
    if (sem_info && !sem_info->removed) {
        atomic_inc(&sem_info->references);
    } else {
        sem_info = NULL;
    }
    spinlock_unlock(&semaphores_list_lock);
    return sem_info;
}

/// @brief Acquire a reference to a semaphore set found by key.
static sem_info_t *__sem_get_ref_by_key(key_t key)
{
    sem_info_t *sem_info;
    spinlock_lock(&semaphores_list_lock);
    sem_info = __list_find_sem_info_by_key(key);
    if (sem_info && !sem_info->removed) {
        atomic_inc(&sem_info->references);
    } else {
        sem_info = NULL;
    }
    spinlock_unlock(&semaphores_list_lock);
    return sem_info;
}

/// @brief Release a syscall reference and reclaim a removed set when unused.
static void __sem_put_ref(sem_info_t *sem_info)
{
    int reclaimed = atomic_dec_and_test(&sem_info->references) && sem_info->removed;
    IPC_TRACE("semaphore set %d: put reference (removed=%d, reclaim=%d)\n", sem_info->id, sem_info->removed, reclaimed);
    if (reclaimed) {
        __sem_info_dealloc(sem_info);
    }
}

long sys_semget(key_t key, int nsems, int semflg)
{
    sem_info_t *sem_info = NULL;
    const int private_key = (key == IPC_PRIVATE);
    // Check if nsems is less than 0 or greater than the maximum number of
    // semaphores per semaphore set.
    if ((nsems <= 0) || (nsems > SEM_SET_MAX)) {
        pr_err("Wrong number of semaphores for semaphore set.\n");
        return -EINVAL;
    }
    if (!private_key) {
        sem_info = __sem_get_ref_by_key(key);
        // An existing set must be validated while its reference is held.
        if (sem_info && (nsems > sem_info->semid.sem_nsems)) {
            __sem_put_ref(sem_info);
            pr_err("Wrong number of semaphores for and existing semaphore set.\n");
            return -EINVAL;
        }
        if (!sem_info && !(semflg & IPC_CREAT)) {
            pr_err("No semaphore set exists for the given key and semflg did "
                   "not specify IPC_CREAT.\n");
            return -ENOENT;
        }

        if (sem_info && (semflg & IPC_CREAT) && (semflg & IPC_EXCL)) {
            pr_err("IPC_CREAT and IPC_EXCL were specified in semflg, but a "
                   "semaphore set already exists for key.\n");
            __sem_put_ref(sem_info);
            return -EEXIST;
        }
        if (sem_info) {
            spinlock_lock(&sem_info->lock);
            int permitted = ipc_valid_mode((mode_t)semflg, &sem_info->semid.sem_perm);
            int id = sem_info->id;
            spinlock_unlock(&sem_info->lock);
            __sem_put_ref(sem_info);
            if (!permitted) {
                pr_err("The semaphore set exists for the given key, but the calling "
                       "process does not have permission to access the set.\n");
                return -EACCES;
            }
            return id;
        }
    }

    if (private_key) {
        do {
            key = -rand();
            sem_info = __sem_get_ref_by_key(key);
            if (sem_info) {
                __sem_put_ref(sem_info);
            }
        } while (sem_info);
    }
    sem_info_t *candidate = __sem_info_alloc(key, nsems, semflg);
    if (!candidate) {
        return -ENOMEM;
    }
    spinlock_lock(&semaphores_list_lock);
    sem_info = __list_find_sem_info_by_key(key);
    if (sem_info && !sem_info->removed) {
        atomic_inc(&sem_info->references);
        spinlock_unlock(&semaphores_list_lock);
        __sem_info_dealloc(candidate);
        if (private_key) {
            __sem_put_ref(sem_info);
            return sys_semget(IPC_PRIVATE, nsems, semflg);
        }
        if (nsems > sem_info->semid.sem_nsems) {
            __sem_put_ref(sem_info);
            return -EINVAL;
        }
        if (semflg & IPC_EXCL) {
            __sem_put_ref(sem_info);
            return -EEXIST;
        }
        spinlock_lock(&sem_info->lock);
        int permitted = ipc_valid_mode((mode_t)semflg, &sem_info->semid.sem_perm);
        int id = sem_info->id;
        spinlock_unlock(&sem_info->lock);
        __sem_put_ref(sem_info);
        return permitted ? id : -EACCES;
    }
    size_t object_count = 0;
    list_for_each_decl (it, &semaphores_list) {
        ++object_count;
    }
    if (object_count >= IPC_MAX_OBJECTS) {
        spinlock_unlock(&semaphores_list_lock);
        __sem_info_dealloc(candidate);
        return -ENOSPC;
    }
    __list_add_sem_info(candidate);
    spinlock_unlock(&semaphores_list_lock);
    return candidate->id;
}

long sys_semop(int semid, struct sembuf *sops, unsigned nsops)
{
    sem_info_t *sem_info;
    struct sembuf *operations;
    wait_queue_entry_t wait_entry;
    int result = 0;
    // The semid is less than zero.
    if (semid < 0) {
        pr_err("The semid is less than zero.\n");
        return -EINVAL;
    }
    if (nsops == 0) {
        return -EINVAL;
    }
    // The whole array of operations is read out of the caller's memory, and
    // the multiplication of count and element size must not be allowed to
    // wrap before the check runs (#191).
    if (((size_t)nsops > (size_t)-1 / sizeof(struct sembuf)) ||
        !access_ok(USER_READ, sops, (size_t)nsops * sizeof(struct sembuf))) {
        return -EFAULT;
    }
    if (nsops > SEMOPM_MAX) {
        pr_err("The number of semaphore operations is outside the supported range.\n");
        return -E2BIG;
    }
    operations = (struct sembuf *)kmalloc(nsops * sizeof(*operations));
    if (!operations) {
        return -ENOMEM;
    }
    memcpy(operations, sops, nsops * sizeof(*operations));

    sem_info = __sem_get_ref(semid);
    // The semaphore set doesn't exist.
    if (!sem_info) {
        pr_err("The semaphore set doesn't exist.\n");
        kfree(operations);
        return -EINVAL;
    }
    // A zero operation observes a value and needs read permission; modifying
    // operations need alter/write permission.  A transaction containing both
    // kinds requires both classes of permission.
    unsigned required_access = 0;
    for (unsigned i = 0; i < nsops; ++i) {
        required_access |= operations[i].sem_op == 0 ? IPC_ACCESS_READ : IPC_ACCESS_WRITE;
    }
    if (!ipc_valid_permissions(required_access, &sem_info->semid.sem_perm)) {
        pr_err("The semaphore set exists for the given key, but the calling "
               "process does not have permission to access the set.\n");
        result = -EACCES;
        goto out;
    }

    wait_queue_entry_init(&wait_entry, scheduler_get_current_process());
    for (;;) {
        int ready = 1;
        IPC_TRACE("semaphore set %d: operation entering wait loop\n", sem_info->id);
        prepare_to_wait(&sem_info->wait_queue, &wait_entry, TASK_INTERRUPTIBLE);
        spinlock_lock(&sem_info->lock);
        if (sem_info->removed) {
            result = -EIDRM;
            spinlock_unlock(&sem_info->lock);
            finish_wait(&sem_info->wait_queue, &wait_entry);
            break;
        }
        for (unsigned i = 0; i < nsops; ++i) {
            struct sembuf *operation = &operations[i];
            if (operation->sem_num >= sem_info->semid.sem_nsems || operation->sem_op < -32767) {
                result = -EFBIG;
                spinlock_unlock(&sem_info->lock);
                finish_wait(&sem_info->wait_queue, &wait_entry);
                goto out;
            }
            unsigned short value = sem_info->sem_base[operation->sem_num].sem_val;
            if ((operation->sem_op < 0 && value < (unsigned short)-operation->sem_op) ||
                (operation->sem_op == 0 && value != 0)) {
                ready = 0;
                break;
            }
            if (operation->sem_op > 0 && value > (unsigned short)(0xFFFF - operation->sem_op)) {
                result = -ERANGE;
                spinlock_unlock(&sem_info->lock);
                finish_wait(&sem_info->wait_queue, &wait_entry);
                goto out;
            }
        }
        if (!ready) {
            int nowait = 0;
            for (unsigned i = 0; i < nsops; ++i) {
                nowait |= (operations[i].sem_flg & IPC_NOWAIT) != 0;
            }
            spinlock_unlock(&sem_info->lock);
            if (nowait) {
                result = -EAGAIN;
                finish_wait(&sem_info->wait_queue, &wait_entry);
                break;
            }
            schedule();
            continue;
        }
        for (unsigned i = 0; i < nsops; ++i) {
            struct sembuf *operation = &operations[i];
            sem_info->sem_base[operation->sem_num].sem_val += operation->sem_op;
            sem_info->sem_base[operation->sem_num].sem_pid = sys_getpid();
        }
        sem_info->semid.sem_otime = sys_time(NULL);
        sem_info->semid.sem_ctime = sys_time(NULL);
        spinlock_unlock(&sem_info->lock);
        finish_wait(&sem_info->wait_queue, &wait_entry);
        IPC_TRACE("semaphore set %d: waking waiters after operation\n", sem_info->id);
        wake_up_all(&sem_info->wait_queue);
        break;
    }

out:
    kfree(operations);
    __sem_put_ref(sem_info);
    return result;
}

/*
 * Execute a semaphore-control command while the caller owns a reference to
 * the set.  The public wrapper below acquires that reference before entering
 * this legacy command dispatcher; keeping the lifetime protocol here avoids
 * having every early-return branch duplicate cleanup code.
 */
static long __semctl(sem_info_t *sem_info, int semnum, int cmd, union semun *arg)
{
    task_struct *task    = NULL;
    struct semid_ds snapshot;
    unsigned short values[SEM_SET_MAX];
    // Get the calling task.
    task = scheduler_get_current_process();
    assert(task && "Failed to get the current running process.");

    if (cmd == IPC_RMID) {
        // Remove the semaphore set; any processes blocked is awakened (errno set to
        // EIDRM); no argument required.

        if ((sem_info->semid.sem_perm.uid != task->uid) && (sem_info->semid.sem_perm.cuid != task->uid)) {
            pr_err("The calling process is not the creator or the owner of the "
                   "semaphore set.\n");
            return -EPERM;
        }
        // Unlink and tombstone the set before waking sleepers.  A blocked
        // semop continuation may still hold a reference, so freeing here
        // would turn a valid wakeup into a use-after-free.
        spinlock_lock(&sem_info->lock);
        sem_info->removed = 1;
        IPC_TRACE("semaphore set %d: IPC_RMID tombstone published\n", sem_info->id);
        spinlock_unlock(&sem_info->lock);
        spinlock_lock(&semaphores_list_lock);
        __list_remove_sem_info(sem_info);
        spinlock_unlock(&semaphores_list_lock);
        __sem_put_ref(sem_info); // drop the namespace reference
        IPC_TRACE("semaphore set %d: waking waiters after IPC_RMID\n", sem_info->id);
        wake_up_all(&sem_info->wait_queue);
        return 0;
    } else if (cmd == SETVAL) {
        // The value of the semnum-th semaphore in the set is initialized to the
        // value specified in arg.val.

        // Check if the index is valid.
        if ((semnum < 0) || (semnum >= (sem_info->semid.sem_nsems))) {
            pr_err("Semaphore number out of bound (%d not in [%d, %d])\n", semnum, 0, sem_info->semid.sem_nsems);
            return -EINVAL;
        }
        // The argument is read out of the caller's memory (#191).
        if (!access_ok(USER_READ, arg, sizeof(*arg))) {
            return -EFAULT;
        }
        // Checking if the value is valid.
        if (arg->val < 0) {
            pr_err("The value to set is not valid %d.\n", arg->val);
            return -EINVAL;
        }
        spinlock_lock(&sem_info->lock);
        int can_write = ipc_valid_permissions(IPC_ACCESS_WRITE, &sem_info->semid.sem_perm);
        if (!can_write) {
            spinlock_unlock(&sem_info->lock);
            pr_err("The calling process does not have read permission to "
                   "access the set.\n");
            return -EACCES;
        }
        // Setting the value.
        sem_info->sem_base[semnum].sem_val = arg->val;
        // Update the last change time.
        sem_info->semid.sem_ctime          = sys_time(NULL);
        spinlock_unlock(&sem_info->lock);
    } else if (cmd == SETALL) {
        // Initialize all semaphore in the set referred to by semid, using the
        // values supplied in the array pointed to by arg.array.

        // The argument and the array it points to are read out of the
        // caller's memory (#191).
        if (!access_ok(USER_READ, arg, sizeof(*arg))) {
            return -EFAULT;
        }
        if (!arg->array ||
            !access_ok(USER_READ, arg->array, sem_info->semid.sem_nsems * sizeof(*arg->array))) {
            return -EFAULT;
        }
        // Copy user data before taking the object lock: the copy itself may
        // fault and must never happen while a spinlock is held.
        memcpy(values, arg->array, sem_info->semid.sem_nsems * sizeof(*values));
        spinlock_lock(&sem_info->lock);
        int can_write = ipc_valid_permissions(IPC_ACCESS_WRITE, &sem_info->semid.sem_perm);
        if (!can_write) {
            spinlock_unlock(&sem_info->lock);
            pr_err("The calling process does not have read permission to "
                   "access the set.\n");
            return -EACCES;
        }
        // Setting the values.
        for (unsigned i = 0; i < sem_info->semid.sem_nsems; ++i) {
            sem_info->sem_base[i].sem_val = values[i];
        }
        // Update the last change time.
        sem_info->semid.sem_ctime = sys_time(NULL);
        spinlock_unlock(&sem_info->lock);
    } else if (cmd == IPC_STAT) {
        // Place a copy of the semid_ds data structure in the buffer pointed to by
        // arg.buf.

        // The union itself is only read, to reach the buffer; the answer
        // is what gets written, so only the buffer needs the write
        // direction (#191).
        if (!access_ok(USER_READ, arg, sizeof(*arg))) {
            return -EFAULT;
        }
        if (!arg->buf || !access_ok(USER_WRITE, arg->buf, sizeof(*arg->buf))) {
            return -EFAULT;
        }
        spinlock_lock(&sem_info->lock);
        int can_read = ipc_valid_permissions(IPC_ACCESS_READ, &sem_info->semid.sem_perm);
        if (can_read) {
            snapshot = sem_info->semid;
        }
        spinlock_unlock(&sem_info->lock);
        if (!can_read) {
            pr_err("The calling process does not have read permission to "
                   "access the set.\n");
            return -EACCES;
        }
        // Copying all the data.
        memcpy(arg->buf, &snapshot, sizeof(snapshot));
    } else if (cmd == GETALL) {
        // Retrieve the values of all of the semaphores in the set referred to by
        // semid, placing them in the array pointed to by arg.array.

        // The union is read to reach the array; the array is what gets
        // written (#191).
        if (!access_ok(USER_READ, arg, sizeof(*arg))) {
            return -EFAULT;
        }
        if (!arg->array ||
            !access_ok(USER_WRITE, arg->array, sem_info->semid.sem_nsems * sizeof(*arg->array))) {
            return -EFAULT;
        }
        spinlock_lock(&sem_info->lock);
        int can_read = ipc_valid_permissions(IPC_ACCESS_READ, &sem_info->semid.sem_perm);
        if (can_read) {
            for (unsigned i = 0; i < sem_info->semid.sem_nsems; ++i) {
                values[i] = sem_info->sem_base[i].sem_val;
            }
        }
        unsigned nsems = sem_info->semid.sem_nsems;
        spinlock_unlock(&sem_info->lock);
        if (!can_read) {
            return -EACCES;
        }
        memcpy(arg->array, values, nsems * sizeof(*values));
    } else if (cmd == GETVAL) {
        // Returns the value of the semnum-th semaphore in the set specified by
        // semid; no argument required.

        // Check if the index is valid.
        if ((semnum < 0) || (semnum >= (sem_info->semid.sem_nsems))) {
            pr_err("Semaphore number out of bound (%d not in [%d, %d])\n", semnum, 0, sem_info->semid.sem_nsems);
            return -EINVAL;
        }
        spinlock_lock(&sem_info->lock);
        int can_read = ipc_valid_permissions(IPC_ACCESS_READ, &sem_info->semid.sem_perm);
        if (!can_read) {
            spinlock_unlock(&sem_info->lock);
            pr_err("The calling process does not have read permission to "
                   "access the set.\n");
            return -EACCES;
        }
        int value = sem_info->sem_base[semnum].sem_val;
        spinlock_unlock(&sem_info->lock);
        return value;
    } else if (cmd == GETPID) {
        // Return the process ID of the last process to perform a semop on the
        // semnum-th semaphore.

        // Check if the index is valid.
        if ((semnum < 0) || (semnum >= (sem_info->semid.sem_nsems))) {
            pr_err("Semaphore number out of bound (%d not in [%d, %d])\n", semnum, 0, sem_info->semid.sem_nsems);
            return -EINVAL;
        }
        spinlock_lock(&sem_info->lock);
        int can_read = ipc_valid_permissions(IPC_ACCESS_READ, &sem_info->semid.sem_perm);
        if (!can_read) {
            spinlock_unlock(&sem_info->lock);
            pr_err("The calling process does not have read permission to "
                   "access the set.\n");
            return -EACCES;
        }
        int pid = sem_info->sem_base[semnum].sem_pid;
        spinlock_unlock(&sem_info->lock);
        return pid;
    } else if (cmd == GETNCNT) {
        // Return the number of processes currently waiting for the resources to
        // become available.

        // Check if the index is valid.
        if ((semnum < 0) || (semnum >= (sem_info->semid.sem_nsems))) {
            pr_err("Semaphore number out of bound (%d not in [%d, %d])\n", semnum, 0, sem_info->semid.sem_nsems);
            return -EINVAL;
        }
        spinlock_lock(&sem_info->lock);
        int can_read = ipc_valid_permissions(IPC_ACCESS_READ, &sem_info->semid.sem_perm);
        if (!can_read) {
            spinlock_unlock(&sem_info->lock);
            pr_err("The calling process does not have read permission to "
                   "access the set.\n");
            return -EACCES;
        }
        int ncnt = sem_info->sem_base[semnum].sem_ncnt;
        spinlock_unlock(&sem_info->lock);
        return ncnt;
    } else if (cmd == GETZCNT) {
        // Return the number of processes currently waiting for the value of the
        // semnum-th semaphore to become 0.

        // Check if the index is valid.
        if ((semnum < 0) || (semnum >= (sem_info->semid.sem_nsems))) {
            pr_err("Semaphore number out of bound (%d not in [%d, %d])\n", semnum, 0, sem_info->semid.sem_nsems);
            return -EINVAL;
        }
        spinlock_lock(&sem_info->lock);
        int can_read = ipc_valid_permissions(IPC_ACCESS_READ, &sem_info->semid.sem_perm);
        if (!can_read) {
            spinlock_unlock(&sem_info->lock);
            pr_err("The calling process does not have read permission to "
                   "access the set.\n");
            return -EACCES;
        }
        int zcnt = sem_info->sem_base[semnum].sem_zcnt;
        spinlock_unlock(&sem_info->lock);
        return zcnt;
    } else if (cmd == SEM_STAT) {
        pr_err("Not implemented.\n");
        return -ENOSYS;
    } else if (cmd == SEM_INFO) {
        pr_err("Not implemented.\n");
        return -ENOSYS;
    } else {
        return -EINVAL;
    }
    return 0;
}

/**
 * @brief Apply a control operation to a semaphore set.
 *
 * The reference acquired here keeps the object alive across metadata copies
 * and user-memory validation.  IPC_RMID may unlink the set while the command
 * is running, but reclamation is deferred until this reference is released.
 *
 * @param semid semaphore-set identifier.
 * @param semnum semaphore index for per-semaphore commands.
 * @param cmd control operation.
 * @param arg command-specific user argument.
 * @return command result or a negative errno value.
 */
long sys_semctl(int semid, int semnum, int cmd, union semun *arg)
{
    sem_info_t *sem_info = __sem_get_ref(semid);
    if (!sem_info) {
        return -EINVAL;
    }

    long result = __semctl(sem_info, semnum, cmd, arg);
    __sem_put_ref(sem_info);
    return result;
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
ssize_t procipc_sem_read(vfs_file_t *file, char *buf, off_t offset, size_t nbyte)
{
    if (!file) {
        pr_err("Received a NULL file.\n");
        return -ENOENT;
    }
    size_t buffer_len    = 0;
    size_t read_pos      = 0;
    size_t ret           = 0;
    ssize_t write_count  = 0;
    sem_info_t *sem_info = NULL;
    char buffer[BUFSIZ];

    // Prepare a buffer.
    memset(buffer, 0, BUFSIZ);
    // Prepare the header.
    ret = sprintf(
        buffer, "key      semid perms      nsems   uid   gid  cuid  cgid      "
                "otime      ctime\n");

    // Iterate through the list of semaphore set.
    spinlock_lock(&semaphores_list_lock);
    list_for_each_decl (it, &semaphores_list) {
        // Get the current entry.
        sem_info = list_entry(it, sem_info_t, list);
        // Add the line.
        ret += sprintf(
            buffer + ret, "%8d %5d %10d %7d %5d %4d %5d %9d %10d %d\n", abs(sem_info->semid.sem_perm.key), sem_info->id,
            sem_info->semid.sem_perm.mode, sem_info->semid.sem_nsems, sem_info->semid.sem_perm.uid,
            sem_info->semid.sem_perm.gid, sem_info->semid.sem_perm.cuid, sem_info->semid.sem_perm.cgid,
            sem_info->semid.sem_otime, sem_info->semid.sem_ctime);
    }
    spinlock_unlock(&semaphores_list_lock);
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
