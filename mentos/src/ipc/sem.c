/// @file sem.c
/// @brief
/// @copyright (c) 2014-2023 This file is distributed under the MIT License.
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

#include "sys/sem.h"
#include "ipc/ipc.h"

#include "process/scheduler.h"
#include "process/process.h"
#include "klib/list.h"
#include "sys/errno.h"
#include "stdlib.h"
#include "string.h"
#include "assert.h"
#include "stdio.h"
#include "fcntl.h"

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
    /// Reference inside the list of semaphore management structures.
    list_head list;
} sem_info_t;

/// @brief List of all current active semaphores.
list_head semaphores_list;

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
    assert(sem_info && "Failed to allocate memory for a semaphore management structure.");
    // Clean the memory.
    memset(sem_info, 0, sizeof(sem_info_t));
    // Allocate the memory for semaphores.
    sem_info->sem_base = (struct sem *)kmalloc(sizeof(struct sem) * nsems);
    // Check the allocated memory.
    assert(sem_info->sem_base && "Failed to allocate memory for a set of semaphores.");
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
// SEARCH FUNCTIONS (Private)
// ============================================================================

/// @brief Searches for the semaphore with the given id.
/// @param semid the id we are searching.
/// @return the semaphore with the given id.
static inline sem_info_t *__list_find_sem_info_by_id(int semid)
{
    sem_info_t *sem_info;
    // Iterate through the list of semaphore set.
    list_for_each_decl(it, &semaphores_list)
    {
        // Get the current entry.
        sem_info = list_entry(it, sem_info_t, list);
        // If semaphore set is valid, check the id.
        if (sem_info && (sem_info->id == semid))
            return sem_info;
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
    list_for_each_decl(it, &semaphores_list)
    {
        // Get the current entry.
        sem_info = list_entry(it, sem_info_t, list);
        // If semaphore set is valid, check the id.
        if (sem_info && (sem_info->semid.sem_perm.key == key))
            return sem_info;
    }
    return NULL;
}

static inline void __list_add_sem_info(sem_info_t *sem_info)
{
    assert(sem_info && "Received a NULL pointer.");
    // Add the new sem_info at the end.
    list_head_insert_before(&sem_info->list, &semaphores_list);
}

static inline void __list_remove_sem_info(sem_info_t *sem_info)
{
    assert(sem_info && "Received a NULL pointer.");
    // Delete the sem_info from the list.
    list_head_remove(&sem_info->list);
}

// ============================================================================
// SYSTEM FUNCTIONS
// ============================================================================

int sem_init()
{
    list_head_init(&semaphores_list);
    return 0;
}

long sys_semget(key_t key, int nsems, int semflg)
{
    sem_info_t *sem_info = NULL;
    // Check if nsems is less than 0 or greater than the maximum number of
    // semaphores per semaphore set.
    if ((nsems < 0) || (nsems > SEM_SET_MAX)) {
        pr_err("Wrong number of semaphores for semaphore set.\n");
        return -EINVAL;
    }
    // Need to find a unique key.
    if (key == IPC_PRIVATE) {
        // Exit when i find a unique key.
        do {
            key = -rand();
        } while (__list_find_sem_info_by_key(key));
        // We have a unique key, create the semaphore set.
        sem_info = __sem_info_alloc(key, nsems, semflg);
        // Add the semaphore set to the list.
        __list_add_sem_info(sem_info);
    } else {
        // Get the semaphore set if it exists.
        sem_info = __list_find_sem_info_by_key(key);

        // Check if a semaphore set with the given key already exists, but nsems is
        // larger than the number of semaphores in that set.
        if (sem_info && (nsems > sem_info->semid.sem_nsems)) {
            pr_err("Wrong number of semaphores for and existing semaphore set.\n");
            return -EINVAL;
        }

        // Check if no semaphore set exists for the given key and semflg did not
        // specify IPC_CREAT.
        if (!sem_info && !(semflg & IPC_CREAT)) {
            pr_err("No semaphore set exists for the given key and semflg did not specify IPC_CREAT.\n");
            return -ENOENT;
        }

        // Check if IPC_CREAT and IPC_EXCL were specified in semflg, but a semaphore
        // set already exists for key.
        if (sem_info && (semflg & IPC_CREAT) && (semflg & IPC_EXCL)) {
            pr_err("IPC_CREAT and IPC_EXCL were specified in semflg, but a semaphore set already exists for key.\n");
            return -EEXIST;
        }

        // Check if the semaphore set exists for the given key, but the calling
        // process does not have permission to access the set.
        if (sem_info && !ipc_valid_permissions(semflg, &sem_info->semid.sem_perm)) {
            pr_err("The semaphore set exists for the given key, but the calling process does not have permission to access the set.\n");
            return -EACCES;
        }
        // If the semaphore set does not exist we need to create a new one.
        if (sem_info == NULL) {
            // Create the semaphore set.
            sem_info = __sem_info_alloc(key, nsems, semflg);
            // Add the semaphore set to the list.
            __list_add_sem_info(sem_info);
        }
    }
    // Return the id of the semaphore set.
    return sem_info->id;
}

long sys_semop(int semid, struct sembuf *sops, unsigned nsops)
{
    sem_info_t *sem_info = NULL;
    // The semid is less than zero.
    if (semid < 0) {
        pr_err("The semid is less than zero.\n");
        return -EINVAL;
    }
    // The pointer to the operation is NULL.
    if (!sops) {
        pr_err("The pointer to the operation is NULL.\n");
        return -EINVAL;
    }
    // The value of nsops is negative.
    if (nsops <= 0) {
        pr_err("The value of nsops is negative.\n");
        return -EINVAL;
    }
    // Search for the semaphore.
    sem_info = __list_find_sem_info_by_id(semid);
    // The semaphore set doesn't exist.
    if (!sem_info) {
        pr_err("The semaphore set doesn't exist.\n");
        return -EINVAL;
    }
    // The value of sem_num is less than 0 or greater than or equal to the number of semaphores in the set.
    if ((sops->sem_num < 0) || (sops->sem_num >= sem_info->semid.sem_nsems)) {
        pr_err("The value of sem_num is less than 0 or greater than or equal to the number of semaphores in the set.\n");
        return -EFBIG;
    }
    // Check if the semaphore set exists for the given key, but the calling
    // process does not have permission to access the set.
    if (sem_info && !ipc_valid_permissions(O_RDWR, &sem_info->semid.sem_perm)) {
        pr_err("The semaphore set exists for the given key, but the calling process does not have permission to access the set.\n");
        return -EACCES;
    }
    // Update semop time.
    sem_info->semid.sem_otime = sys_time(NULL);
    // If the operation is negative then we need to check for possible blocking
    // operation. If the value of the sem were to become negative then we return
    // a special value.
    if (((int)sem_info->sem_base[sops->sem_num].sem_val + (int)sops->sem_op) < 0) {
        // The value would become negative, we cannot perform the operation.
        return -EAGAIN;
    }
    // Update the semaphore value.
    sem_info->sem_base[sops->sem_num].sem_val += sops->sem_op;
    // Update the pid of the process that did last op.
    sem_info->sem_base[sops->sem_num].sem_pid = sys_getpid();
    // Update the time.
    sem_info->semid.sem_ctime = sys_time(NULL);
    return 0;
}

long sys_semctl(int semid, int semnum, int cmd, union semun *arg)
{
    sem_info_t *sem_info = NULL;
    task_struct *task    = NULL;
    // Search for the semaphore.
    sem_info = __list_find_sem_info_by_id(semid);
    // The semaphore set doesn't exist.
    if (!sem_info) {
        pr_err("The semaphore set doesn't exist.\n");
        return -EINVAL;
    }
    // Get the calling task.
    task = scheduler_get_current_process();
    assert(task && "Failed to get the current running process.");

    if (cmd == IPC_RMID) {
        // Remove the semaphore set; any processes blocked is awakened (errno set to
        // EIDRM); no argument required.

        if ((sem_info->semid.sem_perm.uid != task->uid) && (sem_info->semid.sem_perm.cuid != task->uid)) {
            pr_err("The calling process is not the creator or the owner of the semaphore set.\n");
            return -EPERM;
        }
        // Remove the set from the list.
        __list_remove_sem_info(sem_info);
        // Delete the set.
        __sem_info_dealloc(sem_info);
    } else if (cmd == SETVAL) {
        // The value of the semnum-th semaphore in the set is initialized to the
        // value specified in arg.val.

        // Check if the index is valid.
        if ((semnum < 0) || (semnum >= (sem_info->semid.sem_nsems))) {
            pr_err("Semaphore number out of bound (%d not in [%d, %d])\n", semnum, 0, sem_info->semid.sem_nsems);
            return -EINVAL;
        }
        // Check if the argument is a null pointer.
        if (!arg) {
            pr_err("The argument is NULL.\n");
            return -EINVAL;
        }
        // Checking if the value is valid.
        if (arg->val < 0) {
            pr_err("The value to set is not valid %d.\n", arg->val);
            return -EINVAL;
        }
        // Check permissions.
        if (!ipc_valid_permissions(O_WRONLY, &sem_info->semid.sem_perm)) {
            pr_err("The calling process does not have read permission to access the set.\n");
            return -EACCES;
        }
        // Setting the value.
        sem_info->sem_base[semnum].sem_val = arg->val;
        // Update the last change time.
        sem_info->semid.sem_ctime = sys_time(NULL);
    } else if (cmd == SETALL) {
        // Initialize all semaphore in the set referred to by semid, using the
        // values supplied in the array pointed to by arg.array.

        // Check if the argument is a null pointer.
        if (!arg) {
            pr_err("The argument is NULL.\n");
            return -EINVAL;
        }
        // Check if the array is valid.
        if (!arg->array) {
            pr_err("The array is NULL.\n");
            return -EINVAL;
        }
        // Check permissions.
        if (!ipc_valid_permissions(O_WRONLY, &sem_info->semid.sem_perm)) {
            pr_err("The calling process does not have read permission to access the set.\n");
            return -EACCES;
        }
        // Setting the values.
        for (unsigned i = 0; i < sem_info->semid.sem_nsems; ++i) {
            sem_info->sem_base[i].sem_val = arg->array[i];
        }
        // Update the last change time.
        sem_info->semid.sem_ctime = sys_time(NULL);
    } else if (cmd == IPC_STAT) {
        // Place a copy of the semid_ds data structure in the buffer pointed to by
        // arg.buf.

        // Check if the argument is a null pointer.
        if (!arg) {
            pr_err("The argument is NULL.\n");
            return -EINVAL;
        }
        // Check if the buffer is a null pointer.
        if (!arg->buf) {
            pr_err("The buffer is NULL.\n");
            return -EINVAL;
        }
        // Check permissions.
        if (!ipc_valid_permissions(O_RDONLY, &sem_info->semid.sem_perm)) {
            pr_err("The calling process does not have read permission to access the set.\n");
            return -EACCES;
        }
        // Copying all the data.
        memcpy(arg->buf, &sem_info->semid, sizeof(struct semid_ds));
    } else if (cmd == GETALL) {
        // Retrieve the values of all of the semaphores in the set referred to by
        // semid, placing them in the array pointed to by arg.array.

        // Check if the argument is a null pointer.
        if (!arg) {
            pr_err("The argument is NULL.\n");
            return -EINVAL;
        }
        // Check if the array is valid.
        if (!arg->array) {
            pr_err("The array is NULL.\n");
            return -EINVAL;
        }
        for (unsigned i = 0; i < sem_info->semid.sem_nsems; ++i) {
            arg->array[i] = sem_info->sem_base[i].sem_val;
        }
    } else if (cmd == GETVAL) {
        // Returns the value of the semnum-th semaphore in the set specified by
        // semid; no argument required.

        // Check if the index is valid.
        if ((semnum < 0) || (semnum >= (sem_info->semid.sem_nsems))) {
            pr_err("Semaphore number out of bound (%d not in [%d, %d])\n", semnum, 0, sem_info->semid.sem_nsems);
            return -EINVAL;
        }
        // Check permissions.
        if (!ipc_valid_permissions(O_RDONLY, &sem_info->semid.sem_perm)) {
            pr_err("The calling process does not have read permission to access the set.\n");
            return -EACCES;
        }
        return sem_info->sem_base[semnum].sem_val;
    } else if (cmd == GETPID) {
        // Return the process ID of the last process to perform a semop on the
        // semnum-th semaphore.

        // Check if the index is valid.
        if ((semnum < 0) || (semnum >= (sem_info->semid.sem_nsems))) {
            pr_err("Semaphore number out of bound (%d not in [%d, %d])\n", semnum, 0, sem_info->semid.sem_nsems);
            return -EINVAL;
        }
        // Check permissions.
        if (!ipc_valid_permissions(O_RDONLY, &sem_info->semid.sem_perm)) {
            pr_err("The calling process does not have read permission to access the set.\n");
            return -EACCES;
        }
        return sem_info->sem_base[semnum].sem_pid;
    } else if (cmd == GETNCNT) {
        // Return the number of processes currently waiting for the resources to
        // become available.

        // Check if the index is valid.
        if ((semnum < 0) || (semnum >= (sem_info->semid.sem_nsems))) {
            pr_err("Semaphore number out of bound (%d not in [%d, %d])\n", semnum, 0, sem_info->semid.sem_nsems);
            return -EINVAL;
        }
        // Check permissions.
        if (!ipc_valid_permissions(O_RDONLY, &sem_info->semid.sem_perm)) {
            pr_err("The calling process does not have read permission to access the set.\n");
            return -EACCES;
        }
        return sem_info->sem_base[semnum].sem_ncnt;
    } else if (cmd == GETZCNT) {
        // Return the number of processes currently waiting for the value of the
        // semnum-th semaphore to become 0.

        // Check if the index is valid.
        if ((semnum < 0) || (semnum >= (sem_info->semid.sem_nsems))) {
            pr_err("Semaphore number out of bound (%d not in [%d, %d])\n", semnum, 0, sem_info->semid.sem_nsems);
            return -EINVAL;
        }
        // Check permissions.
        if (!ipc_valid_permissions(O_RDONLY, &sem_info->semid.sem_perm)) {
            pr_err("The calling process does not have read permission to access the set.\n");
            return -EACCES;
        }
        return sem_info->sem_base[semnum].sem_zcnt;
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

// ============================================================================
// PROCFS FUNCTIONS
// ============================================================================

ssize_t procipc_sem_read(vfs_file_t *file, char *buf, off_t offset, size_t nbyte)
{
    if (!file) {
        pr_err("Received a NULL file.\n");
        return -ENOENT;
    }
    size_t buffer_len = 0, read_pos = 0, ret = 0;
    ssize_t write_count  = 0;
    sem_info_t *sem_info = NULL;
    char buffer[BUFSIZ];

    // Prepare a buffer.
    memset(buffer, 0, BUFSIZ);
    // Prepare the header.
    ret = sprintf(buffer, "key      semid perms      nsems   uid   gid  cuid  cgid      otime      ctime\n");

    // Iterate through the list of semaphore set.
    list_for_each_decl(it, &semaphores_list)
    {
        // Get the current entry.
        sem_info = list_entry(it, sem_info_t, list);
        // Add the line.
        ret += sprintf(
            buffer + ret, "%8d %5d %10d %7d %5d %4d %5d %9d %10d %d\n",
            abs(sem_info->semid.sem_perm.key),
            sem_info->id,
            sem_info->semid.sem_perm.mode,
            sem_info->semid.sem_nsems,
            sem_info->semid.sem_perm.uid,
            sem_info->semid.sem_perm.gid,
            sem_info->semid.sem_perm.cuid,
            sem_info->semid.sem_perm.cgid,
            sem_info->semid.sem_otime,
            sem_info->semid.sem_ctime);
    }
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
