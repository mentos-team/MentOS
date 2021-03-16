///                MentOS, The Mentoring Operating system project
/// @file resource.h
/// @brief
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "process.h"
#include "stdint.h"
#include "types.h"
#include "list_head.h"


typedef struct resource {
    /// Resource index.
    size_t rid;

    /// Process ID of the task that created this resource.
    task_struct *created_by_task;

    /// List head for tasks that share this resource.
    list_head resources_list;

    /// Number of instances of this resource. For now, always 1.
    size_t n_instances;

    /// The category of the resource (added for debug purpose).
    const char *category;

    /// If the resource has been assigned, it points to the task assigned, otherwise NULL.
    task_struct *assigned_task;

    /// Number of instances assigned to assigned task.
    size_t assigned_instances;

} resource_t;

typedef struct resource_list {
    /// Number of queued resources.
    size_t num_active;

    /// Head of resources.
    list_head head;
} resource_list_t;

resource_t *resource_create(const char *category);
void resource_init(resource_t *r);
void resource_destroy(resource_t *r);
void resource_assign(resource_t *r);
void resource_deassign(resource_t *r);

void clean_resource_reference(resource_t *r);

void init_deadlock_structures(uint32_t **alloc, uint32_t **max, uint32_t *available, task_struct *idx_map_task_struct[]);
void reset_deadlock_structures(uint32_t **alloc, uint32_t **max, uint32_t *available, task_struct *idx_map_task_struct[]);
task_struct **compute_index_map_task_struct(task_struct *idx_map_task_struct[]);
uint32_t **fill_alloc(uint32_t **alloc, task_struct *idx_map_task_struct[]);
uint32_t **fill_max(uint32_t **max, task_struct *idx_map_task_struct[]);
uint32_t *fill_available(uint32_t *availables);