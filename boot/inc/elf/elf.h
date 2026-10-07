/// @file   elf.h
/// @brief  Minimal ELF32 declarations required by the bootloader.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "stdint.h"

/// ELF identification bytes.
#define EI_NIDENT 16
#define EI_MAG0   0
#define EI_MAG1   1
#define EI_MAG2   2
#define EI_MAG3   3
#define EI_CLASS  4
#define EI_DATA   5
#define EI_VERSION 6

/// ELF32 identity values accepted by the bootloader.
#define ELFMAG0    0x7F
#define ELFMAG1    'E'
#define ELFMAG2    'L'
#define ELFMAG3    'F'
#define ELFCLASS32 1
#define ELFDATA2LSB 1

/// ELF file and machine values accepted by the bootloader.
#define ET_EXEC    2
#define EM_386     3
#define EV_CURRENT 1

/// Loadable ELF program segment.
#define PT_LOAD 1

/// @brief ELF32 file header.
typedef struct elf_header {
    uint8_t  ident[EI_NIDENT];
    uint16_t type;
    uint16_t machine;
    uint32_t version;
    uint32_t entry;
    uint32_t phoff;
    uint32_t shoff;
    uint32_t flags;
    uint16_t ehsize;
    uint16_t phentsize;
    uint16_t phnum;
    uint16_t shentsize;
    uint16_t shnum;
    uint16_t shstrndx;
} elf_header_t;

/// @brief ELF32 program header.
typedef struct elf_program_header {
    uint32_t type;
    uint32_t offset;
    uint32_t vaddr;
    uint32_t paddr;
    uint32_t filesz;
    uint32_t memsz;
    uint32_t flags;
    uint32_t align;
} elf_program_header_t;
