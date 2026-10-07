#pragma once

#include "elf/elf.h"

/** Validate the kernel ELF image before any segment is copied. */
void boot_validate_kernel_image(const elf_header_t *header, uint32_t image_size);

/** Extract the lowest and highest virtual addresses occupied by PT_LOAD segments. */
void boot_get_kernel_low_high(const elf_header_t *elf_header, uint32_t *virt_low, uint32_t *virt_high);

/** Copy PT_LOAD segments from the module image into their linked addresses. */
void boot_relocate_kernel_image(elf_header_t *elf_header);
