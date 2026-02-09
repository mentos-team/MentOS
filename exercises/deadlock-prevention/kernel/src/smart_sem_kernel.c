///                MentOS, The Mentoring Operating system project
/// @file smart_sem_kernel.c
/// @brief Smart semaphore kernel-side implementation source code.
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "smart_sem_kernel.h"

#include "errno.h"
#include "resource.h"
#include "kheap.h"
#include "deadlock_prevention.h"
#include "panic.h"
#include "debug.h"

/// Max number of semaphore that the operating system can manage.
#define SEM_MAX 356
/// Max value of a semaphore. WIP
#define SEM_VALUE_MAX 32

/// @brief Smart semaphore descriptor.
typedef struct {
    /// Semaphore value. The initialization value is 0.
    volatile atomic_t value;
    /// Semaphore mutex value. The initialization value is 0.
    volatile atomic_t mutex;
    /// Set to true if this semaphore instance is used, otherwise is false.
    bool_t used;
    /// Reference to the resource related with the semaphore used. If semaphore
    /// is not used, this pointer is NULL.
    resource_t *sem_resource;
} smart_sem_t;

/// Global array of all smart semaphores that can be allocated.
smart_sem_t semaphores[SEM_MAX] = { 0 };

/// @brief Check if the semaphore is available and if it can be safety taken. If
/// the acquisition is safe, it set the semaphore value.
/// Safety check is done only if ENABLE_DEADLOCK_PREVENTION is true.
/// @param id Smart semaphore id.
/// @return True if semaphore can be safely acquired, otherwise if the
/// semaphore acquisition is unsafe or not available, return false.
static bool_t sys_sem_try(int id)
{
#if ENABLE_DEADLOCK_PREVENTION
    // Init deadlock prevention structures.
    size_t n = kernel_get_active_processes();
    size_t m = kernel_get_active_resources();
    uint32_t *arr_available  = (uint32_t *)  kmalloc(m * sizeof(uint32_t));
    uint32_t **mat_max       = (uint32_t **) kmmalloc(n, m * sizeof(uint32_t));
    uint32_t **mat_alloc     = (uint32_t **) kmmalloc(n, m * sizeof(uint32_t));
    uint32_t **mat_need      = (uint32_t **) kmmalloc(n, m * sizeof(uint32_t));
    task_struct **idx_map_task_struct = (task_struct **) kmalloc(
            n * sizeof(task_struct *));
    if (arr_available && mat_max && mat_alloc && mat_need
        && idx_map_task_struct) {
        init_deadlock_structures(arr_available, mat_max, mat_alloc, mat_need,
                idx_map_task_struct);
    } else {
        kernel_panic("not able to perform allocation for deadlock prevention");
    }

    // Init request vector.
    uint32_t *req_vec = (uint32_t *) kmalloc(m * sizeof(uint32_t));
    if (req_vec) {
        memset(req_vec, 0, m * sizeof(uint32_t));
        req_vec[semaphores[id].sem_resource->rid] = 1;
    } else {
        kernel_panic("not able to perform allocation for deadlock prevention");
    }

    // Find current task correct index.
    int32_t current_task_idx = get_current_task_idx_from(idx_map_task_struct);
    if (current_task_idx < 0) {
        kernel_panic("did't find current task in idx_map_task_struct array");
    }

    // Perform request.
    bool_t ret = false;
    switch (request(req_vec, current_task_idx, arr_available, mat_alloc,
            mat_need, n, m)) {
        case WAIT:
        case WAIT_UNSAFE:
            break;
        case SAFE:
            if (atomic_set_and_test(&semaphores[id].value, 1) == 0) {
                ret = true;
            } else {
                kernel_panic("allocation request return bad safe status");
            }
            break;
        case ERROR:
        default:
            kernel_panic("deadlock prevention error");
    }

    kfree(arr_available);
    kmfree((void **) mat_max, n);
    kmfree((void **) mat_alloc, n);
    kmfree((void **) mat_need, n);
    kfree(idx_map_task_struct);
    kfree(req_vec);

    return ret;
#else
    dbg_print("sys_sem_try(%d)\n", id);
    return atomic_set_and_test(&semaphores[id].value, 1) == 0;
#endif
}

int sys_sem_create()
{
    int id = -1;
    for (unsigned i = 0; i < SEM_MAX; ++i) {
        if (!semaphores[i].used) {
            semaphores[i].sem_resource = resource_create("sem");
            if (!semaphores[i].sem_resource) {
                return -1;
            }

            semaphores[i].used = true;
            id = (int)i;
            break;
        }
    }
    dbg_print("sys_sem_create() -> %d\n", id);
    return id;
}

int sys_sem_destroy(int id)
{
    dbg_print("sys_sem_destroy(%d)\n", id);
    if ((id < 0) || (id > SEM_MAX)) {
        return -1;
    }
    semaphores[id].used = false;
    resource_destroy(semaphores[id].sem_resource);
    return 0;
}

int sys_sem_init(int id)
{
    dbg_print("sys_sem_init(%d)\n", id);
    if ((id < 0) || (id > SEM_MAX)) {
        return -1;
    }
    if (!semaphores[id].used) {
        return -1;
    }
    atomic_set(&semaphores[id].value, 0);
    resource_init(semaphores[id].sem_resource);
    return 0;
}

int sys_sem_try_acquire(int id)
{
    dbg_print("sys_sem_try_acquire(%d)\n", id);
    if ((id < 0) || (id > SEM_MAX)) {
        return -1;
    }
    if (!semaphores[id].used) {
        return -1;
    }

    if (sys_sem_try(id)) {
        resource_assign(semaphores[id].sem_resource);
        return 1;
    }

    return 0;
}

int sys_sem_release(int id)
{
    dbg_print("sys_sem_release(%d)\n", id);
    if ((id < 0) || (id > SEM_MAX)) {
        return -1;
    }
    if (!semaphores[id].used) {
        return -1;
    }
    atomic_set(&semaphores[id].value, 0);
    resource_deassign(semaphores[id].sem_resource);
    return 0;
}
