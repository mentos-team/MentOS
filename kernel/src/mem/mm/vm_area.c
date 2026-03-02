/// @file vm_area.c
/// @brief Segment-level VMA management.
/// @copyright (c) 2014-2025 This file is distributed under the MIT License.
/// See LICENSE.md for details.

// Setup the logging for this file (do this before any other include).
#include "sys/kernel_levels.h"           // Include kernel log levels.
#define __DEBUG_HEADER__ "[VMA   ]"      ///< Change header.
#define __DEBUG_LEVEL__  LOGLEVEL_DEBUG ///< Set log level.
#include "io/debug.h"                    // Include debugging functions.

#include "mem/mm/vm_area.h"

#include "list_head_algorithm.h"
#include "mem/alloc/slab.h"
#include "mem/mm/mm.h"
#include "mem/mm/vmem.h"
#include "mem/paging.h"
#include "string.h"

/// Cache for storing vm_area_struct.
static kmem_cache_t *vm_area_cache;

int vm_area_init(void)
{
    vm_area_cache = KMEM_CREATE(vm_area_struct_t);
    if (!vm_area_cache) {
        pr_crit("Failed to create vm_area_cache.\n");
        return -1;
    }
    return 0;
}

vm_area_struct_t *
vm_area_create(struct mm_struct *mm, uint32_t vm_start, size_t size, uint32_t pgflags, uint32_t gfpflags)
{
    // Validate inputs.
    if (!mm) {
        pr_crit("Invalid arguments: mm is NULL.");
        return NULL;
    }
    if (!vm_start) {
        pr_crit("Invalid arguments: vm_start is 0.");
        return NULL;
    }
    if (!size) {
        pr_crit("Invalid arguments: size is 0.");
        return NULL;
    }

    uint32_t vm_end;
    uint32_t phy_start;
    uint32_t order;
    vm_area_struct_t *segment;

    // Compute the end of the virtual memory area.
    vm_end = vm_start + size;

    // Check if the range is already occupied.
    if (vm_area_is_valid(mm, vm_start, vm_end) <= 0) {
        pr_crit("The virtual memory area range [%p, %p] is already in use.\n", vm_start, vm_end);
        return NULL;
    }

    // Allocate on kernel space the structure for the segment.
    segment = kmem_cache_alloc(vm_area_cache, GFP_KERNEL);
    if (!segment) {
        pr_crit("Failed to allocate memory for vm_area_struct\n");
        return NULL;
    }

    // Find the nearest order for the given memory size.
    order = find_nearest_order_greater(vm_start, size);

    if (pgflags & MM_COW) {
        // If the area is copy-on-write, clear the present and update address
        // flags.
        pgflags   = pgflags & ~(MM_PRESENT | MM_UPDADDR);
        phy_start = 0;
    } else {
        // Otherwise, set the update address flag and allocate physical pages.
        pgflags = pgflags | MM_UPDADDR;

        page_t *page = alloc_pages(gfpflags, order);
        if (!page) {
            pr_crit("Failed to allocate physical pages for vm_area at [%p, %p].\n", vm_start, vm_end);
            kmem_cache_free(segment);
            return NULL;
        }

        // Retrieve the physical address from the allocated page.
        phy_start = get_physical_address_from_page(page);
        if (!phy_start) {
            pr_crit("Failed to retrieve physical address for allocated page.\n");
            kmem_cache_free(segment);
            return NULL;
        }
    }

    // Update the virtual memory area in the page directory.
    if (mem_upd_vm_area(mm->pgd, vm_start, phy_start, size, pgflags) != 0) {
        pr_crit("Failed to update vm_area in page directory\n");
        kmem_cache_free(segment);
        return NULL;
    }

    // Update vm_area_struct info.
    segment->vm_start = vm_start;
    segment->vm_end   = vm_end;
    segment->vm_mm    = mm;

    // Insert the new segment into the memory descriptor's list of vm_area_structs.
    list_head_insert_after(&segment->vm_list, &mm->mmap_list);
    mm->mmap_cache = segment;

    // Sort the mmap_list to maintain order.
    list_head_sort(&mm->mmap_list, vm_area_compare);

    // Update memory descriptor info.
    mm->map_count++;
    mm->total_vm += (1U << order);

    // Return the created vm_area_struct.
    return segment;
}

uint32_t vm_area_clone(mm_struct_t *mm, vm_area_struct_t *area, int cow, uint32_t gfpflags)
{
    // Validate inputs.
    if (!mm) {
        pr_crit("Invalid arguments: mm is NULL.");
        return -1;
    }
    if (!area) {
        pr_crit("Invalid arguments: area is NULL.");
        return -1;
    }

    // Allocate a new vm_area_struct for the cloned area.
    vm_area_struct_t *new_segment = kmem_cache_alloc(vm_area_cache, GFP_KERNEL);
    if (!new_segment) {
        pr_crit("Failed to allocate memory for new vm_area_struct\n");
        return -1;
    }

    // Copy the content of the existing vm_area_struct to the new segment.
    memcpy(new_segment, area, sizeof(vm_area_struct_t));

    // Update the memory descriptor for the new segment.
    new_segment->vm_mm = mm;

    // Calculate the size and the nearest order for the new segment's memory allocation.
    uint32_t size  = new_segment->vm_end - new_segment->vm_start;
    uint32_t order = find_nearest_order_greater(area->vm_start, size);

    if (!cow) {
        // If not copy-on-write, allocate directly the physical pages
        page_t *dst_page = alloc_pages(gfpflags, order);
        if (!dst_page) {
            pr_crit("Failed to allocate physical pages for the new vm_area\n");
            // Free the newly allocated segment on failure.
            kmem_cache_free(new_segment);
            return -1;
        }

        uint32_t phy_vm_start = get_physical_address_from_page(dst_page);

        // Update the virtual memory map in the page directory.
        if (mem_upd_vm_area(
                mm->pgd, new_segment->vm_start, phy_vm_start, size, MM_RW | MM_PRESENT | MM_UPDADDR | MM_USER) < 0) {
            pr_crit("Failed to update virtual memory area in page directory\n");
            // Free the allocated pages on failure.
            free_pages(dst_page);
            // Free the newly allocated segment.
            kmem_cache_free(new_segment);
            return -1;
        }

        // Copy virtual memory from source area into destination area using a virtual mapping.
        vmem_memcpy(mm, area->vm_start, area->vm_mm, area->vm_start, size);
    } else {
        // If copy-on-write, set the original pages as read-only.
        if (mem_upd_vm_area(area->vm_mm->pgd, area->vm_start, 0, size, MM_COW | MM_PRESENT | MM_USER) < 0) {
            pr_crit("Failed to mark original pages as copy-on-write\n");
            // Free the newly allocated segment.
            kmem_cache_free(new_segment);
            return -1;
        }

        // Perform a COW of the whole virtual memory area, handling fragmented physical memory.
        if (mem_clone_vm_area(
                area->vm_mm->pgd, mm->pgd, area->vm_start, new_segment->vm_start, size,
                MM_COW | MM_PRESENT | MM_UPDADDR | MM_USER) < 0) {
            pr_crit("Failed to clone virtual memory area\n");
            // Free the newly allocated segment.
            kmem_cache_free(new_segment);
            return -1;
        }
    }

    // Update memory descriptor list of vm_area_struct.
    list_head_insert_after(&new_segment->vm_list, &mm->mmap_list);
    mm->mmap_cache = new_segment;

    // Update memory descriptor info.
    mm->map_count++;
    mm->total_vm += (1U << order);

    return 0;
}

int vm_area_destroy(mm_struct_t *mm, vm_area_struct_t *area)
{
    size_t area_total_size;
    size_t area_size;
    size_t area_start;
    uint32_t order;
    uint32_t block_size;
    page_t *phy_page;

    // Get the total size of the virtual memory area.
    area_total_size = area->vm_end - area->vm_start;

    // Get the starting address of the area.
    area_start = area->vm_start;

    // Free all the memory associated with the virtual memory area.
    while (area_total_size > 0) {
        area_size = area_total_size;

        // Translate the virtual address to the physical page.
        phy_page = mem_virtual_to_page(mm->pgd, area_start, &area_size);

        // If the page is not present (e.g., COW without backing), skip freeing and advance.
        if (!phy_page) {
            pr_info("Skipping non-present page for virtual address %p\n", (void *)area_start);
            area_size = PAGE_SIZE;
            if (area_size > area_total_size) {
                area_size = area_total_size;
            }
            area_total_size -= area_size;
            area_start += area_size;
            continue;
        }

        // If the pages are marked as copy-on-write, do not deallocate them.
        if (page_count(phy_page) > 1) {
            order      = phy_page->bbpage.order;
            block_size = 1UL << order;

            // Decrement the reference count for each page in the block.
            for (int i = 0; i < block_size; i++) {
                page_dec(phy_page + i);
            }
        } else {
            // If not copy-on-write, free the allocated pages.
            free_pages(phy_page);
        }

        // Update the remaining size and starting address for the next iteration.
        area_total_size -= area_size;
        area_start += area_size;
    }

    // Remove the segment from the memory map list.
    list_head_remove(&area->vm_list);

    // Free the memory allocated for the vm_area_struct.
    kmem_cache_free(area);

    // Decrement the counter for the number of memory-mapped areas.
    --mm->map_count;

    return 0;
}

int vm_area_is_valid(mm_struct_t *mm, uintptr_t vm_start, uintptr_t vm_end)
{
    // Check for a valid memory descriptor.
    if (!mm) {
        pr_crit("Invalid arguments: mm is NULL.\n");
        return -1;
    }
    if (vm_start >= vm_end) {
        pr_crit("Invalid arguments: vm_start >= vm_end (0x%p >= 0x%p).\n", vm_start, vm_end);
        return -1;
    }

    // Iterate through the list of memory areas to check for overlaps.
    vm_area_struct_t *area;
    list_for_each_prev_decl(it, &mm->mmap_list)
    {
        // Get the current area from the list entry.
        area = list_entry(it, vm_area_struct_t, vm_list);

        // Check if the area is NULL.
        if (!area) {
            pr_crit("Encountered a NULL area in the list.");
            return -1;
        }

        // Check if the new area overlaps with the current area.
        if ((vm_start > area->vm_start) && (vm_start < area->vm_end)) {
            pr_crit(
                "Overlap detected at start: %p <= %p <= %p", (void *)area->vm_start, (void *)vm_start,
                (void *)area->vm_end);
            return 0;
        }

        if ((vm_end > area->vm_start) && (vm_end < area->vm_end)) {
            pr_crit(
                "Overlap detected at end: %p <= %p <= %p", (void *)area->vm_start, (void *)vm_end,
                (void *)area->vm_end);
            return 0;
        }

        if ((vm_start < area->vm_start) && (vm_end > area->vm_end)) {
            pr_crit(
                "Wrap-around detected: %p <= (%p, %p) <= %p", (void *)vm_start, (void *)area->vm_start,
                (void *)area->vm_end, (void *)vm_end);
            return 0;
        }
    }

    // If no overlaps were found, return 1 to indicate the area is valid.
    return 1;
}

vm_area_struct_t *vm_area_find(mm_struct_t *mm, uint32_t vm_start)
{
    vm_area_struct_t *segment;

    // Iterate through the memory map list in reverse order to find the area.
    list_for_each_prev_decl(it, &mm->mmap_list)
    {
        // Get the current segment from the list entry.
        segment = list_entry(it, vm_area_struct_t, vm_list);

        // Assert that the segment is not NULL.
        assert(segment && "There is a NULL area in the list.");

        // Check if the starting address matches the requested vm_start.
        if (segment->vm_start == vm_start) {
            // Return the found segment.
            return segment;
        }
    }

    // If the area is not found, return NULL.
    return NULL;
}

int vm_area_search_free_area(mm_struct_t *mm, size_t length, uintptr_t *vm_start)
{
    // Check for a valid memory descriptor.
    if (!mm || !length || !vm_start) {
        pr_crit("Invalid arguments: mm or length or vm_start is NULL.");
        return -1;
    }

    vm_area_struct_t *area;
    vm_area_struct_t *prev_area;

    // Iterate through the list of memory areas in reverse order.
    list_for_each_prev_decl(it, &mm->mmap_list)
    {
        // Get the current area from the list entry.
        area = list_entry(it, vm_area_struct_t, vm_list);

        // Check if the current area is NULL.
        if (!area) {
            pr_crit("Encountered a NULL area in the list.");
            return -1;
        }

        // Check the previous segment if it exists.
        if (area->vm_list.prev != &mm->mmap_list) {
            prev_area = list_entry(area->vm_list.prev, vm_area_struct_t, vm_list);

            // Check if the previous area is NULL.
            if (!prev_area) {
                pr_crit("Encountered a NULL previous area in the list.");
                return -1;
            }

            // Compute the available space between the current area and the previous area.
            unsigned available_space = area->vm_start - prev_area->vm_end;

            // If the available space is sufficient for the requested length,
            // return the starting address.
            if (available_space >= length) {
                *vm_start = area->vm_start - length;
                return 0;
            }
        }
    }

    // If no suitable area was found, return 1 to indicate failure.
    return 1;
}

int vm_area_compare(const list_head_t *vma0, const list_head_t *vma1)
{
    // Retrieve the vm_area_struct from the list_head_t for vma0.
    vm_area_struct_t *_vma0 = list_entry(vma0, vm_area_struct_t, vm_list);
    // Retrieve the vm_area_struct from the list_head_t for vma1.
    vm_area_struct_t *_vma1 = list_entry(vma1, vm_area_struct_t, vm_list);
    // Compare the start address of vma0 with the end address of vma1.
    return _vma0->vm_start > _vma1->vm_end;
}
