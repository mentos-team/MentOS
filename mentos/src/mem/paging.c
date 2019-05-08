///                MentOS, The Mentoring Operating system project
/// @file paging.c
/// @brief Implementation of a memory paging management.
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "paging.h"
#include "zone_allocator.h"
#include "kheap.h"
#include "debug.h"
#include "assert.h"
#include "string.h"

bool_t paging_is_enabled()
{
	return false;
}

struct mm_struct *create_process_image(size_t stack_size)
{
	// Allocate the mm_struct.
	struct mm_struct *mm = kmalloc(sizeof(struct mm_struct));
	memset(mm, 0, sizeof(struct mm_struct));

	// Allocate the stack segment.
	mm->start_stack = create_segment(mm, stack_size);

	return mm;
}

uint32_t create_segment(struct mm_struct *mm, size_t size)
{
	// Allocate on kernel space the structure for the segment.
	struct vm_area_struct *new_segment = kmalloc(sizeof(struct vm_area_struct));

	// Allocate the space requested for the segment on user space.
	if (paging_is_enabled()) {
		assert(0 && "Paging must not be enabled");
	}

	unsigned int order = find_nearest_order_greater(size);
	uint32_t vm_start = __alloc_pages(GFP_HIGHUSER, order);

	// Update vm_area_struct info.
	new_segment->vm_start = vm_start;
	new_segment->vm_end = vm_start + (1 << order) * PAGE_SIZE;
	new_segment->vm_mm = mm;

	// Update memory descriptor list of vm_area_struct.
	new_segment->vm_next = mm->mmap;
	mm->mmap = new_segment;
	mm->mmap_cache = new_segment;

	// Update memory descriptor info.
	mm->map_count++;

	mm->total_vm += (1 << order);

	return vm_start;
}

void destroy_process_image(struct mm_struct *mm)
{
	assert(mm != NULL);

	// Free each segment inside mm.
	struct vm_area_struct *segment = mm->mmap;
	while (segment != NULL) {
		// Free the pages represented by the segment.
		if (paging_is_enabled()) {
			assert(0 && "Paging must not be enabled");
		}

		size_t size = segment->vm_end - segment->vm_start;
		unsigned int order = find_nearest_order_greater(size);

		free_pages(segment->vm_start, order);

		struct vm_area_struct *tmp = segment;
		segment = segment->vm_next;

		// Free the vm_area_struct.
		kfree(tmp);
	}

	// Free the mm_struct.
	kfree(mm);
}
