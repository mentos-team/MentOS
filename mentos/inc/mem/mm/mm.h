/// @file mm.h
/// @brief Process memory management.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "list_head.h"
#include "stdint.h"

/// @brief Memory Descriptor, used to store details about the memory of a user process.
typedef struct mm_struct {
    /// List of memory areas (vm_area_struct references).
    list_head_t mmap_list;
    /// Pointer to the last used memory area.
    struct vm_area_struct *mmap_cache;
    /// Pointer to the process's page directory.
    struct page_directory *pgd;
    /// Number of memory areas.
    int map_count;
    /// List of mm_structs.
    list_head_t mm_list;
    /// Start address of the code segment.
    uint32_t start_code;
    /// End address of the code segment.
    uint32_t end_code;
    /// Start address of the data segment.
    uint32_t start_data;
    /// End address of the data segment.
    uint32_t end_data;
    /// Start address of the heap.
    uint32_t start_brk;
    /// End address of the heap.
    uint32_t brk;
    /// Start address of the stack.
    uint32_t start_stack;
    /// Start address of the arguments.
    uint32_t arg_start;
    /// End address of the arguments.
    uint32_t arg_end;
    /// Start address of the environment variables.
    uint32_t env_start;
    /// End address of the environment variables.
    uint32_t env_end;
    /// Total number of mapped pages.
    unsigned int total_vm;
} mm_struct_t;

/// @brief Initializes the memory management system.
/// @return 0 on success, -1 on error.
int mm_init(void);

/// @brief Creates the main memory descriptor.
/// @param stack_size The size of the stack in byte.
/// @return The Memory Descriptor created.
mm_struct_t *mm_create_blank(size_t stack_size);

/// @brief Create a Memory Descriptor.
/// @param mmp The memory map to clone
/// @return The Memory Descriptor created.
mm_struct_t *mm_clone(mm_struct_t *mmp);

/// @brief Free Memory Descriptor with all the memory segment contained.
/// @param mm The Memory Descriptor to free.
/// @return Returns -1 on error, otherwise 0.
int mm_destroy(mm_struct_t *mm);
