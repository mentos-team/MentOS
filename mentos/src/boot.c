///               MentOS, The Mentoring Operating system project
/// @file boot.c
/// @brief Bootloader.
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "boot.h"

#include "link_access.h"
#include "sys/module.h"
#include "mem/paging.h"
#include "elf/elf.h"

/// @defgroup bootloader Bootloader
/// @brief Set of functions and variables for booting the kernel.
/// @{

/// @brief External function implemented in `boot.S`.
/// @param stack_pointer The stack base pointer, usually at the end of the lowmem.
/// @param entry
/// @param boot_info
extern void boot_kernel(uint32_t stack_pointer, uint32_t entry, struct boot_info_t *boot_info);

/// @brief Size of the kernel's stack.
#define KERNEL_STACK_SIZE 0x100000

/// Serial port for QEMU.
#define SERIAL_COM1 (0x03F8)

/// @brief Linker symbols for where the .data section of `kernel.bin.o`.
EXTLD(kernel_bin)

/// @brief Linker symbol for where the bootloader starts.
extern char _bootloader_start[];
/// @brief Linker symbol for where the bootloader ends.
extern char _bootloader_end[];

/// @brief Boot info provided to the kmain function.
static boot_info_t boot_info;
/// @brief Boot page directory.
static page_directory_t boot_pgdir;
/// @brief Boot page tables.
static page_table_t boot_pgtables[1024];

/// @brief      Use this to write to I/O ports to send bytes to devices.
/// @param port The output port.
/// @param data The data to write.
static inline void __outportb(uint16_t port, uint8_t data)
{
    __asm__ __volatile__("outb %%al, %%dx" ::"a"(data), "d"(port));
}

/// @brief Writes the given character on the debug port.
/// @param c the character to send to the debug port.
static inline void __debug_putchar(char c)
{
    __outportb(SERIAL_COM1, c);
}

/// @brief Writes the given string on the debug port.
/// @param s the string to send to the debug port.
static inline void __debug_puts(char *s)
{
    while ((*s) != 0)
        __outportb(SERIAL_COM1, *s++);
}

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
static inline uint32_t __align_rdown(uint32_t addr, uint32_t value)
{
    return addr - (addr % value);
}

/// @brief Prepares the page frames.
/// @param pfn_virt_start The first virtual page frame.
/// @param pfn_phys_start The first physical page frame.
/// @param pfn_count The number of page frames.
static void __setup_pages(uint32_t pfn_virt_start, uint32_t pfn_phys_start, uint32_t pfn_count)
{
    uint32_t base_pgtable = pfn_virt_start / 1024;
    uint32_t base_pgentry = pfn_virt_start % 1024;

    uint32_t pg_offset = 0;
    for (int i = base_pgtable; i < 1024 && pfn_count; i++) {
        page_table_t *table = boot_pgtables + i;

        uint32_t pgentry_start = (i == base_pgtable) ? base_pgentry : 0;

        for (int j = pgentry_start; j < 1024 && pfn_count; j++, pfn_count--) {
            table->pages[j].frame   = pfn_phys_start + pg_offset++;
            table->pages[j].rw      = 1;
            table->pages[j].present = 1;
            table->pages[j].global  = 0;
            table->pages[j].user    = 0;
        }
        boot_pgdir.entries[i].rw        = 1;
        boot_pgdir.entries[i].present   = 1;
        boot_pgdir.entries[i].available = 1;
        boot_pgdir.entries[i].frame     = ((uint32_t)table) >> 12u;
    }
}

/// @brief Setup paging mapping all the low memory to two places: one is the
/// physical address of the memory itself the other is in the virtual kernel
/// address space.
static inline void __setup_boot_paging()
{
    uint32_t kernel_base_phy_page  = boot_info.kernel_phy_start >> 12U;
    uint32_t kernel_base_virt_page = boot_info.kernel_start >> 12U;
    // Compute the last physical page.
    uint32_t lowmem_last_phy_page = ((uint32_t)(boot_info.lowmem_phy_end - 1)) >> 12U;
    // Compute the number of pages.
    uint32_t num_pages = lowmem_last_phy_page - kernel_base_phy_page + 1;
    // Map lowmem physical pages also to their physical address (to keep bootloader working)
    __setup_pages(0, 0, lowmem_last_phy_page);
    // Setup kernel virtual address space + lowmem
    __setup_pages(kernel_base_virt_page, kernel_base_phy_page, num_pages);
}

/// @brief Extract the starting and ending address of the kernel.
/// @param elf_hdr The elf header of the kernel.
/// @param virt_low  Output variable where we store the lowest address of the kernel.
/// @param virt_high Output variable where we store the highest address of the kernel.
static void __get_kernel_low_high(elf_header_t *elf_hdr, uint32_t *virt_low, uint32_t *virt_high)
{
    // Prepare a pointer to a program header.
    elf_program_header_t *program_header;
    // Compute the offset for accessing the program headers.
    uint32_t offset = (uint32_t)elf_hdr + (uint32_t)elf_hdr->phoff;
    // In this two variables we will store the start and end addresses of the segment.
    uint32_t segment_start, segment_end;
    // Iterate for each program header.
    for (int i = 0; i < elf_hdr->phnum; i++) {
        program_header = (elf_program_header_t *)(offset + elf_hdr->phentsize * i);
        if (program_header->type == PT_LOAD) {
            // Take the start and end addresses of the segment from the program header.
            segment_start = program_header->vaddr;
            segment_end   = segment_start + program_header->memsz;
            // Take the lowest and highest virtual address.
            *virt_low  = min(*virt_low, segment_start);
            *virt_high = max(*virt_high, segment_end);
        }
    }
}

/// @brief Returns the first address after the modules.
/// @param header The multiboot info structure from which we extract the info.
/// @return The address after the modules.
static inline uint32_t __get_address_after_modules(multiboot_info_t *header)
{
    // We set by default the address to the ending physical address
    // of the bootloader.
    uint32_t addr = boot_info.bootloader_phy_end;
    // Get the pointer to the mods.
    multiboot_module_t *mod = (multiboot_module_t *)header->mods_addr;
    for (int i = 0; (i < header->mods_count) && (i < MAX_MODULES); ++i, ++mod) {
        addr = max(max(addr, mod->mod_start), mod->mod_end);
    }
    return addr;
}

/// @brief Relocate the kernel image.
/// @param elf_hdr The elf header of the kernel.
static inline void __relocate_kernel_image(elf_header_t *elf_hdr)
{
    // Support variables.
    elf_program_header_t *program_header;
    char *kernel_start, *virtual_address, *physical_address;
    uint32_t offset, valid_size;

    // Get the elf file starting address.
    kernel_start = (char *)elf_hdr;
    // Compute the offset for accessing the program headers.
    offset = (uint32_t)kernel_start + (uint32_t)elf_hdr->phoff;
    // Iterate over the program headers.
    for (int i = 0; i < elf_hdr->phnum; i++) {
        // Get the program header.
        program_header = (elf_program_header_t *)(offset + elf_hdr->phentsize * i);
        // Get the virtual address of the program header.
        virtual_address = (char *)program_header->vaddr;
        // Get the physical address of the program header.
        physical_address = (char *)(kernel_start + program_header->offset);
        // Move only the loadable segments.
        if (program_header->type == PT_LOAD) {
            // Get the valid size of the segment by taking the minimum between
            // the size in bytes of the segment in the file image, in memory.
            valid_size = min(program_header->filesz, program_header->memsz);
            // Copy the physical data of the image to the corresponding virtual address.
            for (int j = 0; j < valid_size; j++)
                virtual_address[j] = physical_address[j];
            // Set to 0 parts not present in memory!
            for (int j = valid_size; j < program_header->memsz; j++)
                virtual_address[j] = 0;
        }
    }
}

/// @brief Entry point of the bootloader.
/// @param magic  The magic number coming from the multiboot assembly code.
/// @param header Multiboot header provided by the bootloader.
/// @param esp    The initial stack pointer.
void boot_main(uint32_t magic, multiboot_info_t *header, uint32_t esp)
{
    __debug_puts("\n[bootloader] Start...\n");
    elf_header_t *elf_hdr = (elf_header_t *)LDVAR(kernel_bin);

    // Get the physical addresses of where the kernel starts and ends.
    uint32_t boot_start = (uint32_t)_bootloader_start;
    uint32_t boot_end   = (uint32_t)_bootloader_end;

    // Extract the lowest and highest address of the kernel.
    uint32_t kernel_virt_low  = 0xFFFFFFFF;
    uint32_t kernel_virt_high = 0;
    __get_kernel_low_high(elf_hdr, &kernel_virt_low, &kernel_virt_high);

    // Initialize the boot_info_t structure.
    __debug_puts("[bootloader] Initializing the boot_info structure...\n");
    boot_info.magic                = magic;
    boot_info.bootloader_phy_start = boot_start;
    boot_info.bootloader_phy_end   = boot_end;
    boot_info.kernel_start         = kernel_virt_low;
    boot_info.kernel_end           = kernel_virt_high;
    boot_info.kernel_size          = kernel_virt_high - kernel_virt_low;
    boot_info.multiboot_header     = header;

    // Get the address after the modules.
    boot_info.module_end = __get_address_after_modules(header);

    // Get the starting address of the physical pages at the end of the modules.
    uint32_t kernel_phy_page_start = __align_rup(boot_info.module_end, PAGE_SIZE);
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
    boot_info.kernel_phy_end = boot_info.kernel_phy_start + boot_info.kernel_size;

    boot_info.lowmem_phy_start = __align_rup(boot_info.kernel_phy_end, PAGE_SIZE);
    boot_info.lowmem_phy_end   = 896 * 1024 * 1024; // 896 MB of low memory max

    uint32_t lowmem_size = boot_info.lowmem_phy_end - boot_info.lowmem_phy_start;

    boot_info.lowmem_start = __align_rup(boot_info.kernel_end, PAGE_SIZE);
    boot_info.lowmem_end   = boot_info.lowmem_start + lowmem_size;

    boot_info.highmem_phy_start = boot_info.lowmem_phy_end;
    boot_info.highmem_phy_end   = header->mem_upper * 1024;
    boot_info.stack_end         = boot_info.lowmem_end;

    // Setup the page directory and page tables for the boot.
    __debug_puts("[bootloader] Setting up paging...\n");
    __setup_boot_paging();

    // Switch to the newly created page directory.
    __debug_puts("[bootloader] Switching page directory...\n");
    paging_switch_directory(&boot_pgdir);

    // Enable paging.
    __debug_puts("[bootloader] Enabling paging...\n");
    paging_enable();

    // Reserve space for the kernel stack at the end of lowmem.
    boot_info.stack_base     = boot_info.lowmem_end;
    boot_info.lowmem_phy_end = boot_info.lowmem_phy_end - KERNEL_STACK_SIZE;
    boot_info.lowmem_end     = boot_info.lowmem_end - KERNEL_STACK_SIZE;

    __debug_puts("[bootloader] Relocating kernel image...\n");
    __relocate_kernel_image(elf_hdr);

    __debug_puts("[bootloader] Calling `boot_kernel`...\n\n");
    boot_kernel(boot_info.stack_base, elf_hdr->entry, &boot_info);
}

/// @}
