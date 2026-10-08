#pragma once

#include "boot/boot_info.h"
#include "boot_paging.h"

/** Build the identity and kernel-offset mappings needed during handoff. */
void boot_paging_setup(const boot_info_t *info, boot_page_directory_t *directory, boot_page_table_t *tables);

/** Remove the page below the boot-time stack from the temporary mapping. */
void boot_paging_protect_stack_guard(const boot_info_t *info, boot_page_directory_t *directory,
                                     boot_page_table_t *tables);

/** Enable paging after loading the temporary page directory. */
void boot_paging_enable(void);

/** Load the temporary page directory into CR3. */
void boot_paging_switch_directory(boot_page_directory_t *directory);
