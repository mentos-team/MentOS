/// @file page.c
/// @brief Defines the page structure.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

// Setup the logging for this file (do this before any other include).
#include "sys/kernel_levels.h"           // Include kernel log levels.
#define __DEBUG_HEADER__ "[PAGE  ]"      ///< Change header.
#define __DEBUG_LEVEL__  LOGLEVEL_NOTICE ///< Set log level.
#include "io/debug.h"                    // Include debugging functions.

#include "mem/mm/page.h"
#include "mem/paging.h"
#include "mem/alloc/zone_allocator.h"

uint32_t get_virtual_address_from_page(page_t *page)
{
    // Check for NULL page pointer. If it is NULL, print an error and return 0.
    if (!page) {
        pr_err("Invalid page pointer: NULL value provided.\n");
        return 0;
    }

    // Calculate the index of the page in the memory map.
    uint32_t page_index = page - memory.mem_map;

    // Check if the calculated page index is within valid bounds.
    if ((page_index < memory.page_index_min) || (page_index > memory.page_index_max)) {
        pr_err(
            "Page index %u is out of bounds. Valid range: %u to %u.\n", page_index, memory.page_index_min,
            memory.page_index_max - 1);
        return 0;
    }

    // Calculate the offset from the low memory base address.
    uint32_t offset = page_index - memory.page_index_min;

    // Calculate the corresponding low memory virtual address.
    uint32_t vaddr = memory.low_mem.virt_start + (offset * PAGE_SIZE);

    // Validate the computed virtual address.
    if (!is_valid_virtual_address(vaddr)) {
        pr_err("Computed virtual address 0x%p is invalid.\n", vaddr);
        return 0;
    }

    // Return the valid virtual address.
    return vaddr;
}

uint32_t get_physical_address_from_page(page_t *page)
{
    // Ensure the page pointer is not NULL. If it is NULL, print an error and return 0.
    if (!page) {
        pr_err("Invalid page pointer: NULL value provided.\n");
        return 0;
    }

    // Calculate the index of the page in the memory map.
    uint32_t page_index = page - memory.mem_map;

    // Check if the calculated page index is within valid bounds.
    if ((page_index < memory.page_index_min) || (page_index > memory.page_index_max)) {
        pr_err(
            "Page index %u is out of bounds. Valid range: %u to %u.\n", page_index, memory.page_index_min,
            memory.page_index_max - 1);
        return 0;
    }

    // Return the corresponding physical address by multiplying the index by the
    // page size.
    return page_index * PAGE_SIZE;
}

page_t *get_page_from_virtual_address(uint32_t vaddr)
{
    // Ensure it is a valid virtual address.
    if (!is_valid_virtual_address(vaddr)) {
        pr_crit("The provided address 0x%p is not a valid virtual address.\n", vaddr);
        return NULL;
    }

    // Calculate the offset from the low memory virtual base address.
    uint32_t offset = vaddr - memory.low_mem.virt_start;

    // Determine the index of the corresponding page structure in the memory map.
    uint32_t page_index = memory.page_index_min + (offset / PAGE_SIZE);

    // Check if the page index exceeds the memory map limit.
    if ((page_index < memory.page_index_min) || (page_index > memory.page_index_max)) {
        pr_err(
            "Page index %u is out of bounds. Valid range: %u to %u.\n", page_index, memory.page_index_min,
            memory.page_index_max - 1);
        return NULL;
    }

    // Return the pointer to the page structure.
    return memory.mem_map + page_index;
}

page_t *get_page_from_physical_address(uint32_t paddr)
{
    // Ensure the physical address is valid and aligned to page boundaries.
    if (paddr % PAGE_SIZE != 0) {
        pr_crit("Address must be page-aligned. Received address: 0x%08x\n", paddr);
        return NULL;
    }

    // Calculate the index of the page in the memory map.
    uint32_t page_index = paddr / PAGE_SIZE;

    // Check if the calculated page index is within valid bounds.
    if ((page_index < memory.page_index_min) || (page_index > memory.page_index_max)) {
        pr_err(
            "Page index %u is out of bounds. Valid range: %u to %u.\n", page_index, memory.page_index_min,
            memory.page_index_max - 1);
        return NULL;
    }

    // Return the pointer to the corresponding page structure in the memory map.
    return memory.mem_map + page_index;
}
