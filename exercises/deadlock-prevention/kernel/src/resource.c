///                MentOS, The Mentoring Operating system project
/// @file resource.c
/// @brief Resource definition source code.
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "resource.h"
#include "kheap.h"
#include "string.h"
#include "arr_math.h"

/// The list of processes.
extern runqueue_t runqueue;
/// The list of resources.
resource_list_t r_list;

/// @brief Remove the resource reference dependency from each task in running
/// state.
/// @param r Resource pointer.
static void clean_resource_reference(resource_t *r)
{
    if (!r) return;
    // Loop on running tasks.
    list_head *it;
    list_for_each (it, &runqueue.queue) {
        task_struct *entry = list_entry(it, task_struct, run_list);
        if (entry != NULL) {
            // Loop on resources that tasks depend on.
            for (size_t r_i = 0; r_i < TASK_RESOURCE_MAX_AMOUNT; r_i++) {
                // Clean the resource reference in the task resources list.
                if (entry->resources[r_i] == r) {
                    entry->resources[r_i] = NULL;
                }
            }
        }
    }
}

/// @brief Generate the idx_map_task_struct array, that maps an index with a
/// related process.
/// @param idx_map_task_struct Pointer of the array already allocated to fill.
/// @return The same pointer passed as parameter.
static task_struct **compute_index_map_task_struct(
        task_struct *idx_map_task_struct[])
{
    // Loop on running tasks.
    list_head *task_it;
    size_t t_i = 0;
    list_for_each (task_it, &runqueue.queue) {
        task_struct *task = list_entry(task_it, task_struct, run_list);
        if (task != NULL) {
            // Map a task with an index.
            idx_map_task_struct[t_i] = task;
            t_i++;
        }
    }

    return idx_map_task_struct;
}

/// @brief Generate the available array, that contains the resources instances
/// currently available.
/// @param arr_available Pointer of the array already allocated to fill.
/// @return The same pointer passed as parameter.
static uint32_t *fill_available(uint32_t *arr_available)
{
    // Loop on resources created.
    list_head *resource_it;
    list_for_each (resource_it, &r_list.head) {
        resource_t *resource = list_entry(resource_it, resource_t,
                                          resources_list);
        arr_available[resource->rid] =
                resource->n_instances - resource->assigned_instances;
    }

    return arr_available;
}

/// @brief Generate the max matrix, that contains the maximum number of
/// resources instances that each task may require.
/// @param mat_max Pointer of the matrix already allocated to fill.
/// @param idx_map_task_struct Pointer to the array of index and tasks mapping.
/// @return The same pointer passed as first parameter.
static uint32_t **fill_max(uint32_t **mat_max, task_struct *idx_map_task_struct[])
{
    // Loop on all tasks.
    size_t n = kernel_get_active_processes();
    size_t m = kernel_get_active_resources();
    for (size_t t_i = 0; t_i < n; t_i++) {
        task_struct *task = idx_map_task_struct[t_i];
        if (task != NULL) {
            // Find resources needed by the task.
            for (size_t r_i = 0; r_i < m; r_i++) {
                if (task->resources[r_i] != NULL) {
                    mat_max[t_i][r_i] = task->resources[r_i]->n_instances;
                }
            }
        }
    }

    return mat_max;
}

/// @brief Generate the alloc matrix, that contains the current resources
/// instances allocated for each task.
/// @param mat_alloc Pointer of the matrix already allocated to fill.
/// @param idx_map_task_struct Pointer to the array of index and tasks mapping.
/// @return The same pointer passed as first parameter.
static uint32_t **fill_alloc(uint32_t **mat_alloc,
                             task_struct *idx_map_task_struct[])
{
    size_t n = kernel_get_active_processes();
    // Loop on resources created.
    list_head *resource_it;
    list_for_each (resource_it, &r_list.head) {
        resource_t *resource = list_entry(resource_it, resource_t,
                                          resources_list);
        // Find the task with this resource assigned and take the instances num.
        for (size_t t_i = 0; t_i < n; t_i++) {
            if (idx_map_task_struct[t_i] == resource->assigned_task) {
                mat_alloc[t_i][resource->rid] = resource->assigned_instances;
            }
        }
    }

    return mat_alloc;
}

/// @brief Generate the need matrix, that contains the current resources
/// instances need of each task.
/// @param mat_need Pointer of the matrix already allocated to fill.
/// @param mat_max Pointer to the max matrix.
/// @param mat_alloc Pointer to the alloc matrix.
/// @return The same pointer passed as first parameter.
static uint32_t **fill_need(uint32_t **mat_need, uint32_t **mat_max,
        uint32_t **mat_alloc)
{
    // Calculate need[i][j] = max[i][j] - alloc[i][j].
    size_t n = kernel_get_active_processes();
    size_t m = kernel_get_active_resources();
    for (size_t i = 0; i < n; i++) {
        memcpy(mat_need[i],  mat_max[i],   m * sizeof(uint32_t));
        arr_sub(mat_need[i], mat_alloc[i], m);
    }
    return mat_need;
}

resource_t *resource_create(const char *category)
{
    // Check if current task can allocate a new resource.
    task_struct *current_task = kernel_get_current_process();
    size_t i = 0;
    while(current_task->resources[i] != NULL) {
        i++;

        if (i >= TASK_RESOURCE_MAX_AMOUNT) {
            return NULL;
        }
    }

    // Allocate new resource and initialize it.
    resource_t *new = kmalloc(sizeof(resource_t));
    memset(new, 0, sizeof(resource_t));
    new->rid = r_list.num_active++;
    new->category = category;

    // Current task is one of the tasks that need for this resource allocation.
    current_task->resources[i] = new;
    // The number of resource instances: for now always 1.
    new->n_instances = 1;

    // Initialize the list_head.
    list_head_init(&new->resources_list);
    list_head_add_tail(&new->resources_list, &r_list.head);

    return new;
}

void resource_init(resource_t *r)
{
    if (!r) return;
    r->assigned_task = NULL;
    r->assigned_instances = 0;
}

void resource_destroy(resource_t *r)
{
    if (!r) return;
    // Remove pointer of this resource from running processes.
    clean_resource_reference(r);
    // Remove this resource from resources list.
    list_head_del(&r->resources_list);
    kfree(r);
    r_list.num_active--;

    // Normalize resource ids.
    list_head *resource_it;
    size_t rid = 0;
    list_for_each (resource_it, &r_list.head) {
        resource_t *resource = list_entry(resource_it, resource_t,
                resources_list);
        if (resource != NULL) {
            resource->rid = rid;
        }
    }
}

void resource_assign(resource_t *r)
{
    if (!r) return;
    // Assign resource to current task.
    r->assigned_task = kernel_get_current_process();
    // Number of instances assigned, for now always 1.
    r->assigned_instances = 1;
}

void resource_deassign(resource_t *r)
{
    if (!r) return;
    r->assigned_task = NULL;
    r->assigned_instances = 0;
}

void init_deadlock_structures(uint32_t *arr_available, uint32_t **mat_max,
        uint32_t **mat_alloc, uint32_t **mat_need,
        task_struct *idx_map_task_struct[])
{
    reset_deadlock_structures(arr_available, mat_max, mat_alloc, idx_map_task_struct);
    compute_index_map_task_struct(idx_map_task_struct);
    fill_alloc(mat_alloc, idx_map_task_struct);
    fill_max(mat_max, idx_map_task_struct);
    fill_available(arr_available);
    fill_need(mat_need, mat_max, mat_alloc);
}

void reset_deadlock_structures(uint32_t *arr_available, uint32_t **mat_max,
        uint32_t **mat_alloc, task_struct *idx_map_task_struct[])
{
    size_t n = kernel_get_active_processes();
    size_t m = kernel_get_active_resources();

    // Clean idx_map_task_struct and rows of max and alloc.
    for (size_t t_i = 0; t_i < n; t_i++) {
        idx_map_task_struct[t_i] = NULL;
        memset(mat_alloc[t_i], 0, m * sizeof(uint32_t));
        memset(mat_max[t_i], 0, m * sizeof(uint32_t));
    }

    // Clean row of resources.
    memset(arr_available, 0, m * sizeof(uint32_t));
}

size_t kernel_get_active_resources()
{
    return r_list.num_active;
}

int32_t get_current_task_idx_from(task_struct *idx_map_task_struct[])
{
    size_t n = kernel_get_active_processes();
    int32_t ret = -1;
    for (size_t t_i = 0; t_i < n; t_i++) {
        if (idx_map_task_struct[t_i] == kernel_get_current_process()) {
            ret = t_i;
            break;
        }
    }
    return ret;
}
