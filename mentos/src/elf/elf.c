///                MentOS, The Mentoring Operating system project
/// @file elf.c
/// @brief Function for multiboot support.
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "elf.h"
#include "debug.h"
#include "string.h"
#include "multiboot.h"

/// @brief Data structure containg information about the kernel.
elf_symbols_t kernel_elf;

/*
 * This function grabs a pointer to the array of section headers.
 * It then grabs a pointer to the section where all the strings are (.shstrtab)
 * (remember that each section header has an offset to this .shstrtab)
 *
 * Then it's just a matter of iterating sections (checking their names via
 * indexing shstrtab) until we find strtab and symtab, which is what we're
 * looking for
 */
void build_elf_symbols_from_multiboot(multiboot_info_t *mb)
{
	uint32_t i;
	elf_section_header_t *sh = (elf_section_header_t *)mb->u.elf_sec.addr;

	/*
     * .shstrtab has the names of the sections,
     * and sh is an array of sections, which themselves contain
     * an index to .shstrtab (for their names)
     */
	uint32_t shstrtab = sh[mb->u.elf_sec.shndx].addr;
	for (i = 0; i < mb->u.elf_sec.num; i++) {
		const char *name =
			(const char *)(shstrtab + sh[i].name_offset_in_shstrtab);
		if (!strcmp(name, ".strtab")) {
			kernel_elf.strtab = (const char *)sh[i].addr;
			kernel_elf.strtab_size = sh[i].size;
		}
		if (!strcmp(name, ".symtab")) {
			kernel_elf.symtab = (elf_symbol_t *)sh[i].addr;
			kernel_elf.symtab_size = sh[i].size;
		}
	}
}

/*
 * Iterate through all the symbols and look for functions...
 * Then, as we find functions, check if the symbol is within that
 * function's range (given by value and size)
 */
const char *elf_lookup_symbol(uint32_t addr, elf_symbols_t *elf)
{
	int i;
	int num_symbols = elf->symtab_size / sizeof(elf_symbol_t);

	for (i = 0; i < num_symbols; i++) {
		if (ELF32_ST_TYPE(elf->symtab[i].info) != ELF32_TYPE_FUNCTION) {
			continue;
		}

		if ((addr >= elf->symtab[i].value) &&
			(addr < (elf->symtab[i].value + elf->symtab[i].size))) {
			const char *name =
				(const char *)((uint32_t)elf->strtab +
							   elf->symtab[i].name_offset_in_strtab);
			return name;
		}
	}

	return NULL;
}
