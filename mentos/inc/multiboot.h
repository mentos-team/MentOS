/// @file   multiboot.h
/// @brief  Data structures used for multiboot.
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

// Do not inc here in boot.s.

#pragma once

#include "stdint.h"

#define MULTIBOOT_HEADER_MAGIC     0x1BADB002U ///< The magic field should contain this.
#define MULTIBOOT_BOOTLOADER_MAGIC 0x2BADB002U ///< This should be in %eax.

#define MULTIBOOT_FLAG_MEM              0x00000001U ///< Is there basic lower/upper memory information?
#define MULTIBOOT_FLAG_DEVICE           0x00000002U ///< Is there a boot device set?
#define MULTIBOOT_FLAG_CMDLINE          0x00000004U ///< Is the command-line defined?
#define MULTIBOOT_FLAG_MODS             0x00000008U ///< Are there modules to do something with?
#define MULTIBOOT_FLAG_AOUT             0x00000010U ///< Is there a symbol table loaded?
#define MULTIBOOT_FLAG_ELF              0x00000020U ///< Is there an ELF section header table?
#define MULTIBOOT_FLAG_MMAP             0x00000040U ///< Is there a full memory map?
#define MULTIBOOT_FLAG_DRIVE_INFO       0x00000080U ///< Is there drive info?
#define MULTIBOOT_FLAG_CONFIG_TABLE     0x00000100U ///< Is there a config table?
#define MULTIBOOT_FLAG_BOOT_LOADER_NAME 0x00000200U ///< Is there a boot loader name?
#define MULTIBOOT_FLAG_APM_TABLE        0x00000400U ///< Is there a APM table?
#define MULTIBOOT_FLAG_VBE_INFO         0x00000800U ///< Is there video information?
#define MULTIBOOT_FLAG_FRAMEBUFFER_INFO 0x00001000U ///< Is there a framebuffer table?

#define MULTIBOOT_MEMORY_AVAILABLE 1 ///< The memory is available.
#define MULTIBOOT_MEMORY_RESERVED  2 ///< The memory is reserved.

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
typedef struct multiboot_aout_symbol_table_t {
    /// TODO: Comment.
    uint32_t tabsize;
    /// TODO: Comment.
    uint32_t strsize;
    /// TODO: Comment.
    uint32_t addr;
    /// TODO: Comment.
    uint32_t reserved;
} multiboot_aout_symbol_table_t;

/// The section header table for ELF.
typedef struct multiboot_elf_section_header_table_t {
    /// TODO: Comment.
    uint32_t num;
    /// TODO: Comment.
    uint32_t size;
    /// TODO: Comment.
    uint32_t addr;
    /// TODO: Comment.
    uint32_t shndx;
} multiboot_elf_section_header_table_t;

/// @brief Stores information about a module.
typedef struct multiboot_module_t {
    // The memory used goes from bytes 'mod_start' to 'mod_end-1' inclusive.
    /// The starting address of the modules.
    uint32_t mod_start;
    /// The ending address of the modules.
    uint32_t mod_end;
    /// Module command line.
    uint32_t cmdline;
    /// Padding to take it to 16 bytes (must be zero).
    uint32_t pad;
} multiboot_module_t;

/// @brief Stores information about memory mapping.
typedef struct multiboot_memory_map_t {
    /// Size of this entry.
    uint32_t size;
    /// Lower bytes of the base address.
    uint32_t base_addr_low;
    /// Higher bytes of the base address.
    uint32_t base_addr_high;
    /// Lower bytes of the length.
    uint32_t length_low;
    /// Higher bytes of the length.
    uint32_t length_high;
    /// Memory type.
    uint32_t type;
} multiboot_memory_map_t;

/// @brief Multiboot information structure.
typedef struct multiboot_info_t {
    /// Multiboot info version number.
    uint32_t flags;
    /// Lower memory available from the BIOS.
    uint32_t mem_lower;
    /// Upper memory available from the BIOS.
    uint32_t mem_upper;
    /// Boot device ID.
    uint32_t boot_device;
    /// Pointer to the boot command line.
    uint32_t cmdline;
    /// Amount of modules loaded.
    uint32_t mods_count;
    /// Address to the first module structure.
    uint32_t mods_addr;
    /// Contains data either of an aout or elf file.
    union {
        /// Symbol table for `aout` file.
        multiboot_aout_symbol_table_t aout_sym;
        /// Elf section header table for `elf` file.
        multiboot_elf_section_header_table_t elf_sec;
    } u;
    /// Memory map length.
    uint32_t mmap_length;
    /// Memory map address.
    uint32_t mmap_addr;
    /// Drive map length.
    uint32_t drives_length;
    /// Drive map address.
    uint32_t drives_addr;
    /// ROM configuration table.
    uint32_t config_table;
    /// Boot Loader Name.
    uint32_t boot_loader_name;
    /// APM table.
    uint32_t apm_table;
    /// Pointer to the VBE control info structure.
    uint32_t vbe_control_info;
    /// Pointer to the VBE mode info structure.
    uint32_t vbe_mode_info;
    /// Current VBE mode.
    uint32_t vbe_mode;
    /// VBE3.0 interface segment.
    uint32_t vbe_interface_seg;
    /// VBE3.0 interface segment offset.
    uint32_t vbe_interface_off;
    /// VBE3.0 interface segment length.
    uint32_t vbe_interface_len;
    /// TODO: Comment.
    uint32_t framebuffer_addr;
    /// TODO: Comment.
    uint32_t framebuffer_pitch;
    /// TODO: Comment.
    uint32_t framebuffer_width;
    /// TODO: Comment.
    uint32_t framebuffer_height;
    /// TODO: Comment.
    uint32_t framebuffer_bpp;
    /// TODO: Comment.
    uint32_t framebuffer_type;
    /// TODO: Comment.
    union {
        /// TODO: Comment.
        struct {
            /// TODO: Comment.
            uint32_t framebuffer_palette_addr;
            /// TODO: Comment.
            uint16_t framebuffer_palette_num_colors;
        } palette_field;
        /// TODO: Comment.
        struct {
            /// TODO: Comment.
            uint8_t framebuffer_red_field_position;
            /// TODO: Comment.
            uint8_t framebuffer_red_mask_size;
            /// TODO: Comment.
            uint8_t framebuffer_green_field_position;
            /// TODO: Comment.
            uint8_t framebuffer_green_mask_size;
            /// TODO: Comment.
            uint8_t framebuffer_blue_field_position;
            /// TODO: Comment.
            uint8_t framebuffer_blue_mask_size;
        } rgb_field;
    } framebuffer_info;
} multiboot_info_t;

/// @brief Returns the first mmap entry.
/// @param info The multiboot info from which we extract the entry.
/// @return The first entry.
multiboot_memory_map_t *mmap_first_entry(multiboot_info_t *info);

/// @brief The first entry of the given type.
/// @param info  The multiboot info from which we extract the entry.
/// @param type  The type of entry we are looking for.
/// @return The first entry of the given type.
multiboot_memory_map_t *mmap_first_entry_of_type(multiboot_info_t *info, uint32_t type);

/// @brief Returns the next mmap entry.
/// @param info  The multiboot info from which we extract the entry.
/// @param entry The current entry.
/// @return The next entry.
multiboot_memory_map_t *mmap_next_entry(multiboot_info_t *info, multiboot_memory_map_t *entry);

/// @brief Returns the next mmap entry of the given type.
/// @param info  The multiboot info from which we extract the entry.
/// @param entry The current entry.
/// @param type  The type of entry we are looking for.
/// @return The next entry of the given type.
multiboot_memory_map_t *mmap_next_entry_of_type(multiboot_info_t *info, multiboot_memory_map_t *entry, uint32_t type);

/// @brief Returns the type of the entry as string.
/// @param entry The current entry.
/// @return String representing the type of entry.
char *mmap_type_name(multiboot_memory_map_t *entry);

/// @brief Finds the first module.
/// @param info  The multiboot info from which we extract the information.
/// @return Pointer to the first module.
multiboot_module_t *first_module(multiboot_info_t *info);

/// @brief Finds the next module after the one we provide.
/// @param info  The multiboot info from which we extract the information.
/// @param mod   The current module.
/// @return Pointer to the next module.
multiboot_module_t *next_module(multiboot_info_t *info, multiboot_module_t *mod);

/// @brief Prints as debugging output the info regarding the multiboot.
/// @param mboot_ptr The pointer to the multiboot information.
void dump_multiboot(multiboot_info_t *mboot_ptr);
