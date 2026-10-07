/// @file boot.c
/// @brief Bootloader.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "boot/boot_info.h"
#include "boot_console.h"
#include "boot_elf.h"
#include "boot_module.h"
#include "boot_math.h"
#include "boot_paging.h"
#include "boot_paging_ops.h"
#include "boot/multiboot.h"

#include "stddef.h"

/// @defgroup bootloader Bootloader
/// @brief Set of functions and variables for booting the kernel.
/// @{

/// @brief External function implemented in `boot.S`.
/// @param stack_pointer The stack base pointer, usually at the end of the lowmem.
/// @param entry
/// @param boot_info
extern void boot_kernel(uint32_t stack_pointer, uint32_t entry, boot_info_t *boot_info);

/// @brief Size of the kernel's stack (4MB - increased from 1MB to accommodate debug logging).
#define KERNEL_STACK_SIZE (4 * 0x100000)

/// @brief Linker symbol for where the bootloader starts.
extern char _bootloader_start[];
/// @brief Linker symbol for where the bootloader ends.
extern char _bootloader_end[];

/// @brief Boot info provided to the kmain function.
static boot_info_t boot_info;
/// @brief Boot page directory.
static boot_page_directory_t boot_pgdir;
/// @brief Boot page tables.
static boot_page_table_t boot_pgtables[BOOT_PAGE_ENTRIES];

/// @brief Align memory address to the specified value (round up).
/// @param addr the address to align
/// @param value the value used to align.
/// @return the aligned address.
static inline uint32_t __align_rup(uint32_t addr, uint32_t value)
{
    uint32_t reminder = (addr % value);
    return addr + (reminder ? (value - reminder) : 0);
}

/// @brief Align memory address to the specified value (round down).
/// @param addr the address to align
/// @param value the value used to align.
/// @return the aligned address.
static inline uint32_t __align_rdown(uint32_t addr, uint32_t value) { return addr - (addr % value); }

/// @brief Prepares the page frames.
/// @param pfn_virt_start The first virtual page frame.
/// @param pfn_phys_start The first physical page frame.
/// @param pfn_count The number of page frames.
/// @brief Entry point of the bootloader.
/// @param magic  The magic number coming from the multiboot assembly code.
/// @param header Multiboot header provided by the bootloader.
/// @param esp    The initial stack pointer.
void boot_main(uint32_t magic, multiboot_info_t *header, uint32_t esp)
{
    boot_console_puts("\n[bootloader] Start...\n");
    uint32_t kernel_image_size = 0;
    const unsigned char *kernel_image = boot_get_kernel_image(header, &kernel_image_size);
    elf_header_t *elf_hdr = (elf_header_t *)kernel_image;
    boot_validate_kernel_image(elf_hdr, kernel_image_size);

    // Get the physical addresses of where the kernel starts and ends.
    uint32_t boot_start = (uint32_t)_bootloader_start;
    uint32_t boot_end   = (uint32_t)_bootloader_end;

    // Extract the lowest and highest address of the kernel.
    uint32_t kernel_virt_low  = 0xFFFFFFFF;
    uint32_t kernel_virt_high = 0;
    boot_get_kernel_low_high(elf_hdr, &kernel_virt_low, &kernel_virt_high);

    // Initialize the boot_info_t structure.
    boot_console_puts("[bootloader] Initializing the boot_info structure...\n");
    boot_info.version             = BOOT_INFO_ABI_VERSION;
    boot_info.size                = sizeof(boot_info);
    boot_info.magic                = magic;
    boot_info.bootloader_phy_start = boot_start;
    boot_info.bootloader_phy_end   = boot_end;
    boot_info.kernel_start         = kernel_virt_low;
    boot_info.kernel_end           = kernel_virt_high;
    boot_info.kernel_size          = kernel_virt_high - kernel_virt_low;
    boot_info.multiboot_header     = header;

    // Get the address after the modules.
    boot_info.module_end = boot_get_address_after_modules(header, boot_info.bootloader_phy_end);

    // Get the starting address of the physical pages at the end of the modules.
    uint32_t kernel_phy_page_start  = __align_rup(boot_info.module_end, PAGE_SIZE);
    // Get the starting address of the virtual pages.
    uint32_t kernel_virt_page_start = __align_rdown(kernel_virt_low, PAGE_SIZE);

    // Compute the absolute offset of the first virtual page, by subtracting
    // the starting address of the virtual pages and the lowest virtual address
    // of the kernel.
    uint32_t kernel_page_offset = kernel_virt_page_start - kernel_virt_low;

    // If we add the offset we computed earlier to the physical address where
    // the modules ends, we obtain the starting address of the physical memory.
    boot_info.kernel_phy_start = kernel_phy_page_start + kernel_page_offset;
    // The ending address of the physical memory is just the start plus the
    // size of the kernel (virt_high - virt_low).
    boot_info.kernel_phy_end   = boot_info.kernel_phy_start + boot_info.kernel_size;

    // Start lowmem right after the kernel end (page-aligned).
    // DMA zone will be carved from physical memory below 16MB during zone init.
    boot_info.lowmem_phy_start  = __align_rup(boot_info.kernel_phy_end, PAGE_SIZE);
    boot_info.lowmem_phy_end    = 896 * 1024 * 1024; // 896 MB of low memory max
    boot_info.lowmem_size       = boot_info.lowmem_phy_end - boot_info.lowmem_phy_start;
    // Use linear mapping offset so lowmem virtual addresses match physical addresses.
    boot_info.lowmem_virt_start = boot_info.kernel_start + (boot_info.lowmem_phy_start - boot_info.kernel_phy_start);
    boot_info.lowmem_virt_end   = boot_info.lowmem_virt_start + boot_info.lowmem_size;

    boot_info.highmem_phy_start = boot_info.lowmem_phy_end;
    boot_info.highmem_phy_end   = header->mem_upper * 1024;
    boot_info.stack_end         = boot_info.lowmem_virt_end;

    // Setup the page directory and page tables for the boot.
    boot_console_puts("[bootloader] Setting up paging...\n");
    boot_paging_setup(&boot_info, &boot_pgdir, boot_pgtables);

    // Switch to the newly created page directory.
    boot_console_puts("[bootloader] Switching page directory...\n");
    boot_paging_switch_directory(&boot_pgdir);

    // Enable paging.
    boot_console_puts("[bootloader] Enabling paging...\n");
    boot_paging_enable();

    // Reserve space for the kernel stack at the end of lowmem.
    boot_info.stack_base      = boot_info.lowmem_virt_end;
    boot_info.stack_size      = KERNEL_STACK_SIZE;
    boot_info.lowmem_phy_end  = boot_info.lowmem_phy_end - KERNEL_STACK_SIZE;
    boot_info.lowmem_virt_end = boot_info.lowmem_virt_end - KERNEL_STACK_SIZE;

    boot_console_puts("[bootloader] Relocating kernel image...\n");
    boot_relocate_kernel_image(elf_hdr);

    boot_paging_protect_stack_guard(&boot_info, &boot_pgdir, boot_pgtables);

    boot_console_puts("[bootloader] Calling `boot_kernel`...\n\n");
    boot_kernel(boot_info.stack_base, elf_hdr->entry, &boot_info);
}

/// @}
