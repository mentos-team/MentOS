///                MentOS, The Mentoring Operating system project
/// @file   multiboot.h
/// @brief  Data structures used for multiboot.
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

// Do not inc here in boot.s.

#pragma once

#include "stdint.h"

#define MULTIBOOT_FLAG_MEM     0x001

#define MULTIBOOT_FLAG_DEVICE  0x002

#define MULTIBOOT_FLAG_CMDLINE 0x004

#define MULTIBOOT_FLAG_MODS    0x008

#define MULTIBOOT_FLAG_AOUT    0x010

#define MULTIBOOT_FLAG_ELF     0x020

#define MULTIBOOT_FLAG_MMAP    0x040

#define MULTIBOOT_FLAG_CONFIG  0x080

#define MULTIBOOT_FLAG_LOADER  0x100

#define MULTIBOOT_FLAG_APM     0x200

#define MULTIBOOT_FLAG_VBE     0x400

#define MULTIBOOT_FRAMEBUFFER_TYPE_INDEXED  0

#define MULTIBOOT_FRAMEBUFFER_TYPE_RGB      1

#define MULTIBOOT_FRAMEBUFFER_TYPE_EGA_TEXT 2

#define MULTIBOOT_MEMORY_AVAILABLE              1

#define MULTIBOOT_MEMORY_RESERVED               2

#define MULTIBOOT_MEMORY_ACPI_RECLAIMABLE       3

#define MULTIBOOT_MEMORY_NVS                    4

#define MULTIBOOT_MEMORY_BADRAM                 5

//            +-------------------+
//    0       | flags             |    (required)
//            +-------------------+
//    4       | mem_lower         |    (present if flags[0] is set)
//    8       | mem_upper         |    (present if flags[0] is set)
//            +-------------------+
//    12      | boot_device       |    (present if flags[1] is set)
//            +-------------------+
//    16      | cmdline           |    (present if flags[2] is set)
//            +-------------------+
//    20      | mods_count        |    (present if flags[3] is set)
//    24      | mods_addr         |    (present if flags[3] is set)
//            +-------------------+
//    28 - 40 | syms              |    (present if flags[4] or
//            |                   |                flags[5] is set)
//            +-------------------+
//    44      | mmap_length       |    (present if flags[6] is set)
//    48      | mmap_addr         |    (present if flags[6] is set)
//            +-------------------+
//    52      | drives_length     |    (present if flags[7] is set)
//    56      | drives_addr       |    (present if flags[7] is set)
//            +-------------------+
//    60      | config_table      |    (present if flags[8] is set)
//            +-------------------+
//    64      | boot_loader_name  |    (present if flags[9] is set)
//            +-------------------+
//    68      | apm_table         |    (present if flags[10] is set)
//            +-------------------+
//    72      | vbe_control_info  |    (present if flags[11] is set)
//    76      | vbe_mode_info     |
//    80      | vbe_mode          |
//    82      | vbe_interface_seg |
//    84      | vbe_interface_off |
//    86      | vbe_interface_len |
//            +-------------------+
//    88      | framebuffer_addr  |    (present if flags[12] is set)
//    96      | framebuffer_pitch |
//    100     | framebuffer_width |
//    104     | framebuffer_height|
//    108     | framebuffer_bpp   |
//    109     | framebuffer_type  |
//    110-115 | color_info        |
//            +-------------------+

/// The symbol table for a.out.
typedef struct multiboot_aout_symbol_table
{
    uint32_t tabsize;
    uint32_t strsize;
    uint32_t addr;
    uint32_t reserved;
} multiboot_aout_symbol_table_t;

/// The section header table for ELF.
typedef struct multiboot_elf_section_header_table
{
    uint32_t num;
    uint32_t size;
    uint32_t addr;
    uint32_t shndx;
} multiboot_elf_section_header_table_t;

// TODO: doxygen comment.
typedef struct multiboot_mod_list
{
    // The memory used goes from bytes 'mod_start' to 'mod_end-1' inclusive.
    uint32_t mod_start;
    uint32_t mod_end;
    // Module command line.
    uint32_t cmdline;
    // Padding to take it to 16 bytes (must be zero).
    uint32_t pad;
} multiboot_module_t;

// TODO: doxygen comment.
typedef struct multiboot_mmap_entry
{
    uint32_t size;
    uint64_t addr;
    uint64_t len;
    uint32_t type;
} __attribute__((packed)) multiboot_memory_map_t;

// TODO: doxygen comment.
typedef struct multiboot_info
{
    /// Multiboot info version number.
    uint32_t flags;

    /// Available memory from BIOS.
    uint32_t mem_lower;
    uint32_t mem_upper;

    /// "root" partition.
    uint32_t boot_device;

    /// Kernel command line.
    uint32_t cmdline;

    /// Boot-Module list.
    uint32_t mods_count;
    uint32_t mods_addr;

    union
    {
        multiboot_aout_symbol_table_t aout_sym;
        multiboot_elf_section_header_table_t elf_sec;
    } u;

    /// Memory Mapping buffer.
    uint32_t mmap_length;
    uint32_t mmap_addr;

    /// Drive Info buffer.
    uint32_t drives_length;
    uint32_t drives_addr;

    /// ROM configuration table.
    uint32_t config_table;

    /// Boot Loader Name.
    uint32_t boot_loader_name;

    /// APM table.
    uint32_t apm_table;

    /// Video.
    uint32_t vbe_control_info;
    uint32_t vbe_mode_info;
    uint32_t vbe_mode;
    uint32_t vbe_interface_seg;
    uint32_t vbe_interface_off;
    uint32_t vbe_interface_len;

    uint32_t framebuffer_addr;
    uint32_t framebuffer_pitch;
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint32_t framebuffer_bpp;
    uint32_t framebuffer_type;
    union
    {
        struct
        {
            uint32_t framebuffer_palette_addr;
            uint16_t framebuffer_palette_num_colors;
        };
        struct
        {
            uint8_t framebuffer_red_field_position;
            uint8_t framebuffer_red_mask_size;
            uint8_t framebuffer_green_field_position;
            uint8_t framebuffer_green_mask_size;
            uint8_t framebuffer_blue_field_position;
            uint8_t framebuffer_blue_mask_size;
        };
    };
} __attribute__((packed)) multiboot_info_t;

// Be careful that the offset 0 is base_addr_low but no size.
/// @brief The memory map.
typedef struct memory_map
{
    uint32_t size;
    uint32_t base_addr_low;
    uint32_t base_addr_high;
    uint32_t length_low;
    uint32_t length_high;
    uint32_t type;
} memory_map_t;

// TODO: doxygen comment.
void dump_multiboot(multiboot_info_t *mboot_ptr);
