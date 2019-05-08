///                MentOS, The Mentoring Operating system project
/// @file shm.c
/// @brief
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "shm.h"

struct shmid_ds *head = NULL;
static ushort shm_descriptor = 0;

int syscall_shmctl(int *args)
{
	int shmid = args[0];
	int cmd = args[1];

	// TODO: for IPC_STAT
	// struct shmid_ds * buf = (struct shmid_ds *) args[2];

	struct shmid_ds *myshmid_ds = find_shm_fromid(shmid);

	if (myshmid_ds == NULL) {
		return -1;
	}

	// Upgrade shm info.
	myshmid_ds->shm_lpid = kernel_get_current_process()->pid;
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
	int flags = args[2];
	key_t key = (key_t)args[0];
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
		dbg_print("\n[SHM] shmget() shmid_ds      : 0x%p", shmid_ds);

		shmid_ds->shm_location = kmalloc_align(size);
		dbg_print("\n[SHM] shmget() Location      : 0x%p",
				  shmid_ds->shm_location);
		dbg_print("\n[SHM] shmget() physLocation  : 0x%p",
				  paging_virtual_to_physical(get_current_page_directory(),
											 shmid_ds->shm_location));

		shmid_ds->next = head;
		head = shmid_ds;

		shmid_ds->shm_segsz = size;
		shmid_ds->shm_atime = 0;
		shmid_ds->shm_dtime = 0;
		shmid_ds->shm_ctime = 0;
		shmid_ds->shm_cpid = kernel_get_current_process()->pid;
		shmid_ds->shm_lpid = kernel_get_current_process()->pid;
		shmid_ds->shm_nattch = 0;

		// No user implementation.
		(shmid_ds->shm_perm).cuid = 0;
		// No group implementation.
		(shmid_ds->shm_perm).cgid = 0;
		// No user implementation
		(shmid_ds->shm_perm).uid = 0;
		// No group implementation.
		(shmid_ds->shm_perm).gid = 0;
		(shmid_ds->shm_perm).mode = flags & 0777;
		(shmid_ds->shm_perm).seq = shm_descriptor++;
		(shmid_ds->shm_perm).key = key;
	} else {
		shmid_ds = find_shm_fromkey(key);
		dbg_print("\n[SHM] shmget() shmid_ds found  : 0x%p", shmid_ds);

		if (shmid_ds == NULL) {
			return -1;
		}

		if ((flags & 0777) > ((shmid_ds->shm_perm).mode & 0777)) {
			return -1;
		}
		shmid_ds->shm_lpid = kernel_get_current_process()->pid;
	}

	return (shmid_ds->shm_perm).seq;
}

// Attach shared memory segment.
void *syscall_shmat(int *args)
{
	int shmid = args[0];
	void *shmaddr = (void *)args[1];

	// TODO: for more settings
	// int flags = args[2];

	struct shmid_ds *myshmid_ds = find_shm_fromid(shmid);
	dbg_print("\n[SHM] shmat() shmid_ds found  : 0x%p", myshmid_ds);

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

		dbg_print("\n[SHM] shmat() vaddr          : 0x%p", ret);
		dbg_print("\n[SHM] shmat() paddr          : 0x%p",
				  (void *)shm_paddr_start);
		dbg_print("\n[SHM] shmat() paddr after map: 0x%p",
				  paging_virtual_to_physical(get_current_page_directory(),
											 ret));

		// Upgrade shm info.
		myshmid_ds->shm_lpid = kernel_get_current_process()->pid;
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
	dbg_print("\n[SHM] shmdt() shmid_ds found  : 0x%p", myshmid_ds);

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
	myshmid_ds->shm_lpid = kernel_get_current_process()->pid;
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
	__asm__("movl %%eax, %0\n\t" : "=r"(error));

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
	__asm__("movl %%eax, %0\n\t" : "=r"(id));

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
	__asm__("movl %%eax, %0\n\t" : "=r"(addr));

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
	__asm__("movl %%eax, %0\n\t" : "=r"(error));

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
