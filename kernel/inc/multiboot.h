/// @file   multiboot.h
/// @brief  Kernel helpers for the shared Multiboot structures.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "boot/multiboot.h"

/// @brief Return the first memory-map entry, or NULL when it was not supplied.
multiboot_memory_map_t *mmap_first_entry(multiboot_info_t *info);

/// @brief Return the first memory-map entry of the requested type.
multiboot_memory_map_t *mmap_first_entry_of_type(multiboot_info_t *info, uint32_t type);

/// @brief Return the memory-map entry after @p entry.
multiboot_memory_map_t *mmap_next_entry(multiboot_info_t *info, multiboot_memory_map_t *entry);

/// @brief Return the next memory-map entry of the requested type.
multiboot_memory_map_t *mmap_next_entry_of_type(multiboot_info_t *info, multiboot_memory_map_t *entry, uint32_t type);

/// @brief Return a printable name for a memory-map type.
char *mmap_type_name(multiboot_memory_map_t *entry);

/// @brief Find the first module in a Multiboot information block.
multiboot_module_t *first_module(multiboot_info_t *info);

/// @brief Find the module after @p mod.
multiboot_module_t *next_module(multiboot_info_t *info, multiboot_module_t *mod);

/// @brief Print the Multiboot information block through the kernel logger.
void dump_multiboot(multiboot_info_t *mbi);
