/// @file vmem.c
/// @brief Virtual memory mapping routines.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

// Setup the logging for this file (do this before any other include).
#include "sys/kernel_levels.h"           // Include kernel log levels.
#define __DEBUG_HEADER__ "[VMEM  ]"      ///< Change header.
#define __DEBUG_LEVEL__  LOGLEVEL_DEBUG ///< Set log level.
#include "io/debug.h"                    // Include debugging functions.

#include "mem/mm/vmem.h"
#include "string.h"
#include "system/panic.h"

/// Virtual addresses manager.
static virt_map_page_manager_t virt_default_mapping;

/// Size of the virtual memory.
#define VIRTUAL_MEMORY_SIZE (128 * M)

/// Number of virtual memory pages.
#define VIRTUAL_MEMORY_PAGES_COUNT (VIRTUAL_MEMORY_SIZE / PAGE_SIZE)

/// Base address for virtual memory mapping.
#define VIRTUAL_MAPPING_BASE (PROCAREA_END_ADDR + 0x28000000UL)

/// Converts a virtual page to its address.
#define VIRT_PAGE_TO_ADDRESS(page) ((((page) - virt_pages) * PAGE_SIZE) + VIRTUAL_MAPPING_BASE)

/// Converts an address to its corresponding virtual page.
#define VIRT_ADDRESS_TO_PAGE(addr) ((((addr) - VIRTUAL_MAPPING_BASE) / PAGE_SIZE) + virt_pages)

/// Array of virtual pages.
virt_map_page_t virt_pages[VIRTUAL_MEMORY_PAGES_COUNT];

int vmem_init(void)
{
    // Initialize the buddy system for virtual memory management.
    // This system manages free pages in the virtual memory area.
    buddy_system_init(
        &virt_default_mapping.bb_instance,        // Buddy system instance for virtual memory.
        "virt_manager",                           // Name for the buddy system.
        virt_pages,                               // Number of pages managed by the buddy system.
        BBSTRUCT_OFFSET(virt_map_page_t, bbpage), // Offset of the buddy system structure.
        sizeof(virt_map_page_t),                  // Size of each memory page.
        VIRTUAL_MEMORY_PAGES_COUNT);              // Total number of virtual memory pages.

    // Get the main page directory for the system.
    page_directory_t *main_pgd = paging_get_main_pgd();

    // Error handling: If the main page directory could not be retrieved, return failure.
    if (!main_pgd) {
        pr_crit("Failed to get the main page directory\n");
        return -1;
    }

    // Calculate the starting Page Frame Number (PFN), page table index, and
    // table index.
    // VIRTUAL_MAPPING_BASE is the base address of the virtual memory area.
    // Divide by PAGE_SIZE to calculate the starting page frame number.
    uint32_t start_virt_pfn = VIRTUAL_MAPPING_BASE / PAGE_SIZE;

    // Calculate the page table index (1024 entries per page table in a 32-bit
    // system).
    uint32_t start_virt_pgt = start_virt_pfn / 1024;

    // Calculate the table index for the specific page inside the page table.
    uint32_t start_virt_tbl_idx = start_virt_pfn % 1024;

    // Initialize the number of pages to allocate based on
    // VIRTUAL_MEMORY_PAGES_COUNT.
    uint32_t pfn_num = VIRTUAL_MEMORY_PAGES_COUNT;

    // Allocate all page tables inside the main directory, so they will be
    // shared across all page directories of processes.
    page_dir_entry_t *entry;
    page_table_t *table;
    for (uint32_t i = start_virt_pgt; i < 1024 && (pfn_num > 0); i++) {
        // Get the page directory entry for the current page table index.
        entry = main_pgd->entries + i;

        // Set up the page directory entry.
        entry->present   = 1; // Mark the entry as present
        entry->rw        = 0; // Read-only
        entry->global    = 1; // Global page
        entry->user      = 0; // Kernel mode
        entry->accessed  = 0; // Not accessed
        entry->available = 1; // Available for system use

        // Allocate a new page table.
        table = kmem_cache_alloc(pgtbl_cache, GFP_KERNEL);
        // Error handling: failed to allocate page table.
        if (!table) {
            pr_crit("Failed to allocate page table\n");
            return -1;
        }

        // Determine the starting page index for the current page table. If this
        // is the first table, start from the previously calculated index.
        uint32_t start_page = (i == start_virt_pgt) ? start_virt_tbl_idx : 0;

        // Initialize the pages within the page table.
        for (uint32_t j = start_page; j < 1024 && (pfn_num > 0); j++, pfn_num--) {
            table->pages[j].frame   = 0; // No frame allocated
            table->pages[j].rw      = 0; // Read-only
            table->pages[j].present = 0; // Not present
            table->pages[j].global  = 1; // Global page
            table->pages[j].user    = 0; // Kernel mode
        }

        // Get the physical address of the allocated page table.
        page_t *table_page = get_page_from_virtual_address((uint32_t)table);
        // Error handling: failed to get low memory page from address.
        if (!table_page) {
            pr_crit("Failed to get low memory page from address\n");
            return -1;
        }

        // Get the physical address.
        uint32_t phy_addr = get_physical_address_from_page(table_page);

        // Set the physical frame address in the page directory entry.
        entry->frame = phy_addr >> 12U;
    }

    return 0;
}

/// @brief Allocates a virtual page, given the page frame count.
/// @param pfn_count the page frame count.
/// @return pointer to the virtual page.
static virt_map_page_t *_alloc_virt_pages(uint32_t pfn_count)
{
    // Find the nearest order greater than or equal to the page frame count.
    unsigned order = find_nearest_order_greater(0, pfn_count << 12);

    // Allocate pages from the buddy system.
    bb_page_t *bbpage = bb_alloc_pages(&virt_default_mapping.bb_instance, order);
    // Error handling: failed to allocate pages from the buddy system.
    if (!bbpage) {
        pr_crit("Failed to allocate pages from the buddy system\n");
        return NULL;
    }

    // Convert the buddy system page to a virtual map page.
    virt_map_page_t *vpage = PG_FROM_BBSTRUCT(bbpage, virt_map_page_t, bbpage);
    // Error handling: failed to convert from buddy system page to virtual map page.
    if (!vpage) {
        pr_emerg("Failed to convert from buddy system page to virtual map page.\n");
        return NULL;
    }

    return vpage;
}

uint32_t vmem_map_physical_pages(page_t *page, int pfn_count)
{
    // Allocate virtual pages for the given page frame count.
    virt_map_page_t *vpage = _alloc_virt_pages(pfn_count);
    // Error handling: failed to allocate virtual pages.
    if (!vpage) {
        pr_crit("Failed to allocate virtual pages\n");
        return 0;
    }

    // Convert the virtual page to its corresponding virtual address.
    uint32_t vaddr = VIRT_PAGE_TO_ADDRESS(vpage);

    if (!is_valid_virtual_address(vaddr)) {
        pr_crit(
            "The virtual address 0x%p associated with the virtual page 0x%p is "
            "not valid.\n",
            vaddr, vpage);
        return 0;
    }

    // Get the physical address of the given page.
    uint32_t phy_address = get_physical_address_from_page(page);

    // Get the main page directory.
    page_directory_t *main_pgd = paging_get_main_pgd();
    // Error handling: Failed to get the main page directory.
    if (!main_pgd) {
        pr_crit("Failed to get the main page directory\n");
        return 0;
    }

    // Update the virtual memory area with the new mapping.
    mem_upd_vm_area(main_pgd, vaddr, phy_address, pfn_count * PAGE_SIZE, MM_PRESENT | MM_RW | MM_GLOBAL | MM_UPDADDR);

    return vaddr;
}

virt_map_page_t *vmem_map_alloc_virtual(uint32_t size)
{
    // Calculate the number of pages required to cover the given size.
    uint32_t pages_count = (size + PAGE_SIZE - 1) / PAGE_SIZE;

    // Allocate the required number of virtual pages.
    virt_map_page_t *vpages = _alloc_virt_pages(pages_count);
    // Error handling: failed to allocate virtual pages.
    if (!vpages) {
        pr_crit("Failed to allocate virtual pages for size %u\n", size);
        return NULL;
    }

    // Return the pointer to the allocated virtual pages.
    return vpages;
}

uint32_t vmem_map_virtual_address(mm_struct_t *mm, virt_map_page_t *vpage, uint32_t vaddr, uint32_t size)
{
    // Ensure the memory management structure and page directory are valid.
    if (!mm || !mm->pgd) {
        pr_crit("Invalid memory management structure or page directory\n");
        return 0;
    }

    // Convert the virtual map page to its corresponding virtual address.
    uint32_t start_map_virt_address = VIRT_PAGE_TO_ADDRESS(vpage);

    // Validate the start address of the virtual map.
    if (!is_valid_virtual_address(start_map_virt_address)) {
        pr_crit(
            "Invalid start virtual address for mapping 0x%08x, associated "
            "virtual page: 0x%p\n",
            start_map_virt_address, vpage);
        return 0;
    }

    // Get the main page directory.
    page_directory_t *main_pgd = paging_get_main_pgd();
    // Error handling: Failed to get the main page directory.
    if (!main_pgd) {
        pr_crit("Failed to get the main page directory\n");
        return 0;
    }

    // Clone the source vaddr the the requested virtual memory portion.
    mem_clone_vm_area(
        mm->pgd, main_pgd, vaddr, start_map_virt_address, size, MM_PRESENT | MM_RW | MM_GLOBAL | MM_UPDADDR);

    // Return the starting virtual address of the mapped area.
    return start_map_virt_address;
}

int vmem_unmap_virtual_address(uint32_t addr)
{
    // Ensure it is a valid virtual address.
    if (!is_valid_virtual_address(addr)) {
        pr_crit("The provided address 0x%p is not a valid virtual address.\n", addr);
        return -1;
    }

    // Convert the virtual address to its corresponding virtual map page.
    virt_map_page_t *page = VIRT_ADDRESS_TO_PAGE(addr);

    // Error handling: ensure the page is valid.
    if (!page) {
        pr_crit("Failed to convert address %u to virtual map page\n", addr);
        return -1;
    }

    // Unmap the virtual map page.
    vmem_unmap_virtual_address_page(page);

    return 0;
}

int vmem_unmap_virtual_address_page(virt_map_page_t *page)
{
    // Error handling: ensure the page is valid.
    if (!page) {
        pr_crit("Invalid virtual map page\n");
        return -1;
    }

    // Convert the virtual map page to its corresponding virtual address.
    uint32_t addr = VIRT_PAGE_TO_ADDRESS(page);

    // Get the main page directory.
    page_directory_t *main_pgd = paging_get_main_pgd();
    // Error handling: Failed to get the main page directory.
    if (!main_pgd) {
        pr_crit("Failed to get the main page directory\n");
        return -1;
    }

    // Set all virtual pages as not present to avoid unwanted memory accesses by the kernel.
    mem_upd_vm_area(main_pgd, addr, 0, (1 << page->bbpage.order) * PAGE_SIZE, MM_GLOBAL);

    // Free the pages in the buddy system.
    bb_free_pages(&virt_default_mapping.bb_instance, &page->bbpage);

    return 0;
}

// FIXME: Check if this function should support unaligned page-boundaries copy
void vmem_memcpy(mm_struct_t *dst_mm, uint32_t dst_vaddr, mm_struct_t *src_mm, uint32_t src_vaddr, uint32_t size)
{
    // Buffer size for copying.
    const uint32_t VMEM_BUFFER_SIZE = 65536;

    // Determine the buffer size to use for copying.
    uint32_t buffer_size = min(VMEM_BUFFER_SIZE, size);

    // Allocate virtual pages for the source and destination.
    virt_map_page_t *src_vpage = vmem_map_alloc_virtual(size);
    virt_map_page_t *dst_vpage = vmem_map_alloc_virtual(size);

    // Error handling: ensure both source and destination virtual pages are allocated.
    if (!src_vpage || !dst_vpage) {
        kernel_panic("Cannot copy virtual memory address, unable to reserve vmem!");
    }

    // Loop to copy memory in chunks.
    for (;;) {
        // Map the source and destination virtual addresses to the allocated
        // virtual pages.
        uint32_t src_map = vmem_map_virtual_address(src_mm, src_vpage, src_vaddr, buffer_size);
        uint32_t dst_map = vmem_map_virtual_address(dst_mm, dst_vpage, dst_vaddr, buffer_size);

        // Determine the size to copy in this iteration.
        uint32_t cpy_size = min(buffer_size, size);

        // Perform the memory copy.
        memcpy((void *)dst_map, (void *)src_map, cpy_size);

        // Check if the entire size has been copied.
        if (size <= buffer_size) {
            break;
        }

        // Update the remaining size and addresses for the next iteration.
        size -= cpy_size;
        src_vaddr += cpy_size;
        dst_vaddr += cpy_size;
    }

    // Unmap the allocated virtual pages.
    vmem_unmap_virtual_address_page(src_vpage);
    vmem_unmap_virtual_address_page(dst_vpage);
}
