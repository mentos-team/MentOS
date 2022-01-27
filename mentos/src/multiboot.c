/// @file   multiboot.c
/// @brief
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

// Include the kernel log levels.
#include "sys/kernel_levels.h"
/// Change the header.
#define __DEBUG_HEADER__ "[BOOT  ]"
/// Set the log level.
#define __DEBUG_LEVEL__ LOGLEVEL_NOTICE

#include "multiboot.h"
#include "kernel.h"
#include "sys/bitops.h"
#include "stddef.h"
#include "io/debug.h"
#include "system/panic.h"
#include "stddef.h"


multiboot_memory_map_t *mmap_first_entry(multiboot_info_t *info)
{
    if (!bitmask_check(info->flags, MULTIBOOT_FLAG_MMAP))
        return NULL;
    return (multiboot_memory_map_t *)((uintptr_t)info->mmap_addr);
}

multiboot_memory_map_t *mmap_first_entry_of_type(multiboot_info_t *info,
                                                 uint32_t type)
{
    multiboot_memory_map_t *entry = mmap_first_entry(info);
    if (entry && (entry->type == type))
        return entry;
    return mmap_next_entry_of_type(info, entry, type);
}

multiboot_memory_map_t *mmap_next_entry(multiboot_info_t *info,
                                        multiboot_memory_map_t *entry)
{
    uintptr_t next = ((uintptr_t)entry) + entry->size + sizeof(entry->size);
    if (next >= (info->mmap_addr + info->mmap_length))
        return NULL;
    return (multiboot_memory_map_t *)next;
}

multiboot_memory_map_t *mmap_next_entry_of_type(multiboot_info_t *info,
                                                multiboot_memory_map_t *entry,
                                                uint32_t type)
{
    do {
        entry = mmap_next_entry(info, entry);
    } while (entry && entry->type != type);
    return entry;
}

char *mmap_type_name(multiboot_memory_map_t *entry)
{
    if (entry->type == MULTIBOOT_MEMORY_AVAILABLE)
        return "AVAILABLE";
    if (entry->type == MULTIBOOT_MEMORY_RESERVED)
        return "RESERVED";
    return "NONE";
}

multiboot_module_t *first_module(multiboot_info_t *info)
{
    if (!bitmask_check(info->flags, MULTIBOOT_FLAG_MODS))
        return NULL;
    if (!info->mods_count)
        return NULL;
    return (multiboot_module_t *)(uintptr_t)info->mods_addr;
}

multiboot_module_t *next_module(multiboot_info_t *info,
                                multiboot_module_t *mod)
{
    multiboot_module_t *first = (multiboot_module_t *)((uintptr_t)info->mods_addr);
    ++mod;
    if ((mod - first) >= info->mods_count)
        return NULL;
    return mod;
}

void dump_multiboot(multiboot_info_t *mbi)
{
    pr_debug("\n--------------------------------------------------\n");
    pr_debug("MULTIBOOT header at 0x%x:\n", mbi);

    // Print out the flags.
    pr_debug("%-16s = 0x%x\n", "flags", mbi->flags);

    // Are mem_* valid?
    if (bitmask_check(mbi->flags, MULTIBOOT_FLAG_MEM)) {
        pr_debug("%-16s = %u Kb (%u Mb)\n", "mem_lower", mbi->mem_lower,
                 mbi->mem_lower / K);
        pr_debug("%-16s = %u Kb (%u Mb)\n", "mem_upper", mbi->mem_upper,
                 mbi->mem_upper / K);
        pr_debug("%-16s = %u Kb (%u Mb)\n", "total",
                 mbi->mem_lower + mbi->mem_upper,
                 (mbi->mem_lower + mbi->mem_upper) / K);
    }

    // Is boot_device valid?
    if (bitmask_check(mbi->flags, MULTIBOOT_FLAG_DEVICE)) {
        pr_debug("%-16s = 0x%x (0x%x)", "boot_device", mbi->boot_device);
        switch ((mbi->boot_device) & 0xFF000000) {
        case 0x00000000:
            pr_debug("(floppy)\n");
            break;
        case 0x80000000:
            pr_debug("(disk)\n");
            break;
        default:
            pr_debug("(unknown)\n");
        }
    }

    // Is the command line passed?
    if (bitmask_check(mbi->flags, MULTIBOOT_FLAG_CMDLINE)) {
        pr_debug("%-16s = %s\n", "cmdline", (char *)mbi->cmdline);
    }

    // Are mods_* valid?
    if (bitmask_check(mbi->flags, MULTIBOOT_FLAG_MODS)) {
        pr_debug("%-16s = %d\n", "mods_count", mbi->mods_count);
        pr_debug("%-16s = 0x%x\n", "mods_addr", mbi->mods_addr);
        multiboot_module_t *mod = first_module(mbi);
        for (int i = 0; mod; ++i, mod = next_module(mbi, mod)) {
            pr_debug("    [%2d] "
                     "mod_start = 0x%x, "
                     "mod_end = 0x%x, "
                     "cmdline = %s\n",
                     i, mod->mod_start, mod->mod_end, (char *)mod->cmdline);
        }
    }
    // Bits 4 and 5 are mutually exclusive!
    if (bitmask_check(mbi->flags, MULTIBOOT_FLAG_AOUT) &&
        bitmask_check(mbi->flags, MULTIBOOT_FLAG_ELF)) {
        kernel_panic("Both bits 4 and 5 are set.\n");
        return;
    }

    // Is the symbol table of a.out valid?
    if (bitmask_check(mbi->flags, MULTIBOOT_FLAG_AOUT)) {
        multiboot_aout_symbol_table_t *multiboot_aout_sym = &(mbi->u.aout_sym);
        pr_debug("multiboot_aout_symbol_table: tabsize = 0x%0x, "
                 "strsize = 0x%x, addr = 0x%x\n",
                 multiboot_aout_sym->tabsize, multiboot_aout_sym->strsize,
                 multiboot_aout_sym->addr);
    }

    // Is the section header table of ELF valid?
    if (bitmask_check(mbi->flags, MULTIBOOT_FLAG_ELF)) {
        multiboot_elf_section_header_table_t *multiboot_elf_sec =
            &(mbi->u.elf_sec);
        pr_debug("multiboot_elf_sec: num = %u, size = 0x%x,"
                 " addr = 0x%x, shndx = 0x%x\n",
                 multiboot_elf_sec->num, multiboot_elf_sec->size,
                 multiboot_elf_sec->addr, multiboot_elf_sec->shndx);
    }

    // Are mmap_* valid?
    if (bitmask_check(mbi->flags, MULTIBOOT_FLAG_MMAP)) {
        pr_debug("%-16s = 0x%x\n", "mmap_addr", mbi->mmap_addr);
        pr_debug("%-16s = 0x%x (%d entries)\n", "mmap_length",
                 mbi->mmap_length,
                 mbi->mmap_length / sizeof(multiboot_memory_map_t));
        multiboot_memory_map_t *mmap = mmap_first_entry(mbi);
        for (int i = 0; mmap; ++i, mmap = mmap_next_entry(mbi, mmap)) {
            pr_debug("    [%2d] "
                     "base_addr = 0x%09x%09x, "
                     "length = 0x%09x%09x, "
                     "type = 0x%x (%s)\n",
                     i, mmap->base_addr_high, mmap->base_addr_low,
                     mmap->length_high, mmap->length_low, mmap->type,
                     mmap_type_name(mmap));
        }
    }

    if (bitmask_check(mbi->flags, MULTIBOOT_FLAG_DRIVE_INFO)) {
        pr_debug("Drives: 0x%x\n", mbi->drives_length);
        pr_debug("Addr  : 0x%x\n", mbi->drives_addr);
    }

    if (bitmask_check(mbi->flags, MULTIBOOT_FLAG_CONFIG_TABLE)) {
        pr_debug("Config: 0x%x\n", mbi->config_table);
    }

    if (bitmask_check(mbi->flags, MULTIBOOT_FLAG_BOOT_LOADER_NAME)) {
        pr_debug("boot_loader_name: %s\n", (char *)mbi->boot_loader_name);
    }

    if (bitmask_check(mbi->flags, MULTIBOOT_FLAG_APM_TABLE)) {
        pr_debug("APM   : 0x%x\n", mbi->apm_table);
    }

    if (bitmask_check(mbi->flags, MULTIBOOT_FLAG_VBE_INFO)) {
        pr_debug("VBE Co: 0x%x\n", mbi->vbe_control_info);
        pr_debug("VBE Mo: 0x%x\n", mbi->vbe_mode_info);
        pr_debug("VBE In: 0x%x\n", mbi->vbe_mode);
        pr_debug("VBE se: 0x%x\n", mbi->vbe_interface_seg);
        pr_debug("VBE of: 0x%x\n", mbi->vbe_interface_off);
        pr_debug("VBE le: 0x%x\n", mbi->vbe_interface_len);
    }
    pr_debug("--------------------------------------------------\n");
    pr_debug("\n");
}
