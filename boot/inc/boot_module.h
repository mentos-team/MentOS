#pragma once

#include "boot/multiboot.h"

/** Locate the kernel ELF image identified by the exact module command line `kernel`. */
const unsigned char *boot_get_kernel_image(multiboot_info_t *header, uint32_t *image_size);

/** Return the first physical address not occupied by the bootloader or modules. */
uint32_t boot_get_address_after_modules(const multiboot_info_t *header, uint32_t bootloader_end);
