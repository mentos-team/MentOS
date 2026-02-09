///                MentOS, The Mentoring Operating system project
/// @file resource.h
/// @brief Resource definition header code.
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "process.h"
#include "stdint.h"
#include "types.h"
#include "list_head.h"

/// @brief Resource descriptor.
typedef struct resource {
    /// Resource index. The resources indexes has to be continuous: 0, 1, ... M.
    size_t rid;

    /// List head for tasks that share this resource.
    list_head resources_list;

    /// Number of instances of this resource. For now, always 1.
    size_t n_instances;

    /// The category of the resource (added for debug purpose).
    const char *category;

    /// If the resource has been assigned, it points to the task assigned,
    /// otherwise NULL.
    task_struct *assigned_task;

    /// Number of instances assigned to assigned task.
    size_t assigned_instances;

} resource_t;

/// @brief Structure that maintains information about resources currently
/// allocated in the system.
typedef struct resource_list {
    /// Number of queued resources.
    size_t num_active;

    /// Head of resources.
    list_head head;
} resource_list_t;

/// @brief Resource creation.
/// @param category Resource category string, used to group resources.
/// @return The pointer to the resource created.
resource_t *resource_create(const char *category);

/// @brief Resource initialization.
/// @param r Pointer to a resource created.
void resource_init(resource_t *r);

/// @brief Resource destruction.
/// @param r Pointer to a resource created.
void resource_destroy(resource_t *r);

/// @brief Assign the ownership of a resource to the current calling task.
/// @param r Pointer to a resource created.
void resource_assign(resource_t *r);

/// @brief Remove the ownership of a resource from the current calling task.
/// @param r Pointer to a resource created.
void resource_deassign(resource_t *r);

/// @brief Initialize deadlock prevention structures.
/// @param available    Array of resources instances currently available;
/// @param max          Matrix of the maximum resources instances that each
///                     task may require;
/// @param alloc        Matrix of current resources instances allocation of
///                     each task.
/// @param need         Matrix of current resources instances need of each task.
/// @param idx_map_task_struct Pointer to the array of index and tasks mapping.
void init_deadlock_structures(uint32_t *available, uint32_t **max,
        uint32_t **alloc, uint32_t **need,
        task_struct *idx_map_task_struct[]);

/// @brief Reset to zero deadlock prevention structures.
/// @param available    Array of resources instances currently available;
/// @param max          Matrix of the maximum resources instances that each
///                     task may require;
/// @param alloc        Matrix of current resources instances allocation of
///                     each task.
/// @param idx_map_task_struct Pointer to the array of index and tasks mapping.
/// There is no need to reset the need matrix because it has to be calculated
/// starting from max matrix, which is clean.
void reset_deadlock_structures(uint32_t *available, uint32_t **max,
        uint32_t **alloc, task_struct *idx_map_task_struct[]);

/// @brief Get the number of total resources allocated in the system.
/// @return The number of resources.
size_t kernel_get_active_resources();

/// @brief Return the index of the current task contained in the array of index
/// and processes mapping.
/// @param idx_map_task_struct Pointer to the array of index and tasks mapping.
/// @return The index that maps with the current task in the
/// idx_map_task_struct array, or -1 if not found the mapping task.
int32_t get_current_task_idx_from(task_struct *idx_map_task_struct[]);
