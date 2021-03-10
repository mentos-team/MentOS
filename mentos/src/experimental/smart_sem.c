///                MentOS, The Mentoring Operating system project
/// @file smart_sem.c
/// @brief
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "smart_sem.h"
#include "syscall.h"
#include "syscall_types.h"
#include "errno.h"

int sem_create()
{
	int retval;
	DEFN_SYSCALL0(retval, __NR_lock_create);
	if (retval < 0) {
		errno = -retval;
		return -1;
	}
	return retval;
}

int sem_destroy(int id)
{
	int retval;
	DEFN_SYSCALL1(retval, __NR_lock_destroy, id);
	if (retval < 0) {
		errno = -retval;
		return -1;
	}
	return retval;
}

int sem_init(int id)
{
	int retval;
	DEFN_SYSCALL1(retval, __NR_lock_init, id);
	if (retval < 0) {
		errno = -retval;
		return -1;
	}
	return retval;
}

int sem_acquire(int id)
{
	int retval;
	do {
		DEFN_SYSCALL1(retval, __NR_lock_try_acquire, id);
		if (retval < 0) {
			errno = -retval;
			return -1;
		}
	} while (retval != 1);
	return 0;
}

int sem_release(int id)
{
	int retval;
	DEFN_SYSCALL1(retval, __NR_lock_release, id);
	if (retval < 0) {
		errno = -retval;
		return -1;
	}
	return retval;
}