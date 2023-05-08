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
#include "sys/kernel_levels.h"          // Include kernel log levels.
#define __DEBUG_HEADER__ "[IPCsem]"     ///< Change header.
#define __DEBUG_LEVEL__  LOGLEVEL_DEBUG ///< Set log level.
#include "io/debug.h"                   // Include debugging functions.
// ============================================================================

#include "sys/sem.h"

#include "process/process.h"
#include "klib/list.h"
#include "sys/errno.h"
#include "string.h"
#include "assert.h"
#include "stdio.h"

///@brief A value to compute the semid value.
int semid_assign = 0;

/// @brief List of all current active semaphores.
list_t semaphores_list = {
    .head = NULL,
    .tail = NULL,
    .size = 0
};

/// @brief [temporary] To implement the IPC_PRIVATE mechanism.
int count_ipc_private = 2;

/// @brief Allocates the memory for an array of semaphores.
/// @param nsems number of semaphores in the array.
/// @return a pointer to the allocated array of semaphores.
static inline struct sem *__sem_alloc(int nsems)
{
    // Allocate the memory.
    struct sem *ptr = (struct sem *)kmalloc(sizeof(struct sem) * nsems);
    // Check the allocated memory.
    assert(ptr && "Failed to allocate memory for the array of semaphores.");
    // Clean the memory.
    memset(ptr, 0, sizeof(struct sem));
    return ptr;
}

/// @brief Frees the memory of an array of semaphores.
/// @param ptr the pointer to the array of semaphores.
static inline void __sem_dealloc(struct sem *ptr)
{
    assert(ptr && "Received a NULL pointer.");
    kfree(ptr);
}

/// @brief Initializes a single semaphore.
/// @param ptr the pointer to the single semaphore.
static inline void __sem_init(struct sem *ptr)
{
    ptr->sem_val  = 0;
    ptr->sem_pid  = sys_getpid();
    ptr->sem_zcnt = 0;
}

/// @brief Allocates the memory for a semid structure.
/// @return a pointer to the allocated semid structure.
static inline struct semid_ds *__semid_alloc()
{
    // Allocate the memory.
    struct semid_ds *ptr = (struct semid_ds *)kmalloc(sizeof(struct semid_ds));
    // Check the allocated memory.
    assert(ptr && "Failed to allocate memory for a semid structure.");
    // Clean the memory.
    memset(ptr, 0, sizeof(struct semid_ds));
    return ptr;
}

/// @brief Frees the memory of a semid structure.
/// @param ptr pointer to the semid structure.
static inline void __semid_dealloc(struct semid_ds *ptr)
{
    assert(ptr && "Received a NULL pointer.");
    kfree(ptr);
}

/// @brief Initializes a semid struct.
/// @param ptr the pointer to the semid struct.
/// @param key IPC_KEY associated with the set of semaphores
/// @param nsems number of semaphores to initialize
/// @todo The way we compute the semid is a temporary solution.
static inline void __semid_init(struct semid_ds *ptr, key_t key, int nsems)
{
    assert(ptr && "Received a NULL pointer.");
    ptr->owner     = sys_getpid();
    ptr->key       = key;
    ptr->semid     = ++semid_assign;
    ptr->sem_otime = 0;
    ptr->sem_ctime = 0;
    ptr->sem_nsems = nsems;
    ptr->sems      = __sem_alloc(nsems);
    for (int i = 0; i < nsems; i++) {
        __sem_init(&ptr->sems[i]);
    }
}

/// @brief Searches for the semaphore with the given id.
/// @param semid the id we are searching.
/// @return the semaphore with the given id.
static inline struct semid_ds *__find_semaphore(int semid)
{
    struct semid_ds *semaphores;
    // Iterate through the list of semaphores.
    listnode_foreach(listnode, &semaphores_list)
    {
        // Get the current list of semaphores.
        semaphores = (struct semid_ds *)listnode->value;
        // If semaphores is valid, check the id.
        if (semaphores && (semaphores->semid == semid))
            return semaphores;
    }
    return NULL;
}

long sys_semget(key_t key, int nsems, int semflg)
{
    struct semid_ds *semaphores = NULL;
    //check if nsems is a valid value
    if (nsems <= 0 && semflg != 0) {
        //pr_err("Errore NSEMS\n"); //debuggin purposes
        errno = EINVAL;
        return -1;
    }
    // Need to find a unique key.
    if (key == IPC_PRIVATE) {
        int flag = 1;
        // Exit when i find a unique key.
        while (1) {
            listnode_foreach(listnode, &semaphores_list)
            {
                // Get the current list of semaphores.
                semaphores = (struct semid_ds *)listnode->value;
                // If semaphores is valid, check the key.
                if (semaphores && (semaphores->key == count_ipc_private))
                    flag = 0;
            }
            if (flag == 1)
                break;
            flag = 1;
            count_ipc_private *= 3; //multiply to try to find a unique key
        }
        //we have the key
        semaphores = __semid_alloc();
        __semid_init(semaphores, count_ipc_private, nsems);
        list_insert_front(&semaphores_list, semaphores);
        return semaphores->semid;
    }

    int flag       = 0;
    int temp_semid = -1;
    listnode_foreach(listnode, &semaphores_list)
    {                                                                 //iterate through the list
        if (((struct semid_ds *)listnode->value)->key == key) {       //if we find a semid with the given key
            temp_semid = ((struct semid_ds *)listnode->value)->semid; //saving the semid
            flag       = 1;                                           //found
        }
    }
    if (flag == 0) {              //unique key
        if (semflg & IPC_CREAT) { //and i want to create a semaphore set with it
            semaphores = __semid_alloc();
            __semid_init(semaphores, key, nsems);
            list_insert_front(&semaphores_list, semaphores);
            return semaphores->semid;
        }
        errno = EINVAL; //invalid argument bc it is a unique key but with no IPC_CREAT
        return -1;
    }
    if (semflg & IPC_EXCL) { //error, the sem set already exist
        errno = EEXIST;
        return -1;
    }
    return temp_semid; //return the correspondent semid

    //TODO("Not implemented");
}

long sys_semop(int semid, struct sembuf *sops, unsigned nsops)
{
    struct semid_ds *temp;
    // Search for the semaphore.
    temp = __find_semaphore(semid);
    // If the semaphore is NULL, stop.
    if (!temp) {
        return -EINVAL;
    }

    if (sops->sem_num < 0 || sops->sem_num >= temp->sem_nsems) { //checking parameters
        return -EINVAL;
    }

    temp->sem_otime = sys_time(NULL);

    if (sops->sem_op < 0) {
        /*If the operation is negative then we need to check for possible blocking operation*/

        /*if the value of the sem were to become negative then we return a special value*/
        if (temp->sems[sops->sem_num].sem_val < (-(sops->sem_op))) {
            return OPERATION_NOT_ALLOWED; //not allowed
        } else {
            /*otherwise we can modify the sem_val and all the other parameters of the semaphore*/
            temp->sems[sops->sem_num].sem_val += (sops->sem_op);
            temp->sems[sops->sem_num].sem_pid = sys_getpid();
            temp->sem_ctime                   = sys_time(NULL);
            return 1; //allowed
        }
    } else {
        /*the operation is non negative so we can always do it*/
        temp->sems[sops->sem_num].sem_val += (sops->sem_op);
        temp->sems[sops->sem_num].sem_pid = sys_getpid();
        temp->sem_ctime                   = sys_time(NULL);
        return 1; //allowed
    }

    return 0;
}

long sys_semctl(int semid, int semnum, int cmd, union semun *arg)
{
    struct semid_ds *temp;
    // Search for the semaphore.
    temp = __find_semaphore(semid);
    // If the semaphore is NULL, stop.
    if (!temp) {
        return -EINVAL;
    }

    switch (cmd) {
    //remove the semaphore set; any processes blocked is awakened (errno set to EIDRM); no argument required.
    case IPC_RMID:
        list_remove_node(&semaphores_list, list_find(&semaphores_list, temp));

        //pr_debug("\ndone\n");
        /*gonna need to unblock all the processes on the semaphore*/

        break;

    //place a copy of the semid_ds data structure in the buffer pointed to by arg.buf.
    case IPC_STAT:
        if (arg->buf == NULL || arg->buf->sems == NULL) { /*checking the parameters*/
            return -EINVAL;
        }

        //copying all the data
        arg->buf->key       = temp->key;
        arg->buf->owner     = temp->owner;
        arg->buf->semid     = temp->semid;
        arg->buf->sem_otime = temp->sem_otime;
        arg->buf->sem_ctime = temp->sem_ctime;
        arg->buf->sem_nsems = temp->sem_nsems;
        for (int i = 0; i < temp->sem_nsems; i++) {
            arg->buf->sems[i].sem_val  = temp->sems[i].sem_val;
            arg->buf->sems[i].sem_pid  = temp->sems[i].sem_pid;
            arg->buf->sems[i].sem_zcnt = temp->sems[i].sem_zcnt;
        }

        return 0;

        //update selected fields of the semid_ds using values in the buffer pointed to by arg.buf.
        //case IPC_SET:
        /* code */
        //break;

    //the value of the semnum-th semaphore in the set is initialized to the value specified in arg.val.
    case SETVAL:
        if (semnum < 0 || semnum >= (temp->sem_nsems)) { //if the index is valid
            return -EINVAL;
        }

        if (arg->val < 0) { //checking if the value is valid
            return -EINVAL;
        }
        //setting the values
        temp->sem_ctime            = sys_time(NULL);
        temp->sems[semnum].sem_val = arg->val;
        return 0;

    //returns the value of the semnum-th semaphore in the set specified by semid; no argument required.
    case GETVAL:
        if (semnum < 0 || semnum >= (temp->sem_nsems)) { //if the index is valid
            return -EINVAL;
        }

        return temp->sems[semnum].sem_val;

    //initialize all semaphore in the set referred to by semid, using the values supplied in the array pointed to by arg.array.
    case SETALL:
        if (arg->array == NULL) { /*checking parameters*/
            return -EINVAL;
        }
        for (int i = 0; i < temp->sem_nsems; i++) { //setting all the values
            temp->sems[i].sem_val = arg->array[i];
        }
        temp->sem_ctime = sys_time(NULL);
        return 0;

    //retrieve the values of all of the semaphores in the set referred to by semid, placing them in the array pointed to by arg.array.
    case GETALL:
        if (arg->array == NULL) { //checking if the argument passed is valid
            return -EINVAL;
        }
        for (int i = 0; i < temp->sem_nsems; i++) {
            arg->array[i] = temp->sems[i].sem_val;
        }
        return 0;

    //return the process ID of the last process to perform a semop on the semnum-th semaphore.
    case GETPID:
        if (semnum < 0 || semnum >= (temp->sem_nsems)) { //if the index is valid
            return -EINVAL;
        }

        return temp->sems[semnum].sem_pid;

    //return the number of processes currently waiting for the value of the semnum-th semaphore to become 0.
    case GETZCNT:
        if (semnum < 0 || semnum >= (temp->sem_nsems)) { //if the index is valid
            return -EINVAL;
        }

        return temp->sems[semnum].sem_zcnt;

    //return the number of semaphores in the set.
    case GETNSEMS:
        return temp->sem_nsems;

    //not a valid argument.
    default:
        return -EINVAL;
    }

    return 0;
}

ssize_t procipc_sem_read(vfs_file_t *file, char *buf, off_t offset, size_t nbyte)
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
    ret = sprintf(buffer, "key      semid perms      nsems   uid   gid  cuid  cgid      otime      ctime\n");

    // Iterate through the list.
    if (semaphores_list.size > 0) {
        listnode_foreach(listnode, &semaphores_list)
        {
            entry = ((struct semid_ds *)listnode->value);
            ret += sprintf(buffer + ret, "%8d %5d %10d %7d %5d %4d %5d %9d %10d %d\n",
                           entry->key, entry->semid, 0, entry->sem_nsems, entry->owner, 0, 0, 0, entry->sem_otime, entry->sem_ctime);
        }
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

///! @endcond
