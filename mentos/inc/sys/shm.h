///                MentOS, The Mentoring Operating system project
/// @file shm.h
/// @brief
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "ipc.h"
#include "debug.h"
#include "clock.h"
#include "kheap.h"
#include "stddef.h"
#include "paging.h"
#include "syscall.h"
#include "scheduler.h"

//======== Permission flag for shmget ==========================================
// or S_IRUGO from <linux/stat.h>.
#define SHM_R 0400

// or S_IWUGO from <linux/stat.h>.
#define SHM_W 0200
//==============================================================================

//======== Flags for shmat =====================================================
// Attach read-only else read-write.
#define SHM_RDONLY 010000

// Round attach address to SHMLBA.
#define SHM_RND 020000

// Take-over region on attach.
#define SHM_REMAP 040000

// Execution access.
#define SHM_EXEC 0100000
//==============================================================================

//======== Commands for shmctl =================================================
// Lock segment (root only).
#define SHM_LOCK 11

// Unlock segment (root only).
#define SHM_UNLOCK 12
//==============================================================================

// Ipcs ctl commands.
#define SHM_STAT 13

#define SHM_INFO 14

#define SHM_STAT_ANY 15

//======== shm_mode upper byte flags ===========================================
// segment will be destroyed on last detach.
#define SHM_DEST 01000

// Segment will not be swapped.
#define SHM_LOCKED 02000

// Segment is mapped via hugetlb.
#define SHM_HUGETLB 04000

// Don't check for reservations.
#define SHM_NORESERVE 010000

typedef unsigned long shmatt_t;

struct shmid_ds {
	// Operation permission struct.
	struct ipc_perm shm_perm;

	// Size of segment in bytes.
	size_t shm_segsz;

	// Time of last shmat().
	time_t shm_atime;

	// Time of last shmdt().
	time_t shm_dtime;

	// Time of last change by shmctl().
	time_t shm_ctime;

	// Pid of creator.
	pid_t shm_cpid;

	// Pid of last shmop.
	pid_t shm_lpid;

	// Number of current attaches.
	shmatt_t shm_nattch;

	struct shmid_ds *next;

	// Where shm created is memorized, should be a file.
	void *shm_location;
};

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
