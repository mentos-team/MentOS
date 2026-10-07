/// @file boot_elf.c
/// @brief ELF validation and relocation for the bootloader.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "boot_elf.h"

#include "boot_console.h"
#include "boot_math.h"

static int boot_u32_add_overflows(uint32_t left, uint32_t right, uint32_t *result)
{
    if (left > UINT32_MAX - right) {
        return 1;
    }
    *result = left + right;
    return 0;
}

static int boot_is_power_of_two(uint32_t value)
{
    return value == 0 || (value & (value - 1U)) == 0;
}

void boot_validate_kernel_image(const elf_header_t *header, uint32_t image_size)
{
    uint32_t table_end;
    uint32_t loadable_segments = 0;
    int entry_is_executable = 0;

    if (image_size < sizeof(*header)) {
        boot_fatal("kernel module is smaller than an ELF header");
    }
    if (header->ident[EI_MAG0] != ELFMAG0 || header->ident[EI_MAG1] != ELFMAG1 ||
        header->ident[EI_MAG2] != ELFMAG2 || header->ident[EI_MAG3] != ELFMAG3 ||
        header->ident[EI_CLASS] != ELFCLASS32 || header->ident[EI_DATA] != ELFDATA2LSB ||
        header->ident[EI_VERSION] != EV_CURRENT || header->type != ET_EXEC || header->machine != EM_386 ||
        header->version != EV_CURRENT || header->ehsize != sizeof(*header) ||
        header->phentsize != sizeof(elf_program_header_t) || !header->phnum) {
        boot_fatal("kernel module has an unsupported ELF header");
    }
    if (header->phnum > UINT32_MAX / header->phentsize ||
        boot_u32_add_overflows(header->phoff, (uint32_t)header->phnum * header->phentsize, &table_end) ||
        table_end > image_size) {
        boot_fatal("kernel module program headers are out of bounds");
    }

    const elf_program_header_t *program_headers =
        (const elf_program_header_t *)((uintptr_t)header + header->phoff);
    for (uint32_t i = 0; i < header->phnum; ++i) {
        const elf_program_header_t *program = &program_headers[i];
        uint32_t file_end;
        uint32_t virtual_end;

        if (program->type != PT_LOAD) {
            continue;
        }
        ++loadable_segments;
        if (program->filesz > program->memsz ||
            boot_u32_add_overflows(program->offset, program->filesz, &file_end) || file_end > image_size ||
            boot_u32_add_overflows(program->vaddr, program->memsz, &virtual_end) ||
            program->vaddr < BOOT_KERNEL_VIRT_START || virtual_end > BOOT_KERNEL_VIRT_END ||
            !boot_is_power_of_two(program->align) ||
            (program->align > 1U && ((program->vaddr - program->offset) & (program->align - 1U)) != 0)) {
            boot_fatal("kernel module has an invalid load segment");
        }

        for (uint32_t previous = 0; previous < i; ++previous) {
            const elf_program_header_t *other = &program_headers[previous];
            uint32_t other_end;
            if (other->type != PT_LOAD) {
                continue;
            }
            boot_u32_add_overflows(other->vaddr, other->memsz, &other_end);
            if (program->vaddr < other_end && other->vaddr < virtual_end) {
                boot_fatal("kernel module load segments overlap");
            }
        }
        if ((program->flags & PF_X) && header->entry >= program->vaddr && header->entry < virtual_end) {
            entry_is_executable = 1;
        }
    }

    if (!loadable_segments || !entry_is_executable) {
        boot_fatal("kernel module has no executable entry segment");
    }
}

void boot_get_kernel_low_high(const elf_header_t *elf_header, uint32_t *virt_low, uint32_t *virt_high)
{
    const uint32_t offset = (uint32_t)elf_header + elf_header->phoff;

    for (uint32_t i = 0; i < elf_header->phnum; ++i) {
        const elf_program_header_t *program =
            (const elf_program_header_t *)(offset + (elf_header->phentsize * i));
        if (program->type == PT_LOAD) {
            *virt_low = boot_min(*virt_low, program->vaddr);
            *virt_high = boot_max(*virt_high, program->vaddr + program->memsz);
        }
    }
}

void boot_relocate_kernel_image(elf_header_t *elf_header)
{
    char *kernel_start = (char *)elf_header;
    const uint32_t offset = (uint32_t)kernel_start + elf_header->phoff;

    for (uint32_t i = 0; i < elf_header->phnum; ++i) {
        elf_program_header_t *program =
            (elf_program_header_t *)(offset + (elf_header->phentsize * i));
        if (program->type == PT_LOAD) {
            char *virtual_address = (char *)program->vaddr;
            const char *physical_address = kernel_start + program->offset;
            uint32_t valid_size = boot_min(program->filesz, program->memsz);

            for (uint32_t j = 0; j < valid_size; ++j) {
                virtual_address[j] = physical_address[j];
            }
            for (uint32_t j = valid_size; j < program->memsz; ++j) {
                virtual_address[j] = 0;
            }
        }
    }
}
