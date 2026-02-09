/// @file mm.c
/// @brief Process memory management.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

// Setup the logging for this file (do this before any other include).
#include "sys/kernel_levels.h"           // Include kernel log levels.
#define __DEBUG_HEADER__ "[MM_STR]"      ///< Change header.
#define __DEBUG_LEVEL__  LOGLEVEL_NOTICE ///< Set log level.
#include "io/debug.h"                    // Include debugging functions.

#include "mem/alloc/slab.h"
#include "mem/mm/mm.h"
#include "mem/paging.h"
#include "string.h"

/// Cache for storing mm_struct.
static kmem_cache_t *mm_cache;
/// The mm_struct of the kernel.
static mm_struct_t main_mm;

int mm_init(void)
{
    // Create memory cache for managing mm_struct.
    mm_cache = KMEM_CREATE(mm_struct_t);
    if (!mm_cache) {
        pr_crit("Failed to create mm_cache.\n");
        return -1;
    }

    // Clean the memory management structure for the kernel.
    memset(&main_mm, 0, sizeof(mm_struct_t));

    return 0;
}

mm_struct_t *mm_get_main(void) { return &main_mm; }

mm_struct_t *mm_create_blank(size_t stack_size)
{
    // Allocate the mm_struct for the new process image.
    mm_struct_t *mm = kmem_cache_alloc(mm_cache, GFP_KERNEL);
    if (!mm) {
        pr_crit("Failed to allocate memory for mm_struct\n");
        return NULL;
    }

    // Initialize the allocated mm_struct to zero.
    memset(mm, 0, sizeof(mm_struct_t));

    // Initialize the list for memory management (mm) structures.
    // TODO(enrico): Use this field for process memory management.
    list_head_init(&mm->mm_list);

    // Get the main page directory.
    page_directory_t *main_pgd = paging_get_main_pgd();
    // Error handling: Failed to get the main page directory.
    if (!main_pgd) {
        pr_crit("Failed to get the main page directory\n");
        return NULL;
    }

    // Allocate a new page directory structure and copy the main page directory.
    page_directory_t *pdir_cpy = kmem_cache_alloc(pgdir_cache, GFP_KERNEL);
    if (!pdir_cpy) {
        pr_crit("Failed to allocate memory for page directory\n");
        // Free previously allocated mm_struct.
        kmem_cache_free(mm);
        return NULL;
    }

    // Initialize the allocated page_directory to zero.
    memcpy(pdir_cpy, main_pgd, sizeof(page_directory_t));

    // Assign the copied page directory to the mm_struct.
    mm->pgd = pdir_cpy;

    // Initialize the virtual memory areas list for the new process.
    list_head_init(&mm->mmap_list);

    // Allocate the stack segment.
    vm_area_struct_t *segment = vm_area_create(
        mm, PROCAREA_END_ADDR - stack_size, stack_size, MM_PRESENT | MM_RW | MM_USER, GFP_HIGHUSER);
    if (!segment) {
        pr_crit("Failed to create stack segment for new process\n");
        // Free page directory if allocation fails.
        kmem_cache_free(pdir_cpy);
        // Free mm_struct as well.
        kmem_cache_free(mm);
        return NULL;
    }

    // Update the start of the stack in the mm_struct.
    mm->start_stack = segment->vm_start;

    return mm;
}

mm_struct_t *mm_clone(mm_struct_t *mmp)
{
    // Check if the input mm_struct pointer is valid.
    if (!mmp) {
        pr_crit("Invalid source mm_struct pointer.\n");
        return NULL;
    }

    // Allocate the mm_struct for the new process image.
    mm_struct_t *mm = kmem_cache_alloc(mm_cache, GFP_KERNEL);
    if (!mm) {
        pr_crit("Failed to allocate memory for mm_struct\n");
        return NULL;
    }

    // Copy the contents of the source mm_struct to the new one.
    memcpy(mm, mmp, sizeof(mm_struct_t));

    // Get the main page directory.
    page_directory_t *main_pgd = paging_get_main_pgd();
    // Error handling: Failed to get the main page directory.
    if (!main_pgd) {
        pr_crit("Failed to get the main page directory\n");
        return NULL;
    }

    // Allocate a new page directory to avoid data races on page tables.
    page_directory_t *pdir_cpy = kmem_cache_alloc(pgdir_cache, GFP_KERNEL);
    if (!pdir_cpy) {
        pr_crit("Failed to allocate page directory for new process.\n");
        // Free the previously allocated mm_struct.
        kmem_cache_free(mm);
        return NULL;
    }

    // Initialize the new page directory by copying from the main directory.
    memcpy(pdir_cpy, main_pgd, sizeof(page_directory_t));

    // Assign the copied page directory to the mm_struct.
    mm->pgd = pdir_cpy;

    vm_area_struct_t *vm_area = NULL;

    // Reset the memory area list to prepare for cloning.
    list_head_init(&mm->mmap_list);
    mm->map_count = 0;
    mm->total_vm  = 0;

    // Clone each memory area from the source process to the new process.
    list_for_each_decl (it, &mmp->mmap_list) {
        vm_area = list_entry(it, vm_area_struct_t, vm_list);

        if (vm_area_clone(mm, vm_area, 0, GFP_HIGHUSER) < 0) {
            pr_crit("Failed to clone vm_area from source process.\n");
            // Free the previously allocated mm_struct.
            kmem_cache_free(mm);
            // Free the previously allocated page_directory.
            kmem_cache_free(pdir_cpy);
            return NULL;
        }
    }

    // Return the newly cloned mm_struct.
    return mm;
}

int mm_destroy(mm_struct_t *mm)
{
    // Check if the input mm_struct pointer is valid.
    if (!mm) {
        pr_crit("Invalid source mm_struct pointer.\n");
        return -1;
    }

    // Get the main page directory.
    page_directory_t *main_pgd = paging_get_main_pgd();
    // Error handling: Failed to get the main page directory.
    if (!main_pgd) {
        pr_crit("Failed to get the main page directory\n");
        return -1;
    }

    // Retrieve the current page directory.
    uint32_t current_paging_dir = (uint32_t)paging_get_current_pgd();
    if (current_paging_dir == 0) {
        pr_crit("Failed to retrieve the current paging directory.\n");
        return -1;
    }

    // Get the low memory page associated with the given mm_struct.
    page_t *lowmem_page = get_page_from_virtual_address((uint32_t)mm->pgd);
    if (!lowmem_page) {
        pr_crit("Failed to get low memory page from mm->pgd address: %p\n", (void *)mm->pgd);
        return -1;
    }

    // Step 2: Get the physical address from the low memory page.
    uint32_t mm_pgd_phys_addr = get_physical_address_from_page(lowmem_page);
    if (mm_pgd_phys_addr == 0) {
        pr_crit("Failed to get physical address from low memory page: %p.\n", lowmem_page);
        return -1;
    }

    // Compare the current page directory with the one associated with the process.
    if (current_paging_dir == mm_pgd_phys_addr) {
        // Switch to the main directory if they are the same.
        if (paging_switch_pgd(main_pgd) < 0) {
            pr_crit("Failed to switch to the main directory.\n");
            return -1;
        }
    }

    // Free each segment inside mm.
    vm_area_struct_t *segment = NULL;
    // Iterate through the list of memory areas.
    list_head_t *it           = mm->mmap_list.next;
    list_head_t *next;

    while (!list_head_empty(it)) {
        segment = list_entry(it, vm_area_struct_t, vm_list);

        // Save the pointer to the next element in the list.
        next = segment->vm_list.next;

        // Destroy the current virtual memory area. Return -1 on failure.
        if (vm_area_destroy(mm, segment) < 0) {
            pr_err("We failed to destroy the virtual memory area.");
            return -1; // Failed to destroy the virtual memory area.
        }

        // Move to the next element.
        it = next;
    }

    // Free all the page tables.
    for (int i = 0; i < 1024; i++) {
        page_dir_entry_t *entry = &mm->pgd->entries[i];
        // Check if the page table entry is present and not global.
        if (entry->present && !entry->global) {
            // Get the physical page for the page table.
            page_t *pgt_page = get_page_from_physical_address(entry->frame * PAGE_SIZE);
            if (!pgt_page) {
                pr_crit("Failed to get physical page for entry %d.\n", i);
                continue; // Skip to the next entry on error.
            }

            // Get the low memory address for the page table.
            uint32_t pgt_addr = get_virtual_address_from_page(pgt_page);
            if (pgt_addr == 0) {
                pr_crit("Failed to get low memory address for physical page %p.\n", (void *)pgt_page);
                continue; // Skip to the next entry on error.
            }

            // Free the page table.
            kmem_cache_free((void *)pgt_addr);

            pr_debug("Successfully freed page table for entry %d at address %p.\n", i, (void *)pgt_addr);
        }
    }

    // Free the page directory structure.
    kmem_cache_free((void *)mm->pgd);

    // Free the memory structure representing the process image.
    kmem_cache_free(mm);

    return 0; // Success.
}
