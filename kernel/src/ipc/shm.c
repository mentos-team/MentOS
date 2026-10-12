/// @file shm.c
/// @brief
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

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
#include "errno.h"
#include "fcntl.h"
#include "list_head.h"
#include "klib/stdatomic.h"
#include "klib/spinlock.h"
#include "mem/mm/vm_area.h"
#include "mem/uaccess.h"
#include "process/scheduler.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"

// #include "process/process.h"

///@brief A value to compute the shmid value.
int __shm_id = 0;

/// @brief Shared memory management structure.
typedef struct {
    /// @brief Shared memory ID.
    int id;
    /// @brief Shared memory data structure.
    struct shmid_ds shmid;
    /// @brief Location where shared memory is stored.
    page_t *shm_location;
    /// Serializes lifecycle metadata and attachment accounting.
    spinlock_t lock;
    /// Set after IPC_RMID; existing mappings may continue until detach.
    int removed;
    /// Namespace, syscall, and VMA references protecting this object.
    atomic_t references;
    /// @brief List reference for shared memory structures.
    list_head_t list;
} shm_info_t;

/// @brief List of all current active shared memorys.
list_head_t shm_list;
static spinlock_t shm_list_lock;

static inline shm_info_t *__list_find_shm_info_by_id(int shmid);

// ============================================================================
// MEMORY MANAGEMENT (Private)
// ============================================================================

/// @brief Allocates memory for a shared memory structure.
/// @param key IPC key associated with the shared memory.
/// @param size Size of the shared memory segment.
/// @param shmflg Flags used to create the shared memory.
/// @return Pointer to the allocated shared memory structure, or NULL on failure.
static inline shm_info_t *__shm_info_alloc(key_t key, size_t size, int shmflg)
{
    // Allocate memory for shm_info_t.
    shm_info_t *shm_info = (shm_info_t *)kmalloc(sizeof(shm_info_t));
    // Check if memory allocation for shm_info failed.
    if (!shm_info) {
        pr_err("Failed to allocate memory for shared memory structure.\n");
        return NULL;
    }
    // Initialize the allocated memory to zero.
    memset(shm_info, 0, sizeof(shm_info_t));
    spinlock_init(&shm_info->lock);
    atomic_set(&shm_info->references, 1); // namespace ownership
    // Initialize shm_info values.
    shm_info->id               = ++__shm_id;
    shm_info->shmid.shm_perm   = register_ipc(key, shmflg & 0x1FF);
    shm_info->shmid.shm_segsz  = size;
    shm_info->shmid.shm_atime  = 0;
    shm_info->shmid.shm_dtime  = 0;
    shm_info->shmid.shm_ctime  = 0;
    shm_info->shmid.shm_cpid   = 0;
    shm_info->shmid.shm_lpid   = 0;
    shm_info->shmid.shm_nattch = 0;
    // Determine the order for memory allocation.
    uint32_t order             = find_nearest_order_greater(0, size);
    // Allocate the shared memory pages.
    // Shared mappings are accessed by both the kernel and user processes.  The
    // current 32-bit VM subsystem has no temporary kmap API yet, therefore
    // these pages must come from the directly mapped normal zone rather than
    // HIGHMEM.
    shm_info->shm_location     = alloc_pages(GFP_KERNEL, order);
    // Check if memory allocation for shm_location failed.
    if (!shm_info->shm_location) {
        pr_err("Failed to allocate shared memory pages.\n");
        kfree(shm_info);
        return NULL;
    }
    memset((void *)get_virtual_address_from_page(shm_info->shm_location), 0, (size_t)1 << (order + PAGE_SHIFT));
    // Return the allocated shared memory structure.
    return shm_info;
}

/// @brief Frees the memory of a shared memory structure.
/// @param shm_info pointer to the shared memory structure.
static inline void __shm_info_dealloc(shm_info_t *shm_info)
{
    assert(shm_info && "Received a NULL pointer.");
    // Free the shared memory.
    free_pages(shm_info->shm_location);
    // Deallocate the shmid memory.
    kfree(shm_info);
}

static inline shm_info_t *__list_find_shm_info_by_key(key_t key);

/// @brief Acquire a reference to a segment still present in the namespace.
static shm_info_t *__shm_get_ref(int shmid)
{
    shm_info_t *shm_info;
    spinlock_lock(&shm_list_lock);
    shm_info = __list_find_shm_info_by_id(shmid);
    if (shm_info && !shm_info->removed) {
        atomic_inc(&shm_info->references);
    } else {
        shm_info = NULL;
    }
    spinlock_unlock(&shm_list_lock);
    return shm_info;
}

/// @brief Acquire a reference to a segment found by key.
static shm_info_t *__shm_get_ref_by_key(key_t key)
{
    shm_info_t *shm_info;
    spinlock_lock(&shm_list_lock);
    shm_info = __list_find_shm_info_by_key(key);
    if (shm_info && !shm_info->removed) {
        atomic_inc(&shm_info->references);
    } else {
        shm_info = NULL;
    }
    spinlock_unlock(&shm_list_lock);
    return shm_info;
}

/// @brief Release a reference and reclaim a removed segment when unused.
static void __shm_put_ref(shm_info_t *shm_info)
{
    if (atomic_dec_and_test(&shm_info->references) && shm_info->removed) {
        __shm_info_dealloc(shm_info);
    }
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
    list_for_each_decl (it, &shm_list) {
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
    list_for_each_decl (it, &shm_list) {
        // Get the current entry.
        shm_info = list_entry(it, shm_info_t, list);
        // If shared memories is valid, check the id.
        if (shm_info && (shm_info->shmid.shm_perm.key == key)) {
            return shm_info;
        }
    }
    return NULL;
}

/// @brief Adds a shared memory info structure to the list.
/// @param shm_info Pointer to the shared memory info structure.
static inline void __list_add_shm_info(shm_info_t *shm_info)
{
    // Check if shm_info is NULL.
    assert(shm_info && "Received a NULL pointer.");
    // Add the new item at the end.
    list_head_insert_before(&shm_info->list, &shm_list);
}

/// @brief Removes a shared memory info structure from the list.
/// @param shm_info Pointer to the shared memory info structure.
static inline void __list_remove_shm_info(shm_info_t *shm_info)
{
    // Check if shm_info is NULL.
    assert(shm_info && "Received a NULL pointer.");
    // Delete the item from the list.
    list_head_remove(&shm_info->list);
}

// ============================================================================
// SYSTEM FUNCTIONS
// ============================================================================

int shm_init(void)
{
    list_head_init(&shm_list);
    spinlock_init(&shm_list_lock);
    return 0;
}

/// @brief Account for a new address-space mapping of a segment.
static void __shm_mapping_acquire(vm_area_struct_t *area)
{
    shm_info_t *shm_info = (shm_info_t *)area->vm_private_data;
    atomic_inc(&shm_info->references);
    spinlock_lock(&shm_info->lock);
    shm_info->shmid.shm_nattch++;
    shm_info->shmid.shm_lpid = sys_getpid();
    shm_info->shmid.shm_atime = sys_time(NULL);
    spinlock_unlock(&shm_info->lock);
}

/// @brief Account for a detached or exited address-space mapping.
static void __shm_mapping_release(vm_area_struct_t *area)
{
    shm_info_t *shm_info = (shm_info_t *)area->vm_private_data;
    spinlock_lock(&shm_info->lock);
    if (shm_info->shmid.shm_nattch > 0) {
        shm_info->shmid.shm_nattch--;
    }
    shm_info->shmid.shm_lpid = sys_getpid();
    shm_info->shmid.shm_dtime = sys_time(NULL);
    spinlock_unlock(&shm_info->lock);
    __shm_put_ref(shm_info);
}

long sys_shmget(key_t key, size_t size, int shmflg)
{
    shm_info_t *shm_info = NULL;
    const int private_key = (key == IPC_PRIVATE);
    if (size == 0) {
        return -EINVAL;
    }
    if (!private_key) {
        shm_info = __shm_get_ref_by_key(key);
        if (shm_info && size > shm_info->shmid.shm_segsz) {
            __shm_put_ref(shm_info);
            return -EINVAL;
        }
        if (!shm_info && !(shmflg & IPC_CREAT)) {
            pr_err("No shared memory exists for the given key and the flags "
                   "did not specify IPC_CREAT.\n");
            return -ENOENT;
        }
        if (shm_info && (shmflg & IPC_CREAT) && (shmflg & IPC_EXCL)) {
            pr_err("IPC_CREAT and IPC_EXCL were specified, but a shared memory "
                   "already exists for key.\n");
            __shm_put_ref(shm_info);
            return -EEXIST;
        }
        if (shm_info) {
            spinlock_lock(&shm_info->lock);
            int permitted = ipc_valid_mode((mode_t)shmflg, &shm_info->shmid.shm_perm);
            int id = shm_info->id;
            spinlock_unlock(&shm_info->lock);
            __shm_put_ref(shm_info);
            if (!permitted) {
                pr_err("The shared memory exists for the given key, but the calling "
                       "process does not have permission to access the set.\n");
                return -EACCES;
            }
            return id;
        }
    }

    if (private_key) {
        do {
            key = -rand();
            shm_info = __shm_get_ref_by_key(key);
            if (shm_info) {
                __shm_put_ref(shm_info);
            }
        } while (shm_info);
    }
    shm_info_t *candidate = __shm_info_alloc(key, size, shmflg);
    if (!candidate) {
        return -ENOENT;
    }
    spinlock_lock(&shm_list_lock);
    shm_info = __list_find_shm_info_by_key(key);
    if (shm_info && !shm_info->removed) {
        atomic_inc(&shm_info->references);
        spinlock_unlock(&shm_list_lock);
        __shm_info_dealloc(candidate);
        if (private_key) {
            __shm_put_ref(shm_info);
            return sys_shmget(IPC_PRIVATE, size, shmflg);
        }
        if (size > shm_info->shmid.shm_segsz || (shmflg & IPC_EXCL)) {
            __shm_put_ref(shm_info);
            return (size > shm_info->shmid.shm_segsz) ? -EINVAL : -EEXIST;
        }
        spinlock_lock(&shm_info->lock);
        int permitted = ipc_valid_mode((mode_t)shmflg, &shm_info->shmid.shm_perm);
        int id = shm_info->id;
        spinlock_unlock(&shm_info->lock);
        __shm_put_ref(shm_info);
        return permitted ? id : -EACCES;
    }
    __list_add_shm_info(candidate);
    spinlock_unlock(&shm_list_lock);
    return candidate->id;
}

void *sys_shmat(int shmid, const void *shmaddr, int shmflg)
{
    shm_info_t *shm_info = NULL;
    task_struct *task    = NULL;
    uint32_t vm_start;
    uint32_t phy_start;
    size_t mapping_size;
    uint32_t flags = MM_RW | MM_PRESENT | MM_USER;


    // `shmaddr` is an address argument, not a buffer: it must merely stay
    // out of the kernel area, and a page-table walk would be wrong here
    // because nothing requires the address to be mapped yet (#191).
    if (shmaddr && ((uintptr_t)shmaddr >= PROCAREA_END_ADDR)) {
        return (void *)-EFAULT;
    }

    // The id is less than zero.
    if (shmid < 0) {
        pr_err("The id is less than zero.\n");
        return (void *)-EINVAL;
    }
    // Get the shared memory if it exists.
    shm_info = __shm_get_ref(shmid);
    // Check if no shared memory exists for the given key and the flags did not specify IPC_CREAT.
    if (!shm_info) {
        pr_err("No shared memory exists for the given id and the flags did not "
               "specify IPC_CREAT.\n");
        return (void *)-ENOENT;
    }
    // Check if the shared memory exists for the given key, but the calling
    // process does not have permission to access the set.
    if (shmflg & SHM_RDONLY) {
        if (!ipc_valid_permissions(IPC_ACCESS_READ, &shm_info->shmid.shm_perm)) {
            pr_err("The shared memory exists for the given key, but the calling "
                   "process does not have permission to access the set.\n");
            __shm_put_ref(shm_info);
            return (void *)-EACCES;
        }
        // Remove the read-write flag.
        flags = bitmask_clear(flags, MM_RW);
    } else if (!ipc_valid_permissions(IPC_ACCESS_READ | IPC_ACCESS_WRITE, &shm_info->shmid.shm_perm)) {
        pr_err("The shared memory exists for the given key, but the calling "
               "process does not have permission to access the set.\n");
        __shm_put_ref(shm_info);
        return (void *)-EACCES;
    }
    // Get the calling task.
    task = scheduler_get_current_process();
    assert(task && "Failed to get the current running process.");
    // A VMA and its physical backing are page-granular.  Rounding the
    // requested segment up is also required when the allocator places the
    // mapping at the end of a page: the final user byte must not cross into an
    // unmapped page merely because shm_segsz is not page aligned.
    mapping_size = (shm_info->shmid.shm_segsz + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    // Get the phyisical address from the allocated pages.
    phy_start = get_physical_address_from_page(shm_info->shm_location);
    // Find the virtual address for the new area.
    if (vm_area_search_free_area(task->mm, mapping_size, &vm_start)) {
        pr_err("shmat address search failed: size=%zu\n", mapping_size);
        __shm_put_ref(shm_info);
        return (void *)-EACCES;
    }
    vm_area_struct_t *area = vm_area_map_physical(
        task->mm, vm_start, mapping_size, phy_start, flags, shm_info,
        __shm_mapping_release, __shm_mapping_acquire);
    if (!area) {
        __shm_put_ref(shm_info);
        return (void *)-ENOMEM;
    }
    __shm_mapping_acquire(area);
    __shm_put_ref(shm_info); // drop the syscall reference
    return (void *)vm_start;
}

long sys_shmdt(const void *shmaddr)
{
    task_struct *task    = NULL;

    // The address must name user memory before anything walks the page
    // tables for it: without this, a kernel-area address would walk the
    // kernel half of the caller's directory (#191).
    if (!shmaddr || ((uintptr_t)shmaddr >= PROCAREA_END_ADDR)) {
        return -EFAULT;
    }

    // Get the calling task.
    task = scheduler_get_current_process();
    assert(task && "Failed to get the current running process.");
    vm_area_struct_t *area = vm_area_find(task->mm, (uint32_t)shmaddr);
    if (!area || !area->vm_release) {
        return -ENOENT;
    }
    return vm_area_destroy(task->mm, area);
}

long sys_shmctl(int shmid, int cmd, struct shmid_ds *buf)
{
    shm_info_t *shm_info = NULL;
    task_struct *task    = NULL;
    // No implemented command reads or writes `buf` yet (only IPC_RMID is
    // handled); the day one does, it must be gated with
    // `access_ok(USER_WRITE, buf, sizeof(*buf))` (#191).
    (void)buf;

    // Hold a reference while removing the segment from the namespace.
    shm_info = __shm_get_ref(shmid);
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
        spinlock_lock(&shm_info->lock);
        int can_remove = (shm_info->shmid.shm_perm.uid == task->uid) ||
                         (shm_info->shmid.shm_perm.cuid == task->uid);
        if (!can_remove) {
            spinlock_unlock(&shm_info->lock);
            pr_err("The calling process is not the creator or the owner of the "
                   "shared memory.\n");
            __shm_put_ref(shm_info);
            return -EPERM;
        }
        shm_info->removed = 1;
        spinlock_unlock(&shm_info->lock);
        spinlock_lock(&shm_list_lock);
        __list_remove_shm_info(shm_info);
        spinlock_unlock(&shm_list_lock);
        __shm_put_ref(shm_info); // drop namespace reference
        __shm_put_ref(shm_info); // drop IPC_RMID syscall reference
        return 0;
    }
    __shm_put_ref(shm_info);
    return 0;
}

// ============================================================================
// PROCFS FUNCTIONS
// ============================================================================

/// @brief Reads data from a shared memory segment.
/// @param file Pointer to the file structure associated with the shared memory.
/// @param buf Buffer where the read data will be stored.
/// @param offset Offset from where to start reading.
/// @param nbyte Number of bytes to read.
/// @return Number of bytes read on success, or -1 on error.
ssize_t procipc_shm_read(vfs_file_t *file, char *buf, off_t offset, size_t nbyte)
{
    // Check if file is NULL.
    if (!file) {
        pr_err("Received a NULL file.\n");
        return -ENOENT;
    }

    size_t buffer_len    = 0;
    size_t read_pos      = 0;
    size_t ret           = 0;
    ssize_t write_count  = 0;
    shm_info_t *shm_info = NULL;
    char buffer[BUFSIZ];

    // Prepare a buffer and initialize it to zero.
    memset(buffer, 0, BUFSIZ);

    // Prepare the header.
    ret = sprintf(
        buffer, "key      shmid perms      segsz   uid   gid  cuid  cgid      "
                "atime      dtime      ctime   cpid   lpid nattch\n");

    // Iterate through the list of shared memory.
    spinlock_lock(&shm_list_lock);
    list_for_each_decl (it, &shm_list) {
        // Get the current entry from the list.
        shm_info = list_entry(it, shm_info_t, list);

        // Add information about the current shared memory entry to the buffer.
        ret += sprintf(
            buffer + ret, "%8d %5d %10d %7zu %5d %4d %5d %9d %10d %10d %10d %5d %5d %5lu\n",
            abs(shm_info->shmid.shm_perm.key), shm_info->id, shm_info->shmid.shm_perm.mode, shm_info->shmid.shm_segsz,
            shm_info->shmid.shm_perm.uid, shm_info->shmid.shm_perm.gid, shm_info->shmid.shm_perm.cuid,
            shm_info->shmid.shm_perm.cgid, shm_info->shmid.shm_atime, shm_info->shmid.shm_dtime,
            shm_info->shmid.shm_ctime, shm_info->shmid.shm_cpid, shm_info->shmid.shm_lpid, shm_info->shmid.shm_nattch);
    }
    spinlock_unlock(&shm_list_lock);

    // Terminate the buffer with a newline.
    sprintf(buffer + ret, "\n");

    // Get the length of the buffer.
    buffer_len = strlen(buffer);
    read_pos   = offset;

    // Perform the read operation if offset is within bounds.
    if (read_pos < buffer_len) {
        while ((write_count < nbyte) && (read_pos < buffer_len)) {
            buf[write_count] = buffer[read_pos];
            // Move the pointers.
            ++read_pos;
            ++write_count;
        }
    }

    // Return the number of bytes read.
    return write_count;
}
