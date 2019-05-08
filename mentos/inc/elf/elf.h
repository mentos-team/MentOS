///                MentOS, The Mentoring Operating system project
/// @file elf.h
/// @brief Function for multiboot support.
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "stdint.h"
#include "multiboot.h"

// Used to get the symbol type.
#define ELF32_ST_TYPE(i) ((i)&0xf)

// List of symbol types.
#define ELF32_TYPE_FUNCTION (0x02)

/// @brief A section header with all kinds of useful information.
typedef struct elf_section_header
{
    uint32_t name_offset_in_shstrtab;
    uint32_t type;
    uint32_t flags;
    uint32_t addr;
    uint32_t offset;
    uint32_t size;
    uint32_t link;
    uint32_t info;
    uint32_t addralign;
    uint32_t entsize;
} __attribute__((packed)) elf_section_header_t;

/// @brief A symbol itself.
typedef struct elf_symbol
{
    uint32_t name_offset_in_strtab;
    uint32_t value;
    uint32_t size;
    uint8_t  info;
    uint8_t  other;
    uint16_t shndx;
} __attribute__((packed)) elf_symbol_t;

// Will hold the array of symbols and their names for us.
typedef struct elf_symbols
{
    elf_symbol_t *symtab;
    uint32_t      symtab_size;
    const char   *strtab;
    uint32_t      strtab_size;
} elf_symbols_t;


/// @brief Builds as the set of elf symbols from a multiboot scructure.
void build_elf_symbols_from_multiboot (multiboot_info_t* mb);

/* Locate a symbol (only functions) in the following elf symbols
 * Note that, for now, we'll only use this for the kernel (no other elf things..)
 */
const char *elf_lookup_symbol (uint32_t addr, elf_symbols_t *elf);
