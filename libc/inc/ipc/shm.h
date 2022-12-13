/// @file shm.h
/// @brief Definition of structure for managing shared memories.
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "sys/types.h"
#include "stddef.h"
#include "time.h"
#include "ipc.h"

/// Data type for storing the number of attaches.
typedef unsigned long shmatt_t;

/// @brief Shared memory data structure.
struct shmid_ds {
    /// Operation permission struct.
    struct ipc_perm shm_perm;
    /// Size of segment in bytes.
    size_t shm_segsz;
    /// Time of last shmat().
    time_t shm_atime;
    /// Time of last shmdt().
    time_t shm_dtime;
    /// Time of last change by shmctl().
    time_t shm_ctime;
    /// Pid of creator.
    pid_t shm_cpid;
    /// Pid of last shmop.
    pid_t shm_lpid;
    /// Number of current attaches.
    shmatt_t shm_nattch;
    /// Pointer to the next shared memory data structure.
    struct shmid_ds *next;
    /// Where shm created is memorized, should be a file.
    void *shm_location;
};

#if 0

#include "ipc.h"
#include "debug.h"
#include "time.h"
#include "kheap.h"
#include "stddef.h"
#include "paging.h"
#include "syscall.h"
#include "scheduler.h"

//======== Permission flag for shmget ==========================================
// or S_IRUGO from <linux/stat.h>.
#define SHM_R      0400

// or S_IWUGO from <linux/stat.h>.
#define SHM_W      0200
//==============================================================================

//======== Flags for shmat =====================================================
// Attach read-only else read-write.
#define SHM_RDONLY 010000

// Round attach address to SHMLBA.
#define SHM_RND    020000

// Take-over region on attach.
#define SHM_REMAP  040000

// Execution access.
#define SHM_EXEC   0100000
//==============================================================================

//======== Commands for shmctl =================================================
// Lock segment (root only).
#define SHM_LOCK   11

// Unlock segment (root only).
#define SHM_UNLOCK 12
//==============================================================================

// Ipcs ctl commands.
#define SHM_STAT   13

#define SHM_INFO 14

#define SHM_STAT_ANY  15

//======== shm_mode upper byte flags ===========================================
// segment will be destroyed on last detach.
#define SHM_DEST      01000

// Segment will not be swapped.
#define SHM_LOCKED    02000

// Segment is mapped via hugetlb.
#define SHM_HUGETLB   04000

// Don't check for reservations.
#define SHM_NORESERVE 010000

/// @@brief Syscall Service Routine: Shared memory control operation.
int syscall_shmctl(int *args);

/// @@brief Syscall Service Routine: Get shared memory segment.
int syscall_shmget(int *args);

/// @@brief Syscall Service Routine: Attach shared memory segment.
void *syscall_shmat(int *args);

/// @@brief Syscall  Service Routine: Detach shared memory segment.
int syscall_shmdt(int *args);

/// @@brief User Wrapper: Shared memory control operation.
int shmctl(int shmid, int cmd, struct shmid_ds *buf);

/// @@brief User Wrapper: Get shared memory segment.
int shmget(key_t key, size_t size, int flags);

/// @@brief User Wrapper: Attach shared memory segment.
void *shmat(int shmid, void *shmaddr, int flag);

/// @@brief User Wrapper: Detach shared memory segment.
int shmdt(void *shmaddr);

/// @@brief Find shmid_ds on list.
struct shmid_ds *find_shm_fromid(int shmid);

/// @@brief Find shmid_ds on list.
struct shmid_ds *find_shm_fromkey(key_t key);

/// @@brief shmid_ds on list.
struct shmid_ds *find_shm_fromvaddr(void *shmvaddr);

#endif

#ifdef __KERNEL__

/// @brief Get a System V shared memory identifier.
/// @param key can be used either to obtain the identifier of a previously
/// created shared memory, or to create a new one.
/// @param size of the shared memory, rounded up to a multiple of PAGE_SIZE.
/// @param flag controls the behaviour of the function.
/// @return the shared memory identifier, -1 on failure, and errno is set to
/// indicate the error.
long sys_shmget(key_t key, size_t size, int flag);

/// @brief Attaches the shared memory segment identified by shmid to the address
/// space of the calling process.
/// @param shmid the shared memory identifier.
/// @param shmaddr the attaching address.
/// @param shmflg controls the behaviour of the function.
/// @return returns the address of the attached shared memory segment; on error
/// (void *) -1 is returned, and errno is set to indicate the error.
void * sys_shmat(int shmid, const void *shmaddr, int shmflg);

/// @brief Detaches the shared memory segment located at the address specified
/// by shmaddr from the address space of the calling process
/// @param shmaddr the address of the shared memory segment.
/// @return 0 on success, -1 on failure and errno is set to indicate the error.
long sys_shmdt(const void *shmaddr);

/// @brief Performs the control operation specified by cmd on the shared memory
/// segment whose identifier is given in shmid.
/// @param shmid the shared memory identifier.
/// @param cmd the command to perform.
/// @param buf is a pointer to a shmid_ds structure.
/// @return a non-negative value basedon on the requested operation, -1 on
/// failure and errno is set to indicate the error.
long sys_shmctl(int shmid, int cmd, struct shmid_ds *buf);

#else

/// @brief Get a System V shared memory identifier.
/// @param key can be used either to obtain the identifier of a previously
/// created shared memory, or to create a new one.
/// @param size of the shared memory, rounded up to a multiple of PAGE_SIZE.
/// @param flag controls the behaviour of the function.
/// @return the shared memory identifier, -1 on failure, and errno is set to
/// indicate the error.
long shmget(key_t key, size_t size, int flag);

/// @brief Attaches the shared memory segment identified by shmid to the address
/// space of the calling process.
/// @param shmid the shared memory identifier.
/// @param shmaddr the attaching address.
/// @param shmflg controls the behaviour of the function.
/// @return returns the address of the attached shared memory segment; on error
/// (void *) -1 is returned, and errno is set to indicate the error.
void * shmat(int shmid, const void *shmaddr, int shmflg);

/// @brief Detaches the shared memory segment located at the address specified
/// by shmaddr from the address space of the calling process
/// @param shmaddr the address of the shared memory segment.
/// @return 0 on success, -1 on failure and errno is set to indicate the error.
long shmdt(const void *shmaddr);

/// @brief Performs the control operation specified by cmd on the shared memory
/// segment whose identifier is given in shmid.
/// @param shmid the shared memory identifier.
/// @param cmd the command to perform.
/// @param buf is a pointer to a shmid_ds structure.
/// @return a non-negative value basedon on the requested operation, -1 on
/// failure and errno is set to indicate the error.
long shmctl(int shmid, int cmd, struct shmid_ds *buf);

#endif
