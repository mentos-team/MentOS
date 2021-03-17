///                MentOS, The Mentoring Operating system project
/// @file resource.c
/// @brief
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

resource_t *resource_create(const char *category)
{
    // Check if current task can allocate a new resource.
    struct task_struct *current_task = kernel_get_current_process();
    size_t i = 0;
    while(current_task->resources[i] != NULL) {
        i++;

        if (i >= TASK_RESOURCE_MAX_AMOUNT) {
            return NULL;
        }
    }

    // Allocate new resource.
    resource_t *new = kmalloc(sizeof(resource_t));
    memset(new, 0, sizeof(resource_t));
    new->rid = r_list.num_active++;
    new->category = category;

    // The task that create this resource is the current running task.
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
    r->assigned_task = NULL;
    r->assigned_instances = 0;
}

void resource_destroy(resource_t *r)
{
    clean_resource_reference(r);
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
    r->assigned_task = kernel_get_current_process();
    r->assigned_instances = 1;
}

void resource_deassign(resource_t *r)
{
    r->assigned_task = NULL;
    r->assigned_instances = 0;
}

size_t kernel_get_active_resources()
{
    return r_list.num_active;
}

void clean_resource_reference(resource_t *r)
{
    list_head *it;
    list_for_each (it, &runqueue.queue) {
        struct task_struct *entry = list_entry(it, struct task_struct,
                run_list);
        if (entry != NULL) {
            for (size_t r_i = 0; r_i < TASK_RESOURCE_MAX_AMOUNT; r_i++) {
                if (entry->resources[r_i] == r) {
                    entry->resources[r_i] = NULL;
                }
            }
        }
    }
}

void init_deadlock_structures(uint32_t **alloc, uint32_t **max,
        uint32_t *available, uint32_t **need,
        struct task_struct *idx_map_task_struct[])
{
    reset_deadlock_structures(alloc, max, available, idx_map_task_struct);
    compute_index_map_task_struct(idx_map_task_struct);
    fill_alloc(alloc, idx_map_task_struct);
    fill_max(max, idx_map_task_struct);
    fill_available(available);

    // Calculate need[i][j] = max[i][j] - alloc[i][j].
    size_t n = kernel_get_active_processes();
    size_t m = kernel_get_active_resources();
    for (size_t i = 0; i < n; i++)
    {
        memcpy(need[i],  max[i],   m * sizeof(uint32_t));
        arr_sub(need[i], alloc[i], m);
    }
}

void reset_deadlock_structures(uint32_t **alloc, uint32_t **max,
        uint32_t *available, struct task_struct *idx_map_task_struct[])
{
    size_t n = kernel_get_active_processes();
    size_t m = kernel_get_active_resources();

    // Clean idx_map_task_struct and rows of max and alloc.
    for (size_t t_i = 0; t_i < n; t_i++) {
        idx_map_task_struct[t_i] = NULL;
        memset(alloc[t_i], 0, m * sizeof(uint32_t));
        memset(max[t_i], 0, m * sizeof(uint32_t));
    }

    // Clean row of resources.
    memset(available, 0, m * sizeof(uint32_t));
}

struct task_struct **compute_index_map_task_struct(
        struct task_struct *idx_map_task_struct[])
{
    list_head *task_it;
    size_t t_i = 0;
    list_for_each (task_it, &runqueue.queue) {
        struct task_struct *task = list_entry(task_it, struct task_struct,
                run_list);
        if (task != NULL) {
            idx_map_task_struct[t_i] = task;
            t_i++;
        }
    }

    return idx_map_task_struct;
}

uint32_t **fill_alloc(uint32_t **alloc,
        struct task_struct *idx_map_task_struct[])
{
    size_t tasks_amount = kernel_get_active_processes();

    list_head *resource_it;
    list_for_each (resource_it, &r_list.head) {
        resource_t *resource = list_entry(resource_it, resource_t,
                resources_list);
        for (size_t t_i = 0; t_i < tasks_amount; t_i++) {
            if (idx_map_task_struct[t_i] == resource->assigned_task) {
                alloc[t_i][resource->rid] = resource->assigned_instances;
            }
        }
    }

    return alloc;
}

uint32_t **fill_max(uint32_t **max, struct task_struct *idx_map_task_struct[])
{
    size_t tasks_amount = kernel_get_active_processes();
    for (size_t t_i = 0; t_i < tasks_amount; t_i++) {
        struct task_struct *task = idx_map_task_struct[t_i];
        if (task != NULL) {
            // Find resources needed by task.
            for (size_t r_i = 0; r_i < kernel_get_active_resources(); r_i++) {
                if (task->resources[r_i] != NULL) {
                    max[t_i][r_i] = task->resources[r_i]->n_instances;
                }
            }
        }
    }

    return max;
}

uint32_t *fill_available(uint32_t *available)
{
    list_head *resource_it;
    list_for_each (resource_it, &r_list.head) {
        resource_t *resource = list_entry(resource_it, resource_t,
                resources_list);
        available[resource->rid] =
                resource->n_instances - resource->assigned_instances;
    }

    return available;
}