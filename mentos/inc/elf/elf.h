/// @file elf.h
/// @brief Function for managing the Executable and Linkable Format (ELF).
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "process/process.h"
#include "stdint.h"

/// @defgroup header_segment_types Program Header Segment Types
/// @brief List of numeric defines which identify segment entries types.
/// @{

/// @brief Unused.
#define PT_NULL 0
/// @brief Specifies a loadable segment, described by p_filesz and p_memsz.
/// The bytes from the file are mapped to the beginning of the memory segment.
/// If the segment's memory size (p_memsz) is larger than the file
/// size (p_filesz), the extra bytes are defined to hold the value 0
/// and to follow the segment's initialized area. The file size can not
/// be larger than the memory size. Loadable segment entries in the program
/// header table appear in ascending order, sorted on the p_vaddr member.
#define PT_LOAD 1
/// @brief Specifies dynamic linking information.
#define PT_DYNAMIC 2
/// @brief Specifies the location and size of a null-terminated path name
/// to invoke as an interpreter. This segment type is mandatory for dynamic
/// executable files and can occur in shared objects. It cannot occur more
/// than once in a file. This type, if present, it must precede any loadable
/// segment entry.
#define PT_INTERP 3
/// @brief Specifies the location and size of auxiliary information.
#define PT_NOTE 4
/// @brief Reserved but has unspecified semantics.
#define PT_SHLIB 5
/// @brief Specifies the location and size of the program header table
/// itself, both in the file and in the memory image of the program.
/// This segment type cannot occur more than once in a file. Moreover,
/// it can occur only if the program header table is part of the memory
/// image of the program. This type, if present, must precede any loadable
/// segment entry.
#define PT_PHDR 6
/// @brief Section for supporting exception handling routines.
/// @details
/// The .eh_frame section has the same structure with .debug_frame,
/// which follows DWARF format. It represents the table that describes
/// how to set registers to restore the previous call frame at runtime.
#define PT_EH_FRAME 0x6474E550
/// @brief Is a program header which tells the system how to control
/// the stack when the ELF is loaded into memory.
#define PT_GNU_STACK 0x6474E551
/// @brief This segment indicates the memory region which should be made
/// Read-Only after relocation is done. This segment usually appears in a
/// dynamic link library and it contains .ctors, .dtors, .dynamic, .got s
/// ections. See paragraph below.
#define PT_GNU_RELRO 0x6474E552
/// @brief TODO: Document.
#define PT_LOPROC 0x70000000
/// @brief TODO: Document.
#define PT_HIPROC 0x7FFFFFFF

/// @}

/// Elf header ident size.
#define EI_NIDENT 16

/// @brief The elf starting section.
typedef struct elf_header {
    /// Elf header identity bits.
    uint8_t ident[EI_NIDENT];
    /// Identifies object file type.
    uint16_t type;
    /// Specifies target instruction set architecture.
    uint16_t machine;
    /// Set to 1 for the original version of ELF.
    uint32_t version;
    /// This is the memory address of the entry point from where the process starts executing.
    uint32_t entry;
    /// Points to the start of the program header table.
    uint32_t phoff;
    /// Points to the start of the section header table.
    uint32_t shoff;
    /// Processor-specific flags.
    uint32_t flags;
    /// Size of ELF header, in bytes.
    uint16_t ehsize;
    /// Size of an entry in the program header table.
    uint16_t phentsize;
    /// Number of entries in the program header table.
    uint16_t phnum;
    /// Size of an entry in the section header table.
    uint16_t shentsize;
    /// Number of entries in the section header table.
    uint16_t shnum;
    /// Section header table index of sect name string table.
    uint16_t shstrndx;
} elf_header_t;

/// @brief The elf program header, holding program layout information
typedef struct elf_program_header {
    /// Identifies the type of the segment.
    uint32_t type;
    /// Offset of the segment in the file image.
    uint32_t offset;
    /// Virtual address of the segment in memory.
    uint32_t vaddr;
    /// On systems where physical address is relevant, reserved for
    /// segment's physical address.
    uint32_t paddr;
    /// Size in bytes of the segment in the file image. May be 0.
    uint32_t filesz;
    /// Size in bytes of the segment in memory. May be 0.
    uint32_t memsz;
    /// Segment-dependent flags.
    uint32_t flags;
    /// Values of 0 or 1 specify no alignment. Otherwise should be a positive,
    /// integral power of 2, with p_vaddr equating p_offset modulus p_align.
    uint32_t align;
} elf_program_header_t;

/// @brief A section header with all kinds of useful information
typedef struct elf_section_header {
    /// TODO: Comment.
    uint32_t name;
    /// TODO: Comment.
    uint32_t type;
    /// TODO: Comment.
    uint32_t flags;
    /// TODO: Comment.
    uint32_t addr;
    /// TODO: Comment.
    uint32_t offset;
    /// TODO: Comment.
    uint32_t size;
    /// TODO: Comment.
    uint32_t link;
    /// TODO: Comment.
    uint32_t info;
    /// TODO: Comment.
    uint32_t addralign;
    /// TODO: Comment.
    uint32_t entsize;
} elf_section_header_t;

/// @brief A symbol itself.
typedef struct elf_symbol {
    /// TODO: Comment.
    uint32_t name;
    /// TODO: Comment.
    uint32_t value;
    /// TODO: Comment.
    uint32_t size;
    /// TODO: Comment.
    uint8_t info;
    /// TODO: Comment.
    uint8_t other;
    /// TODO: Comment.
    uint16_t ndx;
} elf_symbol_t;

/// @brief Holds information about relocation object (that do not need an addend).
typedef struct elf_rel_t {
    /// TODO: Comment.
    uint32_t r_offset;
    /// TODO: Comment.
    uint32_t r_info;
} elf_rel_t;

/// @brief Holds information about relocation object (that need an addend).
typedef struct elf_rela_t {
    /// TODO: Comment.
    uint32_t r_offset;
    /// TODO: Comment.
    uint32_t r_info;
    /// TODO: Comment.
    int32_t r_addend;
} elf_rela_t;

/// @brief Fields index of ELF_IDENT.
enum Elf_Ident {
    EI_MAG0       = 0, ///< 0x7F
    EI_MAG1       = 1, ///< 'E'
    EI_MAG2       = 2, ///< 'L'
    EI_MAG3       = 3, ///< 'F'
    EI_CLASS      = 4, ///< Architecture (32/64)
    EI_DATA       = 5, ///< Set to either 1 or 2 to signify little or big endianness.
    EI_VERSION    = 6, ///< ELF Version
    EI_OSABI      = 7, ///< OS Specific
    EI_ABIVERSION = 8, ///< OS Specific
    EI_PAD        = 9  ///< Padding
};

#define ELFMAG0 0x7F ///< e_ident[EI_MAG0]
#define ELFMAG1 'E' ///< e_ident[EI_MAG1]
#define ELFMAG2 'L' ///< e_ident[EI_MAG2]
#define ELFMAG3 'F' ///< e_ident[EI_MAG3]

#define ELFDATA2LSB 1 ///< Little Endian
#define ELFCLASS32  1 ///< 32-bit Architecture

/// @brief Type of ELF files.
typedef enum Elf_Type {
    ET_NONE = 0, ///< Unkown Type
    ET_REL  = 1, ///< Relocatable File
    ET_EXEC = 2  ///< Executable File
} Elf_Type;

#define EM_386     3 ///< x86 Machine Type.
#define EV_CURRENT 1 ///< ELF Current Version.

/// @brief Defines a number of different types of sections, which correspond
/// to values stored in the field sh_type in the section header
typedef enum ShT_Types {
    SHT_NULL     = 0, ///< Null section
    SHT_PROGBITS = 1, ///< Program information
    SHT_SYMTAB   = 2, ///< Symbol table
    SHT_STRTAB   = 3, ///< String table
    SHT_RELA     = 4, ///< Relocation (w/ addend)
    SHT_NOBITS   = 8, ///< Not present in file
    SHT_REL      = 9, ///< Relocation (no addend)
} ShT_Types;

/// @brief ShT_Attributes corresponds to the field sh_flags, but are bit
/// flags rather than stand-alone values.
enum ShT_Attributes {
    SHF_WRITE = 0x01, ///< Writable section
    SHF_ALLOC = 0x02  ///< Exists in memory
};

/// @brief Provide access to teh symbol biding.
#define ELF32_ST_BIND(INFO) ((INFO) >> 4)
/// @brief Provide access to teh symbol type.
#define ELF32_ST_TYPE(INFO) ((INFO)&0x0F)

/// @brief Provides possible symbol bindings.
enum StT_Bindings {
    STB_LOCAL  = 0, ///< Local scope
    STB_GLOBAL = 1, ///< Global scope
    STB_WEAK   = 2  ///< Weak, (ie. __attribute__((weak)))
};

/// @brief Provides a number of possible symbol types.
enum StT_Types {
    STT_NOTYPE = 0, ///< No type
    STT_OBJECT = 1, ///< Variables, arrays, etc.
    STT_FUNC   = 2  ///< Methods or functions
};

/// @brief Loads an ELF file into the memory of task.
/// @param task  The task for which we load the ELF.
/// @param file  The ELF file.
/// @param entry The ELF binary entry.
/// @return 0 if fails, 1 if succeed.
int elf_load_file(task_struct *task, vfs_file_t *file, uint32_t *entry);

/// @brief Checks if the file is a valid ELF.
/// @param file The file to check.
/// @param type The type of ELF file we expect.
/// @return 0 if fails, 1 if succeed.
int elf_check_file_type(vfs_file_t *file, Elf_Type type);

/// @brief Checks the correctness of the ELF header.
/// @param hdr The header to check.
/// @return 0 if fails, 1 if succeed.
int elf_check_file_header(elf_header_t *hdr);

/// @brief Checks the correctness of the ELF header magic number.
/// @param hdr The header to check.
/// @return 0 if fails, 1 if succeed.
int elf_check_magic_number(elf_header_t *hdr);

/// @brief Transforms the passed ELF type to string.
/// @param type The integer representing the ELF type.
/// @return The string representing the ELF type.
const char *elf_type_to_string(int type);

/// @brief Transforms the passed ELF section header type to string.
/// @param type The integer representing the ELF section header type.
/// @return The string representing the ELF section header type.
const char *elf_section_header_type_to_string(int type);

/// @brief Transforms the passed ELF symbol type to string.
/// @param type The integer representing the ELF symbol type.
/// @return The string representing the ELF symbol type.
const char *elf_symbol_type_to_string(int type);

/// @brief Transforms the passed ELF symbol bind to string.
/// @param bind The integer representing the ELF symbol bind.
/// @return The string representing the ELF symbol bind.
const char *elf_symbol_bind_to_string(int bind);
