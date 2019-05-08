///                MentOS, The Mentoring Operating system project
/// @file   multiboot.c
/// @brief
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "multiboot.h"
#include "bitops.h"
#include "debug.h"

void dump_multiboot(multiboot_info_t *mbi)
{
	dbg_print("\n--------------------------------------------------\n");
	dbg_print("MULTIBOOT header at 0x%x:\n", mbi);
	dbg_print("Flags : 0x%x\n", mbi->flags);
	if (has_flag(mbi->flags, MULTIBOOT_FLAG_MEM)) {
		dbg_print("Mem Lo: 0x%x\n", mbi->mem_lower * K);
		dbg_print("Mem Hi: 0x%x (%dMB)\n", mbi->mem_upper * K,
				  (mbi->mem_upper / 1024));
	}
	if (has_flag(mbi->flags, MULTIBOOT_FLAG_DEVICE)) {
		dbg_print("Boot d: 0x%x\n", mbi->boot_device);
	}
	if (has_flag(mbi->flags, MULTIBOOT_FLAG_CMDLINE)) {
		dbg_print("cmdlin: 0x%x (%s)\n", mbi->cmdline, (char *)mbi->cmdline);
	}
	if (has_flag(mbi->flags, MULTIBOOT_FLAG_MODS)) {
		dbg_print("Mods  : 0x%x\n", mbi->mods_count);

		multiboot_module_t *mod = (multiboot_module_t *)mbi->mods_addr;

		if (mbi->mods_count > 0) {
			for (uint32_t i = 0; i < mbi->mods_count && i < MAX_MODULES;
				 i++, mod++) {
				uint32_t start = mod->mod_start;
				uint32_t end = mod->mod_end;
				dbg_print("\tModule %d is at 0x%x:0x%x\n", i + 1, start, end);
			}

			/* Last implementation
            for (uint32_t i = 0; i < mbi->mods_count; ++i)
            {
                // uint32_t start = *((uint32_t *) (mbi->mods_addr + 8 * i));
                uint32_t start = mbi->mods_addr + 8 * i;
                // uint32_t end = *((uint32_t *) (mbi->mods_addr + 8 * i + 4));
                uint32_t end = mbi->mods_addr + 8 * i + 4;
                dbg_print("\tModule %d is at 0x%x:0x%x\n", i + 1, start, end);
            }
             */
		}
	}
	if (has_flag(mbi->flags, MULTIBOOT_FLAG_AOUT)) {
		dbg_print("AOUT t : 0x%x\n", mbi->u.aout_sym.tabsize);
		dbg_print("AOUT s : 0x%x\n", mbi->u.aout_sym.strsize);
		dbg_print("AOUT a : 0x%x\n", mbi->u.aout_sym.addr);
		dbg_print("AOUT r : 0x%x\n", mbi->u.aout_sym.reserved);
	}
	if (has_flag(mbi->flags, MULTIBOOT_FLAG_ELF)) {
		dbg_print("ELF n : 0x%x\n", mbi->u.elf_sec.num);
		dbg_print("ELF s : 0x%x\n", mbi->u.elf_sec.size);
		dbg_print("ELF a : 0x%x\n", mbi->u.elf_sec.addr);
		dbg_print("ELF h : 0x%x\n", mbi->u.elf_sec.shndx);
	}
	dbg_print("MMap  : 0x%x\n", mbi->mmap_length);
	dbg_print("Addr  : 0x%x\n", mbi->mmap_addr);
	dbg_print("Drives: 0x%x\n", mbi->drives_length);
	dbg_print("Addr  : 0x%x\n", mbi->drives_addr);
	dbg_print("Config: 0x%x\n", mbi->config_table);
	dbg_print("Loader: 0x%x (%s)\n", mbi->boot_loader_name,
			  (char *)mbi->boot_loader_name);
	dbg_print("APM   : 0x%x\n", mbi->apm_table);
	dbg_print("VBE Co: 0x%x\n", mbi->vbe_control_info);
	dbg_print("VBE Mo: 0x%x\n", mbi->vbe_mode_info);
	dbg_print("VBE In: 0x%x\n", mbi->vbe_mode);
	dbg_print("VBE se: 0x%x\n", mbi->vbe_interface_seg);
	dbg_print("VBE of: 0x%x\n", mbi->vbe_interface_off);
	dbg_print("VBE le: 0x%x\n", mbi->vbe_interface_len);
	dbg_print("--------------------------------------------------\n");
	dbg_print("\n");
}
