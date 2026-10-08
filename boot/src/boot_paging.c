/// @file boot_paging.c
/// @brief Temporary page tables used while entering the kernel.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "boot_paging_ops.h"

#include "boot_bits.h"
#include "boot_console.h"
#include "boot_cpu.h"

static void boot_setup_pages(boot_page_directory_t *directory, boot_page_table_t *tables, uint32_t virtual_page,
                             uint32_t physical_page, uint32_t page_count)
{
    uint32_t base_table = virtual_page / BOOT_PAGE_ENTRIES;
    uint32_t base_entry = virtual_page % BOOT_PAGE_ENTRIES;
    uint32_t page_offset = 0;

    for (uint32_t table_index = base_table; table_index < BOOT_PAGE_ENTRIES && page_count; ++table_index) {
        boot_page_table_t *table = &tables[table_index];
        uint32_t entry_start = (table_index == base_table) ? base_entry : 0;

        for (uint32_t entry = entry_start; entry < BOOT_PAGE_ENTRIES && page_count; ++entry, --page_count) {
            table->pages[entry].frame = physical_page + page_offset++;
            table->pages[entry].rw = 1;
            table->pages[entry].present = 1;
            table->pages[entry].global = 0;
            table->pages[entry].user = 0;
        }
        directory->entries[table_index].rw = 1;
        directory->entries[table_index].present = 1;
        directory->entries[table_index].available = 1;
        directory->entries[table_index].frame = ((uint32_t)table) >> PAGE_SHIFT;
    }
}

void boot_paging_setup(const boot_info_t *info, boot_page_directory_t *directory, boot_page_table_t *tables)
{
    uint32_t kernel_physical_page = info->kernel_phy_start >> PAGE_SHIFT;
    uint32_t kernel_virtual_page = info->kernel_start >> PAGE_SHIFT;
    uint32_t last_physical_page = (info->lowmem_phy_end - 1U) >> PAGE_SHIFT;
    uint32_t page_count = last_physical_page - kernel_physical_page + 1U;

    boot_setup_pages(directory, tables, 0, 0, last_physical_page);
    boot_setup_pages(directory, tables, kernel_virtual_page, kernel_physical_page, page_count);
}

void boot_paging_protect_stack_guard(const boot_info_t *info, boot_page_directory_t *directory,
                                     boot_page_table_t *tables)
{
    uint32_t guard_address = info->stack_base - info->stack_size;
    uint32_t directory_index = guard_address >> 22U;
    uint32_t table_index = (guard_address >> PAGE_SHIFT) & 0x3FFU;

    if (!directory->entries[directory_index].present) {
        boot_console_puts("[bootloader] Kernel stack guard directory is not mapped.\n");
        return;
    }
    tables[directory_index].pages[table_index].present = 0;
}

void boot_paging_enable(void)
{
    boot_set_cr4(boot_bit_clear(boot_get_cr4(), BOOT_CR4_PSE));
    boot_set_cr0(boot_bit_set(boot_get_cr0(), BOOT_CR0_PG));
}

void boot_paging_switch_directory(boot_page_directory_t *directory)
{
    boot_set_cr3((uintptr_t)directory);
}
