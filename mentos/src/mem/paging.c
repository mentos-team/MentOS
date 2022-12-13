/// @file paging.c
/// @brief Implementation of a memory paging management.
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

// Include the kernel log levels.
#include "sys/kernel_levels.h"
/// Change the header.
#define __DEBUG_HEADER__ "[PAGING]"
/// Set the log level.
#define __DEBUG_LEVEL__ LOGLEVEL_NOTICE

#include "mem/paging.h"
#include "descriptor_tables/isr.h"
#include "mem/vmem_map.h"
#include "mem/zone_allocator.h"
#include "mem/kheap.h"
#include "io/debug.h"
#include "assert.h"
#include "string.h"
#include "system/panic.h"

/// Cache for storing mm_struct.
kmem_cache_t *mm_cache;
/// Cache for storing vm_area_struct.
kmem_cache_t *vm_area_cache;
/// Cache for storing page directories.
kmem_cache_t *pgdir_cache;
/// Cache for storing page tables.
kmem_cache_t *pgtbl_cache;

/// The mm_struct of the kernel.
static mm_struct_t *main_mm;

/// @brief Structure for iterating page directory entries.
typedef struct page_iterator_s {
    /// Pointer to the entry.
    page_dir_entry_t *entry;
    /// Pointer to the page table.
    page_table_t *table;
    /// Page Frame Number (PFN).
    uint32_t pfn;
    /// Last PNF.
    uint32_t last_pfn;
    /// Contains MEMMAP_FLAGS flags.
    uint32_t flags;
} page_iterator_t;

/// @brief Structure for iterating page table entries.
typedef struct pg_iter_entry_s {
    /// Pointer to the page table entry.
    page_table_entry_t *entry;
    /// Page Frame Number (PFN).
    uint32_t pfn;
} pg_iter_entry_t;

page_directory_t *paging_get_main_directory()
{
    return main_mm->pgd;
}

/// @brief Switches paging directory, the pointer can be a lowmem address
void paging_switch_directory_va(page_directory_t *dir)
{
    page_t *page = get_lowmem_page_from_address((uintptr_t)dir);
    paging_switch_directory((page_directory_t *)get_physical_address_from_page(page));
}

void paging_flush_tlb_single(unsigned long addr)
{
    ASM("invlpg (%0)" ::"r"(addr)
        : "memory");
}

uint32_t create_vm_area(mm_struct_t *mm,
                        uint32_t virt_start,
                        size_t size,
                        uint32_t pgflags,
                        uint32_t gfpflags)
{
    // Allocate on kernel space the structure for the segment.
    vm_area_struct_t *new_segment = kmem_cache_alloc(vm_area_cache, GFP_KERNEL);

    uint32_t order = find_nearest_order_greater(virt_start, size);

    uint32_t phy_vm_start;

    if (pgflags & MM_COW) {
        pgflags &= ~(MM_PRESENT | MM_UPDADDR);
        phy_vm_start = 0;
    } else {
        pgflags |= MM_UPDADDR;
        page_t *page = _alloc_pages(gfpflags, order);
        phy_vm_start = get_physical_address_from_page(page);
    }

    mem_upd_vm_area(mm->pgd, virt_start, phy_vm_start, size, pgflags);

    uint32_t vm_start = virt_start;

    // Update vm_area_struct info.
    new_segment->vm_start = vm_start;
    new_segment->vm_end   = vm_start + size;
    new_segment->vm_mm    = mm;

    // Update memory descriptor list of vm_area_struct.
    list_head_insert_after(&new_segment->vm_list, &mm->mmap_list);
    mm->mmap_cache = new_segment;

    // Update memory descriptor info.
    mm->map_count++;

    mm->total_vm += (1U << order);

    return vm_start;
}

uint32_t clone_vm_area(mm_struct_t *mm, vm_area_struct_t *area, int cow, uint32_t gfpflags)
{
    vm_area_struct_t *new_segment = kmem_cache_alloc(vm_area_cache, GFP_KERNEL);
    memcpy(new_segment, area, sizeof(vm_area_struct_t));

    new_segment->vm_mm = mm;

    uint32_t size  = new_segment->vm_end - new_segment->vm_start;
    uint32_t order = find_nearest_order_greater(area->vm_start, size);

    if (!cow) {
        // If not copy-on-write, allocate directly the physical pages
        page_t *dst_page      = _alloc_pages(gfpflags, order);
        uint32_t phy_vm_start = get_physical_address_from_page(dst_page);

        // Then update the virtual memory map
        mem_upd_vm_area(mm->pgd, new_segment->vm_start, phy_vm_start, size,
                        MM_RW | MM_PRESENT | MM_UPDADDR | MM_USER);

        // Copy virtual memory of source area into dest area by using a virtual mapping
        virt_memcpy(mm, area->vm_start, area->vm_mm, area->vm_start, size);
    } else {
        // If copy-on-write, set the original pages as read-only
        mem_upd_vm_area(area->vm_mm->pgd, area->vm_start, 0, size,
                        MM_COW | MM_PRESENT | MM_USER);

        // Do a cow of the whole virtual memory area, handling fragmented physical memory
        // and set it as read-only
        mem_clone_vm_area(area->vm_mm->pgd,
                          mm->pgd,
                          area->vm_start,
                          new_segment->vm_start,
                          size,
                          MM_COW | MM_PRESENT | MM_UPDADDR | MM_USER);
    }

    // Update memory descriptor list of vm_area_struct.
    list_head_insert_after(&new_segment->vm_list, &mm->mmap_list);
    mm->mmap_cache = new_segment;

    // Update memory descriptor info.
    mm->map_count++;

    mm->total_vm += (1U << order);

    return 0;
}

static void __init_pagedir(page_directory_t *pdir)
{
    *pdir = (page_directory_t){ {0} };
}

static void __init_pagetable(page_table_t *ptable)
{
    *ptable = (page_table_t){ {0} };
}

void paging_init(boot_info_t *info)
{
    mm_cache      = KMEM_CREATE(mm_struct_t);
    vm_area_cache = KMEM_CREATE(vm_area_struct_t);

    pgdir_cache = KMEM_CREATE_CTOR(page_directory_t, __init_pagedir);
    pgtbl_cache = KMEM_CREATE_CTOR(page_table_t, __init_pagetable);

    main_mm = kmem_cache_alloc(mm_cache, GFP_KERNEL);

    main_mm->pgd = kmem_cache_alloc(pgdir_cache, GFP_KERNEL);

    uint32_t lowkmem_size = info->stack_end - info->kernel_start;

    // Map the first 1MB of memory with physical mapping to access video memory and other bios stuff
    mem_upd_vm_area(main_mm->pgd, 0, 0, 1024 * 1024, MM_RW | MM_PRESENT | MM_GLOBAL | MM_UPDADDR);

    mem_upd_vm_area(main_mm->pgd, info->kernel_start, info->kernel_phy_start, lowkmem_size,
                    MM_RW | MM_PRESENT | MM_GLOBAL | MM_UPDADDR);

    isr_install_handler(PAGE_FAULT, page_fault_handler, "page_fault_handler");

    paging_switch_directory_va(main_mm->pgd);
    paging_enable();
}

// Error code interpretation.
#define ERR_PRESENT  0x01 ///< Page not present.
#define ERR_RW       0x02 ///< Page is read only.
#define ERR_USER     0x04 ///< Page is privileged.
#define ERR_RESERVED 0x08 ///< Overwrote reserved bit.
#define ERR_INST     0x10 ///< Instruction fetch.

static inline void __set_pg_table_flags(page_table_entry_t *table, uint32_t flags)
{
    table->rw         = (flags & MM_RW) != 0;
    table->present    = (flags & MM_PRESENT) != 0;
    table->kernel_cow = (flags & MM_COW) != 0; // Store the cow/not cow status
    table->available  = 1;                     // Future kernel data 2 bits
    table->global     = (flags & MM_GLOBAL) != 0;
    table->user       = (flags & MM_USER) != 0;
}

/// @brief Prints stack frame data and calls kernel_panic.
/// @param f    The interrupt stack frame.
/// @param addr The faulting address.
static void __page_fault_panic(pt_regs *f, uint32_t addr)
{
    __asm__ __volatile__("cli");

    // Gather fault info and print to screen
    pr_err("Faulting address (cr2): 0x%p\n", addr);

    pr_err("EIP: 0x%p\n", f->eip);

    pr_err("Page fault: 0x%x\n", addr);

    pr_err("Possible causes: [ ");
    if (!(f->err_code & ERR_PRESENT))
        pr_err("Page not present ");
    if (f->err_code & ERR_RW)
        pr_err("Page is read only ");
    if (f->err_code & ERR_USER)
        pr_err("Page is privileged ");
    if (f->err_code & ERR_RESERVED)
        pr_err("Overwrote reserved bits ");
    if (f->err_code & ERR_INST)
        pr_err("Instruction fetch ");
    pr_err("]\n");
    dbg_print_regs(f);

    kernel_panic("Page fault!");

    // Make directory accessible
    //    main_mm->pgd->entries[addr/(1024*4096)].user = 1;
    //    main_directory->entries[addr/(1024*4096)]. = 1;

    __asm__ __volatile__("cli");
}

static void __page_handle_cow(page_table_entry_t *entry)
{
    // Check if the page is Copy On Write (COW).
    if (entry->kernel_cow) {
        // Set the entry is no longer COW.
        entry->kernel_cow = 0;
        // Check if the entry is not present (allocated).
        if (!entry->present) {
            // Allocate a new page.
            page_t *page = _alloc_pages(GFP_HIGHUSER, 0);
            // Clear the new page.
            uint32_t vaddr = virt_map_physical_pages(page, 1);
            memset((void *)vaddr, 0, PAGE_SIZE);
            // Unmap the virtual address.
            virt_unmap(vaddr);
            // Set it as current table entry frame.
            entry->frame = get_physical_address_from_page(page) >> 12U;
            // Set it as allocated.
            entry->present = 1;
            return;
        }
    }
    kernel_panic("Page not cow!");
}

static page_table_t *__mem_pg_entry_alloc(page_dir_entry_t *entry, uint32_t flags)
{
    if (!entry->present) {
        // Alloc page table if not present
        // Present should be always 1, to indicate that the page tables
        // have been allocated and allow lazy physical pages allocation
        entry->present   = 1;
        entry->rw        = 1;
        entry->global    = (flags & MM_GLOBAL) != 0;
        entry->user      = (flags & MM_USER) != 0;
        entry->accessed  = 0;
        entry->available = 1;
        return kmem_cache_alloc(pgtbl_cache, GFP_KERNEL);
    } else {
        entry->present |= (flags & MM_PRESENT) != 0;
        entry->rw |= (flags & MM_RW) != 0;

        // We should not remove a global flag from a page directory,
        // if this happens there is probably a bug in the kernel
        assert(!entry->global || (flags & MM_GLOBAL));

        entry->global &= (flags & MM_GLOBAL) != 0;
        entry->user |= (flags & MM_USER) != 0;
        return (page_table_t *)get_lowmem_address_from_page(
            get_page_from_physical_address(((uint32_t)entry->frame) << 12U));
    }
}

static inline void __set_pg_entry_frame(page_dir_entry_t *entry, page_table_t *table)
{
    page_t *table_page = get_lowmem_page_from_address((uint32_t)table);
    uint32_t phy_addr  = get_physical_address_from_page(table_page);
    entry->frame       = phy_addr >> 12u;
}

void page_fault_handler(pt_regs *f)
{
    // Here you will find the `Demand Paging` mechanism.
    // From `Understanding The Linux Kernel 3rd Edition`:
    //  The term demand paging denotes a dynamic memory allocation
    //  technique that consists of deferring page frame allocation
    //  until the last possible moment—until the process attempts
    //  to address a page that is not present in RAM, thus causing
    //  a Page Fault exception.

    // First, read the linear address that caused the Page Fault.
    // When the exception occurs, the CPU control unit stores that
    // value in the cr2 control register.
    uint32_t faulting_addr;
    __asm__ __volatile__("mov %%cr2, %0"
                         : "=r"(faulting_addr));
    // Get the physical address of the current page directory.
    uint32_t phy_dir = (uint32_t)paging_get_current_directory();
    // Get the page directory.
    page_directory_t *lowmem_dir = (page_directory_t *)get_lowmem_address_from_page(get_page_from_physical_address(phy_dir));
    // Get the directory entry.
    page_dir_entry_t *direntry = &lowmem_dir->entries[faulting_addr / (1024U * PAGE_SIZE)];
    // TODO: Panic only if page is in kernel memory, else abort process with sigsegv
    if (!direntry->present) {
        __page_fault_panic(f, faulting_addr);
    }
    // Get the physical address of the page table.
    uint32_t phy_table = direntry->frame << 12U;
    // Get the page table.
    page_table_t *lowmem_table = (page_table_t *)get_lowmem_address_from_page(get_page_from_physical_address(phy_table));
    // Get the entry inside the table that caused the fault.
    uint32_t table_index = (faulting_addr / PAGE_SIZE) % 1024U;
    // Get the corresponding page table entry.
    page_table_entry_t *entry = &lowmem_table->pages[table_index];
    // There was a page fault on a virtual mapped address,
    // so we must first update the original mapped page
    if (virtual_check_address(faulting_addr)) {
        // Get the original page table entry from the virtually mapped one.
        page_table_entry_t *orig_entry = (page_table_entry_t *)(*(uint32_t *)entry);
        // Check if the page is Copy on Write (CoW).
        __page_handle_cow(orig_entry);
        // Update the page table entry frame.
        entry->frame = orig_entry->frame;
        // Update the entry flags.
        __set_pg_table_flags(entry, MM_PRESENT | MM_RW | MM_GLOBAL | MM_COW | MM_UPDADDR);
    } else {
        // Check if the page is Copy on Write (CoW).
        __page_handle_cow(entry);
    }
    // Invalidate the page table entry.
    paging_flush_tlb_single(faulting_addr);
}

/// @brief Initialize a page iterator.
/// @param iter       The iterator to initialize.
/// @param pgd        The page directory to iterate.
/// @param addr_start The starting address.
/// @param size       The total amount we want to iterate.
/// @param flags      Allocation flags.
static void __pg_iter_init(page_iterator_t *iter,
                           page_directory_t *pgd,
                           uint32_t addr_start,
                           uint32_t size,
                           uint32_t flags)
{
    uint32_t start_pfn = addr_start / PAGE_SIZE;

    uint32_t end_pfn = (addr_start + size + PAGE_SIZE - 1) / PAGE_SIZE;

    uint32_t base_pgt = start_pfn / 1024;
    iter->entry       = pgd->entries + base_pgt;
    iter->pfn         = start_pfn;
    iter->last_pfn    = end_pfn;
    iter->flags       = flags;

    iter->table = __mem_pg_entry_alloc(iter->entry, flags);
    __set_pg_entry_frame(iter->entry, iter->table);
}

/// @brief Checks if the iterator has a next entry.
/// @param iter The iterator.
/// @return If we can continue the iteration.
static int __pg_iter_has_next(page_iterator_t *iter)
{
    return iter->pfn < iter->last_pfn;
}

/// @brief Moves the iterator to the next entry.
/// @param iter The itetator.
/// @return The iterator after moving to the next entry.
static pg_iter_entry_t __pg_iter_next(page_iterator_t *iter)
{
    pg_iter_entry_t result = {
        .entry = &iter->table->pages[iter->pfn % 1024],
        .pfn   = iter->pfn
    };

    if (++iter->pfn % 1024 == 0) {
        // Create a new page only if we haven't reached the end
        // The page directory is always aligned to page boundaries,
        // so we can easily know when we've skipped the last page by checking
        // if the address % PAGE_SIZE is equal to zero.
        if (iter->pfn != iter->last_pfn && ((uint32_t)++iter->entry) % 4096 != 0) {
            iter->table = __mem_pg_entry_alloc(iter->entry, iter->flags);
            __set_pg_entry_frame(iter->entry, iter->table);
        }
    }

    return result;
}

page_t *mem_virtual_to_page(page_directory_t *pgdir, uint32_t virt_start, size_t *size)
{
    uint32_t virt_pfn        = virt_start / PAGE_SIZE;
    uint32_t virt_pgt        = virt_pfn / 1024;
    uint32_t virt_pgt_offset = virt_pfn % 1024;

    page_t *pgd_page = mem_map + pgdir->entries[virt_pgt].frame;

    page_table_t *pgt_address = (page_table_t *)get_lowmem_address_from_page(pgd_page);

    uint32_t pfn = pgt_address->pages[virt_pgt_offset].frame;

    page_t *page = mem_map + pfn;

    // FIXME: handle unaligned page mapping
    // to return the correct to-block-end size
    // instead of 0 (1 page at a time)
    if (size) {
        uint32_t pfn_count   = 1U << page->bbpage.order;
        uint32_t bytes_count = pfn_count * PAGE_SIZE;
        *size                = min(*size, bytes_count);
    }

    return page;
}

void mem_upd_vm_area(page_directory_t *pgd,
                     uint32_t virt_start,
                     uint32_t phy_start,
                     size_t size,
                     uint32_t flags)
{
    page_iterator_t virt_iter;
    __pg_iter_init(&virt_iter, pgd, virt_start, size, flags);

    uint32_t phy_pfn = phy_start / PAGE_SIZE;

    while (__pg_iter_has_next(&virt_iter)) {
        pg_iter_entry_t it = __pg_iter_next(&virt_iter);
        if (flags & MM_UPDADDR) {
            it.entry->frame = phy_pfn++;
            // Flush the tlb to allow address update
            // TODO: Check if it's always needed (ex. when the pgdir is not the current one)
            paging_flush_tlb_single(it.pfn * PAGE_SIZE);
        }
        __set_pg_table_flags(it.entry, flags);
    }
}

void mem_clone_vm_area(page_directory_t *src_pgd,
                       page_directory_t *dst_pgd,
                       uint32_t src_start,
                       uint32_t dst_start,
                       size_t size,
                       uint32_t flags)
{
    page_iterator_t src_iter;
    page_iterator_t dst_iter;

    __pg_iter_init(&src_iter, src_pgd, src_start, size, flags);
    __pg_iter_init(&dst_iter, dst_pgd, dst_start, size, flags);

    while (__pg_iter_has_next(&src_iter) && __pg_iter_has_next(&dst_iter)) {
        pg_iter_entry_t src_it = __pg_iter_next(&src_iter);
        pg_iter_entry_t dst_it = __pg_iter_next(&dst_iter);

        if (src_it.entry->kernel_cow) {
            *(uint32_t *)dst_it.entry = (uint32_t)src_it.entry;
            // This is to make it clear that the page is not present,
            // can be omitted because the .entry address is aligned to 4 bytes boundary
            // so it's first two bytes are always zero
            dst_it.entry->present = 0;
        } else {
            dst_it.entry->frame = src_it.entry->frame;
            __set_pg_table_flags(dst_it.entry, flags);
        }

        // Flush the tlb to allow address update
        // TODO: Check if it's always needed (ex. when the pgdir is not the current one)
        paging_flush_tlb_single(dst_it.pfn * PAGE_SIZE);
    }
}

mm_struct_t *create_blank_process_image(size_t stack_size)
{
    // Allocate the mm_struct.
    mm_struct_t *mm = kmem_cache_alloc(mm_cache, GFP_KERNEL);
    memset(mm, 0, sizeof(mm_struct_t));

    list_head_init(&mm->mmap_list);

    // TODO: Use this field
    list_head_init(&mm->mm_list);

    page_directory_t *pdir_cpy = kmem_cache_alloc(pgdir_cache, GFP_KERNEL);
    memcpy(pdir_cpy, paging_get_main_directory(), sizeof(page_directory_t));

    mm->pgd = pdir_cpy;

    // Initialize vm areas list
    list_head_init(&mm->mmap_list);

    // Allocate the stack segment.
    mm->start_stack = create_vm_area(mm, PROCAREA_END_ADDR - stack_size, stack_size,
                                     MM_PRESENT | MM_RW | MM_USER | MM_COW, GFP_HIGHUSER);
    return mm;
}

mm_struct_t *clone_process_image(mm_struct_t *mmp)
{
    // Allocate the mm_struct.
    mm_struct_t *mm = kmem_cache_alloc(mm_cache, GFP_KERNEL);
    memcpy(mm, mmp, sizeof(mm_struct_t));

    // Initialize the process with the main directory, to avoid page tables data races.
    // Pages from the old process are copied/cow when segments are cloned
    page_directory_t *pdir_cpy = kmem_cache_alloc(pgdir_cache, GFP_KERNEL);
    memcpy(pdir_cpy, paging_get_main_directory(), sizeof(page_directory_t));

    mm->pgd = pdir_cpy;

    vm_area_struct_t *vm_area = NULL;

    // Reset vm areas to allow easy clone
    list_head_init(&mm->mmap_list);
    mm->map_count = 0;
    mm->total_vm  = 0;

    // Clone each memory area to the new process!
    list_head *it;
    list_for_each (it, &mmp->mmap_list) {
        vm_area = list_entry(it, vm_area_struct_t, vm_list);
        clone_vm_area(mm, vm_area, 0, GFP_HIGHUSER);
    }

    //
    //    // Allocate the stack segment.
    //    mm->start_stack = create_segment(mm, stack_size);

    return mm;
}

void destroy_process_image(mm_struct_t *mm)
{
    assert(mm != NULL);

    if ((uint32_t)paging_get_current_directory() == get_physical_address_from_page(get_lowmem_page_from_address((uint32_t)mm->pgd))) {
        paging_switch_directory_va(paging_get_main_directory());
    }

    // Free each segment inside mm.
    vm_area_struct_t *segment = NULL;

    list_head *it = mm->mmap_list.next;
    while (!list_head_empty(it)) {
        segment = list_entry(it, vm_area_struct_t, vm_list);

        size_t size = segment->vm_end - segment->vm_start;

        uint32_t area_start = segment->vm_start;

        while (size > 0) {
            size_t area_size = size;
            page_t *phy_page = mem_virtual_to_page(mm->pgd, area_start, &area_size);

            // If the pages are marked as copy-on-write, do not deallocate them!
            if (page_count(phy_page) > 1) {
                uint32_t order      = phy_page->bbpage.order;
                uint32_t block_size = 1UL << order;
                for (int i = 0; i < block_size; i++) {
                    page_dec(phy_page + i);
                }
            } else {
                __free_pages(phy_page);
            }

            size -= area_size;
            area_start += area_size;
        }
        // Free the vm_area_struct.

        // Delete segment from the mmap
        it = segment->vm_list.next;
        list_head_remove(&segment->vm_list);
        --mm->map_count;

        kmem_cache_free(segment);
    }

    // Free all the page tables
    for (int i = 0; i < 1024; i++) {
        page_dir_entry_t *entry = &mm->pgd->entries[i];
        if (entry->present && !entry->global) {
            page_t *pgt_page  = get_page_from_physical_address(entry->frame * PAGE_SIZE);
            uint32_t pgt_addr = get_lowmem_address_from_page(pgt_page);
            kmem_cache_free((void *)pgt_addr);
        }
    }
    kmem_cache_free((void *)mm->pgd);

    // Free the mm_struct.
    kmem_cache_free(mm);
}
