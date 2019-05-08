///                MentOS, The Mentoring Operating system project
/// @file paging.h
/// @brief Implementation of a memory paging management.
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "zone_allocator.h"
#include "kernel.h"
#include "stddef.h"

/// Size of a page.
#define PAGE_SIZE 4096

/// @brief Virtual Memory Area.
/// Process segments.
struct vm_area_struct {
	/// Memory descriptor associated.
	struct mm_struct *vm_mm;

	/// Start address of the segment.
	uint32_t vm_start;

	/// End address of the segment.
	uint32_t vm_end;

	/// Next memory area of the list.
	struct vm_area_struct *vm_next;

	/// Permissions.
	pgprot_t vm_page_prot;

	/// Flags.
	unsigned short vm_flags;

	/// rbtree node.
	// struct rb_node vm_rb;
};

/// @brief Memory Descriptor.
/// Memory description of user process.
struct mm_struct {
	/// List of memory area.
	struct vm_area_struct *mmap;
	// /// rbtree of memory area.
	// struct rb_root mm_rb;

	/// Last memory area used.
	struct vm_area_struct *mmap_cache;

	// ///< Process page directory.
	// page_directory_t * pgd;

	/// Number of memory area.
	int map_count;

	/// List of mm_struct.
	list_head mmlist;

	/// CODE.
	uint32_t start_code, end_code;

	/// DATA.
	uint32_t start_data, end_data;

	/// HEAP.
	uint32_t start_brk, brk;

	/// STACK.
	uint32_t start_stack;

	/// ARGS.
	uint32_t arg_start, arg_end;

	/// ENVIRONMENT.
	uint32_t env_start, env_end;

	/// Number of mapped pages.
	unsigned int total_vm;
};

/// @brief Returns if paging is enabled.
bool_t paging_is_enabled();

/// @brief            Create a Memory Descriptor.
/// @param stack_size The size of the stack in byte.
/// @return           The Memory Descriptor created.
struct mm_struct *create_process_image(size_t stack_size);

/// @brief      Create a virtual memory area.
/// @param mm   The memory descriptor which will contain the new segment.
/// @param size The size of the segment.
/// @return     The virtual address of the starting point of the segment.
uint32_t create_segment(struct mm_struct *mm, size_t size);

/// @brief    Free Memory Descriptor with all the memory segment contained.
/// @param mm The Memory Descriptor to free.
void destroy_process_image(struct mm_struct *mm);

/*
 * /// @brief         Free a virtual memory area.
 * /// @param mm      The Memory Descriptor that contains the segment.
 * /// @param segment The segment to free.
 * void destroy_segment(struct mm_struct *mm, struct vm_area_struct *segment);
 */
