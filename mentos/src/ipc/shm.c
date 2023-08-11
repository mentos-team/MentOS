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
#include "mem/kheap.h"
#include "stdio.h"
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
    void *shm_location;
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
    // Return the shared memory structure.
    return shm_info;
}

/// @brief Frees the memory of a shared memory structure.
/// @param shm_info pointer to the shared memory structure.
static inline void __shm_info_dealloc(shm_info_t *shm_info)
{
    assert(shm_info && "Received a NULL pointer.");
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

void *sys_shmat(int shmid, const void *shmaddr, int shmflg)
{
    return 0;
}

long sys_shmget(key_t key, size_t size, int flag)
{
    return 0;
}

long sys_shmdt(const void *shmaddr)
{
    return 0;
}

long sys_shmctl(int shmid, int cmd, struct shmid_ds *buf)
{
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

    // Iterate through the list of shmaphore set.
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
