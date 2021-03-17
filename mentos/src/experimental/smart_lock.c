///                MentOS, The Mentoring Operating system project
/// @file smart_lock.c
/// @brief
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "smart_lock.h"
#include "errno.h"
#include "resource.h"
#include "kheap.h"
#include "deadlock_prevention.h"
#include "panic.h"

#define SEM_MAX 356
#define SEM_VALUE_MAX 32

typedef struct {
    volatile atomic_t value;
    volatile atomic_t mutex;
	bool_t used;
	resource_t *sem_resource;
} lock_t;

lock_t semaphores[SEM_MAX] = { 0 };


int lock_create()
{
	for (unsigned i = 0; i < SEM_MAX; ++i) {
		if (!semaphores[i].used) {
			semaphores[i].sem_resource = resource_create("sem");
		    if (!semaphores[i].sem_resource) {
			    return -1;
            }

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
	resource_destroy(semaphores[id].sem_resource);
	return 0;
}

int lock_init(int id)
{
	if ((id < 0) || (id > SEM_MAX))
		return -1;
	atomic_set(&semaphores[id].value, 0);
	resource_init(semaphores[id].sem_resource);
	return 0;
}

int lock_try_acquire(int id)
{
	if ((id < 0) || (id > SEM_MAX))
		return -1;
	/*
	if (atomic_set_and_test(&semaphores[id].value, 1) == 0) {
	    resource_assign(semaphores[id].sem_resource);
	    return 1;
	}*/
	int ret = 0;

	// Init deadlock prevention structures.
    size_t n = kernel_get_active_processes();
    size_t m = kernel_get_active_resources();
    available = (uint32_t *)  kmalloc(m * sizeof(uint32_t));
    max       = (uint32_t **) kmmalloc(n, m * sizeof(uint32_t));
    alloc     = (uint32_t **) kmmalloc(n, m * sizeof(uint32_t));
    need      = (uint32_t **) kmmalloc(n, m * sizeof(uint32_t));
    struct task_struct **idx_map_task_struct = (struct task_struct **) kmalloc(
            n * sizeof(struct task_struct *));
    init_deadlock_structures(alloc, max, available, need, idx_map_task_struct);

    // Init request vector.
    uint32_t *req_vec = (uint32_t *) kmalloc(m * sizeof(uint32_t));
    memset(req_vec, 0, m * sizeof(uint32_t));
    req_vec[semaphores[id].sem_resource->rid] = 1;

    // Find current task correct index.
    size_t current_task_idx = -1;
    for (size_t t_i = 0; t_i < n; t_i++) {
        if (idx_map_task_struct[t_i] == kernel_get_current_process()) {
            current_task_idx = t_i;
            break;
        }
    }

    switch (request(req_vec, current_task_idx, n, m)) {
        case WAIT:
        case WAIT_UNSAFE:
            ret = 0;
            break;
        case SAFE:
            if (atomic_set_and_test(&semaphores[id].value, 1) == 0) {
                resource_assign(semaphores[id].sem_resource);
                ret = 1;
            } else {
                kernel_panic("allocation request return bad safe status");
            }
            break;
        case ERROR:
        default:
            kernel_panic("deadlock prevention error");
    }

    kfree(available);
    kmfree((void **) max, n);
    kmfree((void **) alloc, n);
    kmfree((void **) need, n);
    kfree(idx_map_task_struct);
    kfree(req_vec);

	return ret;
}

int lock_release(int id)
{
	if ((id < 0) || (id > SEM_MAX))
		return -1;
	atomic_set(&semaphores[id].value, 0);
    resource_deassign(semaphores[id].sem_resource);
	return 0;
}
