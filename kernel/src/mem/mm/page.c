/// @file page.c
/// @brief Defines the page structure.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

// Setup the logging for this file (do this before any other include).
#include "sys/kernel_levels.h"           // Include kernel log levels.
#define __DEBUG_HEADER__ "[PAGE  ]"      ///< Change header.
#define __DEBUG_LEVEL__  LOGLEVEL_DEBUG ///< Set log level.
#include "io/debug.h"                    // Include debugging functions.

#include "mem/alloc/zone_allocator.h"
#include "mem/mm/page.h"
#include "mem/paging.h"

static int use_bootstrap_mapping = 1;

void page_set_bootstrap_mapping(int enabled) { use_bootstrap_mapping = enabled ? 1 : 0; }

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

    // Calculate the physical address from the page index.
    uint32_t paddr = page_index * PAGE_SIZE;
    uint32_t vaddr;

    // During early paging setup, use the boot linear mapping for lowmem.
    if (use_bootstrap_mapping &&
        (paddr >= memory.kernel_mem.start_addr) && (paddr < memory.low_mem.end_addr)) {
        vaddr = memory.kernel_mem.virt_start + (paddr - memory.kernel_mem.start_addr);
    } else {
        // Determine which zone the page belongs to and calculate virtual address.
        if ((paddr >= memory.boot_low_mem.start_addr) && (paddr < memory.boot_low_mem.end_addr)) {
            // Page is in boot-time lowmem region (mem_map/page_data gap).
            uint32_t offset = paddr - memory.boot_low_mem.start_addr;
            vaddr           = memory.boot_low_mem.virt_start + offset;
        } else if ((paddr >= memory.dma_mem.start_addr) && (paddr < memory.dma_mem.end_addr)) {
            // Page is in DMA zone.
            uint32_t offset = paddr - memory.dma_mem.start_addr;
            vaddr           = memory.dma_mem.virt_start + offset;
        } else if ((paddr >= memory.low_mem.start_addr) && (paddr < memory.low_mem.end_addr)) {
            // Page is in Normal (low_mem) zone.
            uint32_t offset = paddr - memory.low_mem.start_addr;
            vaddr           = memory.low_mem.virt_start + offset;
        } else if ((paddr >= memory.high_mem.start_addr) && (paddr < memory.high_mem.end_addr)) {
            // Page is in HighMem zone - no permanent mapping exists.
            // HighMem pages must be temporarily mapped via kmap() before use.
            pr_err("HighMem page (paddr 0x%08x) has no permanent virtual mapping. Use kmap().\n", paddr);
            return 0;
        } else if ((paddr >= memory.kernel_mem.start_addr) && (paddr < memory.kernel_mem.end_addr)) {
            // Page is in kernel region.
            uint32_t offset = paddr - memory.kernel_mem.start_addr;
            vaddr           = memory.kernel_mem.virt_start + offset;
        } else {
            pr_err("Physical address 0x%08x (page index %u) does not belong to any known memory zone.\n", paddr, page_index);
            pr_err("  DMA: 0x%08x-0x%08x, Normal: 0x%08x-0x%08x, HighMem: 0x%08x-0x%08x\n", memory.dma_mem.start_addr, memory.dma_mem.end_addr, memory.low_mem.start_addr, memory.low_mem.end_addr, memory.high_mem.start_addr, memory.high_mem.end_addr);
            return 0;
        }
    }

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

    // Return the corresponding physical address by multiplying the index by the page size.
    uint32_t paddr = page_index * PAGE_SIZE;
    return paddr;
}

page_t *get_page_from_virtual_address(uint32_t vaddr)
{
    // Ensure it is a valid virtual address.
    if (!is_valid_virtual_address(vaddr)) {
        pr_crit("The provided address 0x%p is not a valid virtual address.\n", vaddr);
        return NULL;
    }

    uint32_t offset;
    uint32_t page_index;

    // During early paging setup, use the boot linear mapping for lowmem.
    if (use_bootstrap_mapping) {
        uint32_t boot_lowmem_size = memory.low_mem.end_addr - memory.kernel_mem.start_addr;
        if ((vaddr >= memory.kernel_mem.virt_start) &&
            (vaddr < (memory.kernel_mem.virt_start + boot_lowmem_size))) {
            offset     = vaddr - memory.kernel_mem.virt_start;
            page_index = (memory.kernel_mem.start_addr / PAGE_SIZE) + (offset / PAGE_SIZE);
            goto page_index_ready;
        }
    }

    // Check which zone the virtual address belongs to.
    if ((vaddr >= memory.boot_low_mem.virt_start) && (vaddr < memory.boot_low_mem.virt_end)) {
        // Address is in boot-time lowmem region.
        offset     = vaddr - memory.boot_low_mem.virt_start;
        page_index = (memory.boot_low_mem.start_addr / PAGE_SIZE) + (offset / PAGE_SIZE);
    } else if ((vaddr >= memory.dma_mem.virt_start) && (vaddr < memory.dma_mem.virt_end)) {
        // Address is in DMA zone.
        offset     = vaddr - memory.dma_mem.virt_start;
        page_index = (memory.dma_mem.start_addr / PAGE_SIZE) + (offset / PAGE_SIZE);
    } else if ((vaddr >= memory.low_mem.virt_start) && (vaddr < memory.low_mem.virt_end)) {
        // Address is in Normal (low_mem) zone.
        offset     = vaddr - memory.low_mem.virt_start;
        page_index = (memory.low_mem.start_addr / PAGE_SIZE) + (offset / PAGE_SIZE);
    } else if ((vaddr >= memory.kernel_mem.virt_start) && (vaddr < memory.kernel_mem.virt_end)) {
        // Address is in kernel region (bootloader-mapped kernel code and structures).
        offset     = vaddr - memory.kernel_mem.virt_start;
        page_index = (memory.kernel_mem.start_addr / PAGE_SIZE) + (offset / PAGE_SIZE);
    } else {
        pr_err("Virtual address 0x%p does not belong to any known memory zone or region.\n", vaddr);
        return NULL;
    }

page_index_ready:
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
