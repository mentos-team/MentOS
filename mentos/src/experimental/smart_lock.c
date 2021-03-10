///                MentOS, The Mentoring Operating system project
/// @file smart_lock.c
/// @brief
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "smart_lock.h"
#include "errno.h"

#define SEM_MAX 356
#define SEM_VALUE_MAX 32

typedef union {
	volatile atomic_t value;
	volatile atomic_t mutex;
	bool_t used;
} lock_t;

lock_t semaphores[SEM_MAX] = { 0 };

int lock_create()
{
	for (unsigned i = 0; i < SEM_MAX; ++i) {
		if (!semaphores[i].used) {
			semaphores[i].used = true;
			return (int)i;
		}
	}
	return -1;
}

int lock_destroy(int id)
{
	if ((id < 0) || (id > SEM_MAX))
		return -1;
	semaphores[id].used = false;
	return 0;
}

int lock_init(int id)
{
	if ((id < 0) || (id > SEM_MAX))
		return -1;
	atomic_set(&semaphores[id].value, 0);
	return 0;
}

int lock_try_acquire(int id)
{
	if ((id < 0) || (id > SEM_MAX))
		return -1;
	return atomic_set_and_test(&semaphores[id].value, 1) == 0;
}

int lock_release(int id)
{
	if ((id < 0) || (id > SEM_MAX))
		return -1;
	atomic_set(&semaphores[id].value, 0);
	return 0;
}
