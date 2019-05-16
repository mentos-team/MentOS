///                MentOS, The Mentoring Operating system project
/// @file   multiboot.c
/// @brief
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "multiboot.h"
#include "bitops.h"
#include "debug.h"
#include "panic.h"

#define CHECK_FLAG(flags, bit) ((flags) & (1 << (bit)))

static inline multiboot_memory_map_t *first_mmap_entry(multiboot_info_t *info)
{
	if (!has_flag(info->flags, MULTIBOOT_FLAG_MMAP))
		return NULL;
	return (multiboot_memory_map_t *)((uintptr_t)info->mmap_addr);
}

static inline multiboot_memory_map_t *
next_mmap_entry(multiboot_info_t *info, multiboot_memory_map_t *entry)
{
	uintptr_t next = ((uintptr_t)entry) + entry->size + sizeof(entry->size);
	if (next >= info->mmap_addr + info->mmap_length)
		return NULL;
	return (multiboot_memory_map_t *)next;
}

static inline multiboot_memory_map_t *
next_mmap_entry_of_type(multiboot_info_t *info, multiboot_memory_map_t *entry,
						uint32_t type)
{
	do {
		entry = next_mmap_entry(info, entry);
	} while (entry && entry->type != type);
	return entry;
}

static inline multiboot_memory_map_t *
first_mmap_entry_of_type(multiboot_info_t *info, uint32_t type)
{
	multiboot_memory_map_t *entry = first_mmap_entry(info);
	if (entry && (entry->type == type))
		return entry;
	return next_mmap_entry_of_type(info, entry, type);
}

static inline char *mmap_type_name(multiboot_memory_map_t *entry)
{
	if (entry->type == MULTIBOOT_MEMORY_AVAILABLE)
		return "AVAILABLE";
	if (entry->type == MULTIBOOT_MEMORY_RESERVED)
		return "RESERVED";
	return "NONE";
}

static inline multiboot_module_t *first_module(multiboot_info_t *info)
{
	if (!has_flag(info->flags, MULTIBOOT_FLAG_MODS))
		return NULL;
	if (!info->mods_count)
		return NULL;
	return (multiboot_module_t *)(uintptr_t)info->mods_addr;
}

static inline multiboot_module_t *next_module(multiboot_info_t *info,
											  multiboot_module_t *mod)
{
	multiboot_module_t *first =
		(multiboot_module_t *)((uintptr_t)info->mods_addr);
	++mod;
	if ((mod - first) >= info->mods_count)
		return NULL;
	return mod;
}

void dump_multiboot(multiboot_info_t *mbi)
{
	dbg_print("\n--------------------------------------------------\n");
	dbg_print("MULTIBOOT header at 0x%x:\n", mbi);

	// Print out the flags.
	dbg_print("%-16s = 0x%x\n", "flags", mbi->flags);

	// Are mem_* valid?
	if (has_flag(mbi->flags, MULTIBOOT_FLAG_MEM)) {
		dbg_print("%-16s = %u Kb (%u Mb)\n", "mem_lower", mbi->mem_lower,
				  mbi->mem_lower / K);
		dbg_print("%-16s = %u Kb (%u Mb)\n", "mem_upper", mbi->mem_upper,
				  mbi->mem_upper / K);
		dbg_print("%-16s = %u Kb (%u Mb)\n", "total",
				  mbi->mem_lower + mbi->mem_upper,
				  (mbi->mem_lower + mbi->mem_upper) / K);
	}

	// Is boot_device valid?
	if (has_flag(mbi->flags, MULTIBOOT_FLAG_DEVICE)) {
		dbg_print("%-16s = 0x%x (0x%x)", "boot_device", mbi->boot_device);
		switch ((mbi->boot_device) & 0xFF000000) {
		case 0x00000000:
			dbg_print("(floppy)\n");
			break;
		case 0x80000000:
			dbg_print("(disk)\n");
			break;
		default:
			dbg_print("(unknown)\n");
		}
	}

	// Is the command line passed?
	if (has_flag(mbi->flags, MULTIBOOT_FLAG_CMDLINE)) {
		dbg_print("%-16s = %s\n", "cmdline", (char *)mbi->cmdline);
	}

	// Are mods_* valid?
	if (has_flag(mbi->flags, MULTIBOOT_FLAG_MODS)) {
		dbg_print("%-16s = %d\n", "mods_count", mbi->mods_count);
		dbg_print("%-16s = 0x%x\n", "mods_addr", mbi->mods_addr);
		multiboot_module_t *mod = first_module(mbi);
		for (int i = 0; mod; ++i, mod = next_module(mbi, mod)) {
			dbg_print("    [%2d] "
					  "mod_start = 0x%x, "
					  "mod_end = 0x%x, "
					  "cmdline = %s\n",
					  i, mod->mod_start, mod->mod_end, (char *)mod->cmdline);
		}
	}
	// Bits 4 and 5 are mutually exclusive!
	if (has_flag(mbi->flags, MULTIBOOT_FLAG_AOUT) &&
		has_flag(mbi->flags, MULTIBOOT_FLAG_ELF)) {
		kernel_panic("Both bits 4 and 5 are set.\n");
		return;
	}

	// Is the symbol table of a.out valid?
	if (has_flag(mbi->flags, MULTIBOOT_FLAG_AOUT)) {
		multiboot_aout_symbol_table_t *multiboot_aout_sym = &(mbi->u.aout_sym);
		dbg_print("multiboot_aout_symbol_table: tabsize = 0x%0x, "
				  "strsize = 0x%x, addr = 0x%x\n",
				  multiboot_aout_sym->tabsize, multiboot_aout_sym->strsize,
				  multiboot_aout_sym->addr);
	}

	// Is the section header table of ELF valid?
	if (has_flag(mbi->flags, MULTIBOOT_FLAG_ELF)) {
		multiboot_elf_section_header_table_t *multiboot_elf_sec =
			&(mbi->u.elf_sec);
		dbg_print("multiboot_elf_sec: num = %u, size = 0x%x,"
				  " addr = 0x%x, shndx = 0x%x\n",
				  multiboot_elf_sec->num, multiboot_elf_sec->size,
				  multiboot_elf_sec->addr, multiboot_elf_sec->shndx);
	}

	// Are mmap_* valid?
	if (has_flag(mbi->flags, MULTIBOOT_FLAG_MMAP)) {
		dbg_print("%-16s = 0x%x\n", "mmap_addr", mbi->mmap_addr);
		dbg_print("%-16s = 0x%x (%d entries)\n", "mmap_length",
				  mbi->mmap_length,
				  mbi->mmap_length / sizeof(multiboot_memory_map_t));
		multiboot_memory_map_t *mmap = first_mmap_entry(mbi);
		for (int i = 0; mmap; ++i, mmap = next_mmap_entry(mbi, mmap)) {
			dbg_print("    [%2d] "
					  "base_addr = 0x%09x%09x, "
					  "length = 0x%09x%09x, "
					  "type = 0x%x (%s)\n",
					  i, mmap->base_addr_high, mmap->base_addr_low,
					  mmap->length_high, mmap->length_low, mmap->type,
					  mmap_type_name(mmap));
		}
	}

	if (has_flag(mbi->flags, MULTIBOOT_FLAG_DRIVE_INFO)) {
		dbg_print("Drives: 0x%x\n", mbi->drives_length);
		dbg_print("Addr  : 0x%x\n", mbi->drives_addr);
	}

	if (has_flag(mbi->flags, MULTIBOOT_FLAG_CONFIG_TABLE)) {
		dbg_print("Config: 0x%x\n", mbi->config_table);
	}

	if (has_flag(mbi->flags, MULTIBOOT_FLAG_BOOT_LOADER_NAME)) {
		dbg_print("boot_loader_name: %s\n", (char *)mbi->boot_loader_name);
	}

	if (has_flag(mbi->flags, MULTIBOOT_FLAG_APM_TABLE)) {
		dbg_print("APM   : 0x%x\n", mbi->apm_table);
	}

	if (has_flag(mbi->flags, MULTIBOOT_FLAG_VBE_INFO)) {
		dbg_print("VBE Co: 0x%x\n", mbi->vbe_control_info);
		dbg_print("VBE Mo: 0x%x\n", mbi->vbe_mode_info);
		dbg_print("VBE In: 0x%x\n", mbi->vbe_mode);
		dbg_print("VBE se: 0x%x\n", mbi->vbe_interface_seg);
		dbg_print("VBE of: 0x%x\n", mbi->vbe_interface_off);
		dbg_print("VBE le: 0x%x\n", mbi->vbe_interface_len);
	}
	dbg_print("--------------------------------------------------\n");
	dbg_print("\n");
}
