/// @file shm.c
/// @brief
/// @copyright (c) 2014-2023 This file is distributed under the MIT License.
/// See LICENSE.md for details.
///! @cond Doxygen_Suppress

// ============================================================================
// Setup the logging for this file (do this before any other include).
#include "sys/kernel_levels.h"           // Include kernel log levels.
#define __DEBUG_HEADER__ "[IPCshm]"      ///< Change header.
#define __DEBUG_LEVEL__  LOGLEVEL_NOTICE ///< Set log level.
#include "io/debug.h"                    // Include debugging functions.
// ============================================================================

#include "ipc/ipc.h"
#include "sys/shm.h"

#include "assert.h"
#include "fcntl.h"
#include "mem/kheap.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "sys/errno.h"
#include "sys/list_head.h"

// #include "process/process.h"

///@brief A value to compute the shmid value.
int __shm_id = 0;

// @brief Shared memory management structure.
typedef struct {
    /// @brief ID associated to the shared memory.
    int id;
    /// @brief The shared memory data strcutre.
    struct shmid_ds shmid;
    /// Where shm created is memorized.
    page_t *shm_location;
    /// Reference inside the list of shared memory management structures.
    list_head list;
} shm_info_t;

/// @brief List of all current active shared memorys.
list_head shm_list;

// ============================================================================
// MEMORY MANAGEMENT (Private)
// ============================================================================

/// @brief Allocates the memory for shared memory structure.
/// @param key IPC_KEY associated with the shared memory.
/// @param shmflg flags used to create the shared memory.
/// @return a pointer to the allocated shared memory structure.
static inline shm_info_t *__shm_info_alloc(key_t key, size_t size, int shmflg)
{
    // Allocate the memory.
    shm_info_t *shm_info = (shm_info_t *)kmalloc(sizeof(shm_info_t));
    // Check the allocated memory.
    assert(shm_info && "Failed to allocate memory for a shared memory structure.");
    // Clean the memory.
    memset(shm_info, 0, sizeof(shm_info_t));
    // Initialize its values.
    shm_info->id               = ++__shm_id;
    shm_info->shmid.shm_perm   = register_ipc(key, shmflg & 0x1FF);
    shm_info->shmid.shm_segsz  = size;
    shm_info->shmid.shm_atime  = 0;
    shm_info->shmid.shm_dtime  = 0;
    shm_info->shmid.shm_ctime  = 0;
    shm_info->shmid.shm_cpid   = 0;
    shm_info->shmid.shm_lpid   = 0;
    shm_info->shmid.shm_nattch = 0;
    // Allocate the memory.
    uint32_t order         = find_nearest_order_greater(0, size);
    shm_info->shm_location = _alloc_pages(GFP_KERNEL, order);
    // Return the shared memory structure.
    return shm_info;
}

/// @brief Frees the memory of a shared memory structure.
/// @param shm_info pointer to the shared memory structure.
static inline void __shm_info_dealloc(shm_info_t *shm_info)
{
    assert(shm_info && "Received a NULL pointer.");
    // Free the shared memory.
    __free_pages(shm_info->shm_location);
    // Deallocate the shmid memory.
    kfree(shm_info);
}

// ============================================================================
// LIST MANAGEMENT/SEARCH FUNCTIONS (Private)
// ============================================================================

/// @brief Searches for the shared memory with the given id.
/// @param shmid the id we are searching.
/// @return the shared memory with the given id.
static inline shm_info_t *__list_find_shm_info_by_id(int shmid)
{
    shm_info_t *shm_info;
    // Iterate through the list of shared memories.
    list_for_each_decl(it, &shm_list)
    {
        // Get the current entry.
        shm_info = list_entry(it, shm_info_t, list);
        // If shared memories is valid, check the id.
        if (shm_info && (shm_info->id == shmid)) {
            return shm_info;
        }
    }
    return NULL;
}

/// @brief Searches for the shared memory with the given key.
/// @param key the key we are searching.
/// @return the shared memory with the given key.
static inline shm_info_t *__list_find_shm_info_by_key(key_t key)
{
    shm_info_t *shm_info;
    // Iterate through the list of shared memories.
    list_for_each_decl(it, &shm_list)
    {
        // Get the current entry.
        shm_info = list_entry(it, shm_info_t, list);
        // If shared memories is valid, check the id.
        if (shm_info && (shm_info->shmid.shm_perm.key == key)) {
            return shm_info;
        }
    }
    return NULL;
}

static inline void __list_add_shm_info(shm_info_t *shm_info)
{
    assert(shm_info && "Received a NULL pointer.");
    // Add the new item at the end.
    list_head_insert_before(&shm_info->list, &shm_list);
}

static inline void __list_remove_shm_info(shm_info_t *shm_info)
{
    assert(shm_info && "Received a NULL pointer.");
    // Delete the item from the list.
    list_head_remove(&shm_info->list);
}

// ============================================================================
// SYSTEM FUNCTIONS
// ============================================================================

int shm_init()
{
    list_head_init(&shm_list);
    return 0;
}

long sys_shmget(key_t key, size_t size, int shmflg)
{
    shm_info_t *shm_info = NULL;
    // Need to find a unique key.
    if (key == IPC_PRIVATE) {
        // Exit when i find a unique key.
        do {
            key = (int)-rand();
        } while (__list_find_shm_info_by_key(key));
        // We have a unique key, create the shared memory.
        shm_info = __shm_info_alloc(key, size, shmflg);
        // Add the shared memory to the list.
        __list_add_shm_info(shm_info);
    } else {
        // Get the shared memory if it exists.
        shm_info = __list_find_shm_info_by_key(key);
        // Check if no shared memory exists for the given key and the flags did not specify IPC_CREAT.
        if (!shm_info && !(shmflg & IPC_CREAT)) {
            pr_err("No shared memory exists for the given key and the flags did not specify IPC_CREAT.\n");
            return -ENOENT;
        }
        // Check if IPC_CREAT and IPC_EXCL were specified, but a shared memory already exists for key.
        if (shm_info && (shmflg & IPC_CREAT) && (shmflg & IPC_EXCL)) {
            pr_err("IPC_CREAT and IPC_EXCL were specified, but a shared memory already exists for key.\n");
            return -EEXIST;
        }
        // Check if the shared memory exists for the given key, but the calling
        // process does not have permission to access the set.
        if (shm_info && !ipc_valid_permissions(shmflg, &shm_info->shmid.shm_perm)) {
            pr_err("The shared memory exists for the given key, but the calling process does not have permission to access the set.\n");
            return -EACCES;
        }
        // If the shared memory does not exist we need to create a new one.
        if (shm_info == NULL) {
            // Create the shared memory.
            shm_info = __shm_info_alloc(key, size, shmflg);
            // Add the shared memory to the list.
            __list_add_shm_info(shm_info);
        }
    }
    // Return the id of the shared memory.
    return shm_info->id;
}

static inline int __find_vm_free_area(mm_struct_t *mm, size_t length, uintptr_t *vm_start)
{
    // Get the stack.
    vm_area_struct_t *area, *prev_area;
    list_for_each_prev_decl(it, &mm->mmap_list)
    {
        area = list_entry(it, vm_area_struct_t, vm_list);
        assert(area && "There is a NULL area in the list.");
        // Check the previous segment.
        if (area->vm_list.prev != &mm->mmap_list) {
            prev_area = list_entry(area->vm_list.prev, vm_area_struct_t, vm_list);
            assert(prev_area && "There is a NULL area in the list.");
            // Compute the available space.
            unsigned available_space = area->vm_start - prev_area->vm_end;
            // If the space is enough, return the address.
            if (available_space >= length) {
                *vm_start = area->vm_start - length;
                return 0;
            }
        }
    }
    return 1;
}

void *sys_shmat(int shmid, const void *shmaddr, int shmflg)
{
    shm_info_t *shm_info = NULL;
    task_struct *task    = NULL;
    // The id is less than zero.
    if (shmid < 0) {
        pr_err("The id is less than zero.\n");
        return (void *)-EINVAL;
    }
    // Get the shared memory if it exists.
    shm_info = __list_find_shm_info_by_id(shmid);
    // Check if no shared memory exists for the given key and the flags did not specify IPC_CREAT.
    if (!shm_info) {
        pr_err("No shared memory exists for the given id and the flags did not specify IPC_CREAT.\n");
        return (void *)-ENOENT;
    }
    // Check if the shared memory exists for the given key, but the calling
    // process does not have permission to access the set.
    if ((shmflg & SHM_RDONLY) && !ipc_valid_permissions(O_RDONLY, &shm_info->shmid.shm_perm)) {
        pr_err("The shared memory exists for the given key, but the calling process does not have permission to access the set.\n");
        return (void *)-EACCES;
    }
    if (!ipc_valid_permissions(O_RDWR, &shm_info->shmid.shm_perm)) {
        pr_err("The shared memory exists for the given key, but the calling process does not have permission to access the set.\n");
        return (void *)-EACCES;
    }
    // Get the calling task.
    task = scheduler_get_current_process();
    assert(task && "Failed to get the current running process.");

    uint32_t phy_start = get_physical_address_from_page(shm_info->shm_location);
    uint32_t vm_start;
    __find_vm_free_area(
        task->mm,
        shm_info->shmid.shm_segsz,
        &vm_start);

    mem_upd_vm_area(
        task->mm->pgd,
        vm_start,
        phy_start,
        shm_info->shmid.shm_segsz,
        MM_RW | MM_PRESENT | MM_USER | MM_UPDADDR);

    return (void *)vm_start;
}

long sys_shmdt(const void *shmaddr)
{
    return 0;
}

long sys_shmctl(int shmid, int cmd, struct shmid_ds *buf)
{
    shm_info_t *shm_info = NULL;
    task_struct *task    = NULL;

    // Search for the shared memory.
    shm_info = __list_find_shm_info_by_id(shmid);
    // The shared memory doesn't exist.
    if (!shm_info) {
        pr_err("The shared memory doesn't exist.\n");
        return -EINVAL;
    }

    // Get the calling task.
    task = scheduler_get_current_process();
    assert(task && "Failed to get the current running process.");

    if (cmd == IPC_RMID) {
        // Remove the shared memory; any processes blocked is awakened (errno set to EIDRM); no argument required.
        if ((shm_info->shmid.shm_perm.uid != task->uid) && (shm_info->shmid.shm_perm.cuid != task->uid)) {
            pr_err("The calling process is not the creator or the owner of the shared memory.\n");
            return -EPERM;
        }
        // Remove the set from the list.
        __list_remove_shm_info(shm_info);
        // Delete the set.
        __shm_info_dealloc(shm_info);
    }
    return 0;
}

// ============================================================================
// PROCFS FUNCTIONS
// ============================================================================

ssize_t procipc_shm_read(vfs_file_t *file, char *buf, off_t offset, size_t nbyte)
{
    if (!file) {
        pr_err("Received a NULL file.\n");
        return -ENOENT;
    }
    size_t buffer_len = 0, read_pos = 0, ret = 0;
    ssize_t write_count  = 0;
    shm_info_t *shm_info = NULL;
    char buffer[BUFSIZ];

    // Prepare a buffer.
    memset(buffer, 0, BUFSIZ);
    // Prepare the header.
    ret = sprintf(buffer, "key      shmid perms      segsz   uid   gid  cuid  cgid      atime      dtime      ctime   cpid   lpid nattch\n");

    // Iterate through the list of shared memory.
    list_for_each_decl(it, &shm_list)
    {
        // Get the current entry.
        shm_info = list_entry(it, shm_info_t, list);
        // Add the line.
        ret += sprintf(
            buffer + ret, "%8d %5d %10d %7d %5d %4d %5d %9d %10d %10d %10d %5d %5d %5d\n",
            abs(shm_info->shmid.shm_perm.key),
            shm_info->id,
            shm_info->shmid.shm_perm.mode,
            shm_info->shmid.shm_segsz,
            shm_info->shmid.shm_perm.uid,
            shm_info->shmid.shm_perm.gid,
            shm_info->shmid.shm_perm.cuid,
            shm_info->shmid.shm_perm.cgid,
            shm_info->shmid.shm_atime,
            shm_info->shmid.shm_dtime,
            shm_info->shmid.shm_ctime,
            shm_info->shmid.shm_cpid,
            shm_info->shmid.shm_lpid,
            shm_info->shmid.shm_nattch);
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
