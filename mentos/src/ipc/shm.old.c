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

#if 0
struct shmid_ds *head        = NULL;
static ushort shm_descriptor = 0;

int syscall_shmctl(int *args)
{
    int shmid = args[0];
    int cmd   = args[1];

    // TODO: for IPC_STAT
    // struct shmid_ds * buf = (struct shmid_ds *) args[2];

    struct shmid_ds *myshmid_ds = find_shm_fromid(shmid);

    if (myshmid_ds == NULL) {
        return -1;
    }

    // Upgrade shm info.
    myshmid_ds->shm_lpid  = scheduler_get_current_process()->pid;
    myshmid_ds->shm_ctime = time(NULL);

    switch (cmd) {
    case IPC_RMID:
        if (myshmid_ds->shm_nattch == 0) {
            kfree(myshmid_ds->shm_location);

            // Manage list.
            if (myshmid_ds == head) {
                head = head->next;
            } else {
                // Finding the previous shmid_ds.
                struct shmid_ds *prev = head;
                while (prev->next != myshmid_ds) {
                    prev = prev->next;
                }
                prev->next = myshmid_ds->next;
            }
            kfree(myshmid_ds);
        } else {
            (myshmid_ds->shm_perm).mode |= SHM_DEST;
        }

        return 0;

    case IPC_STAT:
        break;
    case IPC_SET:
        break;
    case SHM_LOCK:
        break;
    case SHM_UNLOCK:
        break;
    default:
        break;
    }

    return -1;
}

// Get shared memory segment.
int syscall_shmget(int *args)
{
    int flags   = args[2];
    key_t key   = (key_t)args[0];
    size_t size = (size_t)args[1];

    struct shmid_ds *shmid_ds;

    if (flags & IPC_EXCL) {
        return -1;
    }

    if (flags & IPC_CREAT) {
        shmid_ds = find_shm_fromkey(key);

        if (shmid_ds != NULL) {
            return -1;
        }

        shmid_ds = kmalloc(sizeof(struct shmid_ds));
        pr_default("\n[SHM] shmget() shmid_ds      : 0x%p", shmid_ds);

        shmid_ds->shm_location = kmalloc_align(size);
        pr_default("\n[SHM] shmget() Location      : 0x%p",
                  shmid_ds->shm_location);
        pr_default("\n[SHM] shmget() physLocation  : 0x%p",
                  paging_virtual_to_physical(get_current_page_directory(),
                                             shmid_ds->shm_location));

        shmid_ds->next = head;
        head           = shmid_ds;

        shmid_ds->shm_segsz  = size;
        shmid_ds->shm_atime  = 0;
        shmid_ds->shm_dtime  = 0;
        shmid_ds->shm_ctime  = 0;
        shmid_ds->shm_cpid   = scheduler_get_current_process()->pid;
        shmid_ds->shm_lpid   = scheduler_get_current_process()->pid;
        shmid_ds->shm_nattch = 0;

        // No user implementation.
        (shmid_ds->shm_perm).cuid = 0;
        // No group implementation.
        (shmid_ds->shm_perm).cgid = 0;
        // No user implementation
        (shmid_ds->shm_perm).uid = 0;
        // No group implementation.
        (shmid_ds->shm_perm).gid  = 0;
        (shmid_ds->shm_perm).mode = flags & 0777;
        (shmid_ds->shm_perm).seq  = shm_descriptor++;
        (shmid_ds->shm_perm).key  = key;
    } else {
        shmid_ds = find_shm_fromkey(key);
        pr_default("\n[SHM] shmget() shmid_ds found  : 0x%p", shmid_ds);

        if (shmid_ds == NULL) {
            return -1;
        }

        if ((flags & 0777) > ((shmid_ds->shm_perm).mode & 0777)) {
            return -1;
        }
        shmid_ds->shm_lpid = scheduler_get_current_process()->pid;
    }

    return (shmid_ds->shm_perm).seq;
}

// Attach shared memory segment.
void *syscall_shmat(int *args)
{
    int shmid     = args[0];
    void *shmaddr = (void *)args[1];

    // TODO: for more settings
    // int flags = args[2];

    struct shmid_ds *myshmid_ds = find_shm_fromid(shmid);
    pr_default("\n[SHM] shmat() shmid_ds found  : 0x%p", myshmid_ds);

    if (myshmid_ds == NULL) {
        return (void *)-1;
    }

    void *shm_start = myshmid_ds->shm_location;

    if (shmaddr == NULL) {
        void *ret = kmalloc_align(myshmid_ds->shm_segsz);

        uint32_t shm_vaddr_start = (uint32_t)ret & 0xfffff000;
        uint32_t shm_vaddr_end =
            ((uint32_t)ret + myshmid_ds->shm_segsz) & 0xfffff000;

        uint32_t shm_paddr_start = (uint32_t)paging_virtual_to_physical(
            get_current_page_directory(), shm_start);

        free_map_region(get_current_page_directory(), shm_vaddr_start,
                        shm_vaddr_end, true);

        while (shm_vaddr_start <= shm_vaddr_end) {
            paging_allocate_page(get_current_page_directory(), shm_vaddr_start,
                                 shm_paddr_start / PAGE_SIZE, true, true);
            shm_vaddr_start += PAGE_SIZE;
            shm_paddr_start += PAGE_SIZE;
        }

        pr_default("\n[SHM] shmat() vaddr          : 0x%p", ret);
        pr_default("\n[SHM] shmat() paddr          : 0x%p",
                  (void *)shm_paddr_start);
        pr_default("\n[SHM] shmat() paddr after map: 0x%p",
                  paging_virtual_to_physical(get_current_page_directory(),
                                             ret));

        // Upgrade shm info.
        myshmid_ds->shm_lpid = scheduler_get_current_process()->pid;
        (myshmid_ds->shm_nattch)++;
        myshmid_ds->shm_atime = time(NULL);

        return ret;
    }

    return (void *)-1;
}

// Detach shared memory segment.
int syscall_shmdt(int *args)
{
    void *shmaddr = (void *)args[0];

    if (shmaddr == NULL) {
        return -1;
    }

    struct shmid_ds *myshmid_ds = find_shm_fromvaddr(shmaddr);
    pr_default("\n[SHM] shmdt() shmid_ds found  : 0x%p", myshmid_ds);

    if (myshmid_ds == NULL) {
        return -1;
    }

    // ===== Test ==============================================================
    uint32_t shm_vaddr_start = (uint32_t)shmaddr & 0xfffff000;
    uint32_t shm_vaddr_end =
        ((uint32_t)shmaddr + myshmid_ds->shm_segsz) & 0xfffff000;

    free_map_region(get_current_page_directory(), shm_vaddr_start,
                    shm_vaddr_end, false);

    while (shm_vaddr_start <= shm_vaddr_end) {
        paging_allocate_page(get_current_page_directory(), shm_vaddr_start,
                             shm_vaddr_start / PAGE_SIZE, true, true);
        shm_vaddr_start += PAGE_SIZE;
    }
    // =========================================================================

    kfree(shmaddr);

    // Upgrade shm info.
    myshmid_ds->shm_lpid = scheduler_get_current_process()->pid;
    (myshmid_ds->shm_nattch)--;
    myshmid_ds->shm_dtime = time(NULL);

    // Manage SHM_DEST flag on.
    if (myshmid_ds->shm_nattch == 0 && (myshmid_ds->shm_perm).mode & SHM_DEST) {
        kfree(myshmid_ds->shm_location);

        // Manage list.
        if (myshmid_ds == head) {
            head = head->next;
        } else {
            // Finding the previous shmid_ds.
            struct shmid_ds *prev = head;
            while (prev->next != myshmid_ds) {
                prev = prev->next;
            }
            prev->next = myshmid_ds->next;
        }
        kfree(myshmid_ds);
    }

    return 0;
}

int shmctl(int shmid, int cmd, struct shmid_ds *buf)
{
    int error;

    __asm__("movl   %0, %%ecx\n"
            "movl   %1, %%ebx\n"
            "movl   %2, %%edx\n"
            "movl   $6, %%eax\n"
            "int    $80\n"
            :
            : "r"(shmid), "r"(cmd), "r"(buf));
    __asm__("movl %%eax, %0\n\t"
            : "=r"(error));

    return error;
}

int shmget(key_t key, size_t size, int flags)
{
    int id;

    __asm__("movl   %0, %%ecx\n"
            "movl   %1, %%ebx\n"
            "movl   %2, %%edx\n"
            "movl   $3, %%eax\n"
            "int    $80\n"
            :
            : "r"(key), "r"(size), "r"(flags));
    __asm__("movl %%eax, %0\n\t"
            : "=r"(id));

    return id;
}

void *shmat(int shmid, void *shmaddr, int flag)
{
    void *addr;

    __asm__("movl   %0, %%ecx\n"
            "movl   %1, %%ebx\n"
            "movl   %2, %%edx\n"
            "movl   $4, %%eax\n"
            "int    $80\n"
            :
            : "r"(shmid), "r"(shmaddr), "r"(flag));
    // The kernel is serving my system call

    // Now I have the control
    __asm__("movl %%eax, %0\n\t"
            : "=r"(addr));

    return addr;
}

int shmdt(void *shmaddr)
{
    int error;

    __asm__("movl   %0, %%ecx\n"
            "movl   $5, %%eax\n"
            "int    $80\n"
            :
            : "r"(shmaddr));
    __asm__("movl %%eax, %0\n\t"
            : "=r"(error));

    return error;
}

struct shmid_ds *find_shm_fromid(int shmid)
{
    struct shmid_ds *res = head;

    while (res != NULL) {
        if ((res->shm_perm).seq == shmid) {
            return res;
        }
        res = res->next;
    }

    return NULL;
}

struct shmid_ds *find_shm_fromkey(key_t key)
{
    struct shmid_ds *res = head;

    while (res != NULL) {
        if ((res->shm_perm).key == key) {
            return res;
        }
        res = res->next;
    }

    return NULL;
}

struct shmid_ds *find_shm_fromvaddr(void *shmvaddr)
{
    void *shmpaddr =
        paging_virtual_to_physical(get_current_page_directory(), shmvaddr);
    void *paddr;
    struct shmid_ds *res = head;

    while (res != NULL) {
        paddr = paging_virtual_to_physical(get_current_page_directory(),
                                           res->shm_location);
        if (paddr == shmpaddr) {
            return res;
        }
        res = res->next;
    }

    return NULL;
}
#endif

void *sys_shmat(int shmid, const void *shmaddr, int shmflg)
{
    return 0;
}

int sys_shmget(key_t key, size_t size, int flag)
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

ssize_t procipc_shm_read(vfs_file_t *file, char *buf, off_t offset, size_t nbyte)
{
    if (!file) {
        pr_err("Received a NULL file.\n");
        return -ENOENT;
    }
    size_t buffer_len = 0, read_pos = 0, write_count = 0, ret = 0;
    char buffer[BUFSIZ];

    // Prepare a buffer.
    memset(buffer, 0, BUFSIZ);
    // Prepare the header.
    ret = sprintf(buffer, "key      shmid ...\n");

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

///! @endcond
