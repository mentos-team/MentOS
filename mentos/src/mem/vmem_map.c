/// @file vmem_map.c
/// @brief Virtual memory mapping routines.
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

// Include the kernel log levels.
#include "sys/kernel_levels.h"
/// Change the header.
#define __DEBUG_HEADER__ "[VMEM  ]"
/// Set the log level.
#define __DEBUG_LEVEL__ LOGLEVEL_NOTICE

#include "mem/vmem_map.h"
#include "string.h"
#include "system/panic.h"

/// Virtual addresses manager.
static virt_map_page_manager_t virt_default_mapping;

/// TODO: check.
#define VIRTUAL_MEMORY_PAGES_COUNT (VIRTUAL_MEMORY_SIZE_MB * 256)
/// TODO: check.
#define VIRTUAL_MAPPING_BASE (PROCAREA_END_ADDR + 0x38000000)
/// TODO: check.
#define VIRT_PAGE_TO_ADDRESS(page) ((((page)-virt_pages) * PAGE_SIZE) + VIRTUAL_MAPPING_BASE)
/// TODO: check.
#define VIRT_ADDRESS_TO_PAGE(addr) ((((addr)-VIRTUAL_MAPPING_BASE) / PAGE_SIZE) + virt_pages)

/// Array of virtual pages.
virt_map_page_t virt_pages[VIRTUAL_MEMORY_PAGES_COUNT];

void virt_init(void)
{
    buddy_system_init(
        &virt_default_mapping.bb_instance,
        "virt_manager",
        virt_pages,
        BBSTRUCT_OFFSET(virt_map_page_t, bbpage),
        sizeof(virt_map_page_t),
        VIRTUAL_MEMORY_PAGES_COUNT);

    page_directory_t *mainpgd = paging_get_main_directory();

    uint32_t start_virt_pfn     = VIRTUAL_MAPPING_BASE / PAGE_SIZE;
    uint32_t start_virt_pgt     = start_virt_pfn / 1024;
    uint32_t start_virt_tbl_idx = start_virt_pfn % 1024;

    uint32_t pfn_num = VIRTUAL_MEMORY_PAGES_COUNT;

    // Alloc all page tables inside the main directory, so they will be shared across
    // all page directories of processes
    for (uint32_t i = start_virt_pgt; i < 1024 && (pfn_num > 0); i++) {
        page_dir_entry_t *entry = mainpgd->entries + i;

        page_table_t *table;

        // Alloc virtual page table
        entry->present   = 1;
        entry->rw        = 0;
        entry->global    = 1;
        entry->user      = 0;
        entry->accessed  = 0;
        entry->available = 1;
        table            = kmem_cache_alloc(pgtbl_cache, GFP_KERNEL);

        uint32_t start_page = (i == start_virt_pgt) ? start_virt_tbl_idx : 0;

        for (uint32_t j = start_page; j < 1024 && (pfn_num > 0); j++, pfn_num--) {
            table->pages[j].frame   = 0;
            table->pages[j].rw      = 0;
            table->pages[j].present = 0;
            table->pages[j].global  = 1;
            table->pages[j].user    = 0;
        }

        page_t *table_page = get_lowmem_page_from_address((uint32_t)table);
        uint32_t phy_addr  = get_physical_address_from_page(table_page);
        entry->frame       = phy_addr >> 12u;
    }
}

static virt_map_page_t *_alloc_virt_pages(uint32_t pfn_count)
{
    int order              = find_nearest_order_greater(0, pfn_count << 12);
    virt_map_page_t *vpage = PG_FROM_BBSTRUCT(bb_alloc_pages(&virt_default_mapping.bb_instance, order), virt_map_page_t, bbpage);
    return vpage;
}

uint32_t virt_map_physical_pages(page_t *page, int pfn_count)
{
    virt_map_page_t *vpage = _alloc_virt_pages(pfn_count);
    if (!vpage)
        return 0;

    uint32_t virt_address = VIRT_PAGE_TO_ADDRESS(vpage);
    uint32_t phy_address  = get_physical_address_from_page(page);

    mem_upd_vm_area(paging_get_main_directory(), virt_address, phy_address,
                    pfn_count * PAGE_SIZE, MM_PRESENT | MM_RW | MM_GLOBAL | MM_UPDADDR);
    return virt_address;
}

virt_map_page_t *virt_map_alloc(uint32_t size)
{
    uint32_t pages_count = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    return _alloc_virt_pages(pages_count);
}

uint32_t virt_map_vaddress(mm_struct_t *mm, virt_map_page_t *vpage, uint32_t vaddr, uint32_t size)
{
    uint32_t start_map_virt_address = VIRT_PAGE_TO_ADDRESS(vpage);

    // Clone the source vaddr the the requested virtual memory portion
    mem_clone_vm_area(mm->pgd,
                      paging_get_main_directory(),
                      vaddr,
                      start_map_virt_address,
                      size,
                      MM_PRESENT | MM_RW | MM_GLOBAL | MM_UPDADDR);
    return start_map_virt_address;
}

int virtual_check_address(uint32_t addr)
{
    return addr >= VIRTUAL_MAPPING_BASE; // && addr < VIRTUAL_MAPPING_BASE + VIRTUAL_MEMORY_PAGES_COUNT * PAGE_SIZE;
}

void virt_unmap(uint32_t addr)
{
    virt_map_page_t *page = VIRT_ADDRESS_TO_PAGE(addr);
    virt_unmap_pg(page);
}

void virt_unmap_pg(virt_map_page_t *page)
{
    uint32_t addr = VIRT_PAGE_TO_ADDRESS(page);

    // Set all virtual pages as not present
    mem_upd_vm_area(paging_get_main_directory(), addr, 0,
                    (1 << page->bbpage.order) * PAGE_SIZE, MM_GLOBAL);

    // and avoiding unwanted memory accesses by the kernel
    bb_free_pages(&virt_default_mapping.bb_instance, &page->bbpage);
}

// FIXME: Check if this function should support unaligned page-boundaries copy
void virt_memcpy(mm_struct_t *dst_mm, uint32_t dst_vaddr, mm_struct_t *src_mm, uint32_t src_vaddr, uint32_t size)
{
    const uint32_t VMEM_BUFFER_SIZE = 65536;

    uint32_t buffer_size = min(VMEM_BUFFER_SIZE, size);

    virt_map_page_t *src_vpage = virt_map_alloc(size);
    virt_map_page_t *dst_vpage = virt_map_alloc(size);

    if (!src_vpage || !dst_vpage) {
        kernel_panic("Cannot copy virtual memory address, unable to reserve vmem!");
    }

    for (;;) {
        uint32_t src_map = virt_map_vaddress(src_mm, src_vpage, src_vaddr, buffer_size);
        uint32_t dst_map = virt_map_vaddress(dst_mm, dst_vpage, dst_vaddr, buffer_size);

        uint32_t cpy_size = min(buffer_size, size);

        memcpy((void *)dst_map, (void *)src_map, cpy_size);

        if (size <= buffer_size) {
            break;
        }

        size -= cpy_size;
        src_vaddr += cpy_size;
        dst_vaddr += cpy_size;
    }

    virt_unmap_pg(src_vpage);
    virt_unmap_pg(dst_vpage);
}
