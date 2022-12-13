/// @file elf.c
/// @brief Function for multiboot support.
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

// Include the kernel log levels.
#include "sys/kernel_levels.h"
/// Change the header.
#define __DEBUG_HEADER__ "[ELF   ]"
/// Set the log level.
#define __DEBUG_LEVEL__ LOGLEVEL_NOTICE

#include "elf/elf.h"

#include "process/scheduler.h"
#include "mem/vmem_map.h"
#include "process/process.h"
#include "string.h"
#include "stddef.h"
#include "io/debug.h"
#include "stdio.h"
#include "mem/slab.h"
#include "fs/vfs.h"
#include "assert.h"

// ============================================================================
// GET ELF TABLES
// ============================================================================

/// @brief Returns the pointer to where the section headers reside.
/// @param header a pointer to the ELF header.
/// @return a pointer to the section headers table.
static inline elf_section_header_t *elf_get_section_header_table(elf_header_t *header)
{
    return (elf_section_header_t *)((uintptr_t)header + header->shoff);
}

/// @brief Returns the pointer to where the program headers reside.
/// @param header a pointer to the ELF header.
/// @return a pointer to the program headers table.
static inline elf_program_header_t *elf_get_program_header_table(elf_header_t *header)
{
    return (elf_program_header_t *)((uintptr_t)header + header->phoff);
}

// ============================================================================
// GET ELF OBJECTS
// ============================================================================

/// @brief Returns a pointer to the desired section header.
/// @param header a pointer to the ELF header.
/// @param idx  The index of the section header.
/// @param shdr Where we store the content we read.
/// @return a pointer to the desired section header.
static inline elf_section_header_t *elf_get_section_header(elf_header_t *header, unsigned idx)
{
    return &elf_get_section_header_table(header)[idx];
}

/// @brief Returns a pointer to the desired program header.
/// @param header a pointer to the ELF header.
/// @param idx the index of the program header.
/// @return a pointer to the desired section header.
static inline elf_program_header_t *elf_get_program_header(elf_header_t *header, unsigned idx)
{
    return &elf_get_program_header_table(header)[idx];
}

// ============================================================================
// GET STRING TABLES
// ============================================================================

/// @brief Returns a pointer to the section header string table.
/// @param header a pointer to the ELF header.
/// @return a pointer to the section header string table, or NULL on failure.
static inline const char *elf_get_section_header_string_table(elf_header_t *header)
{
    if (header->shstrndx == SHT_NULL)
        return NULL;
    return (const char *)((uintptr_t)header + elf_get_section_header(header, header->shstrndx)->offset);
}

/// @brief Returns a pointer to the section header string table.
/// @param header a pointer to the ELF header.
/// @return a pointer to the section header string table, or NULL on failure.
static inline const char *elf_get_symbol_string_table(elf_header_t *header, elf_section_header_t *section_header)
{
    if (section_header->link == SHT_NULL)
        return NULL;
    return (const char *)((uintptr_t)header + elf_get_section_header(header, section_header->link)->offset);
}

// ============================================================================
// GET ELF OBJECTS NAME
// ============================================================================

/// @brief Returns the name of the given entry in the section header string table.
/// @param header a pointer to the ELF header.
/// @param name_offset the offset where the desired name resides inside the table.
/// @return a pointer to the name, or NULL on failure.
static inline const char *elf_get_section_header_name(elf_header_t *header, elf_section_header_t *section_header)
{
    const char *strtab = elf_get_section_header_string_table(header);
    if (strtab == NULL)
        return NULL;
    return strtab + section_header->name;
}

/// @brief Returns a pointer to the section header string table.
/// @param header a pointer to the ELF header.
/// @return a pointer to the section header string table, or NULL on failure.
static inline const char *elf_get_symbol_name(elf_header_t *header, elf_section_header_t *section_header, elf_symbol_t *symbol)
{
    const char *strtab = elf_get_symbol_string_table(header, section_header);
    if (strtab == NULL)
        return NULL;
    return strtab + symbol->name;
}

// ============================================================================
// SEARCH FUNCTIONS
// ============================================================================

static inline elf_section_header_t *elf_find_section_header(elf_header_t *header, const char *name)
{
    for (unsigned i = 0; i < header->shnum; ++i) {
        // Get the section header.
        elf_section_header_t *section_header = elf_get_section_header(header, i);
        // Get the section header name.
        const char *section_header_name = elf_get_section_header_name(header, section_header);
        if (section_header_name) {
            // Check the section header name.
            if (strcmp(section_header_name, name) == 0) {
                return section_header;
            }
        }
    }
    return NULL;
}

static inline elf_symbol_t *elf_find_symbol(elf_header_t *header, const char *name)
{
    for (unsigned i = 0; i < header->shnum; ++i) {
        // Get the section header.
        elf_section_header_t *section_header = elf_get_section_header(header, i);
        // Check if it is valid, and it is a symbol table.
        if (section_header && (section_header->type == SHT_SYMTAB)) {
            // Count the number of entries.
            unsigned symtab_entries = section_header->size / section_header->entsize;
            // Get the addresss of the symbol table.
            elf_symbol_t *symtab = (elf_symbol_t *)((uintptr_t)header + section_header->offset);
            // Iterate the entries.
            for (unsigned j = 0; j < symtab_entries; ++j) {
                // Get the symbol.
                elf_symbol_t *symbol = &symtab[j];
                // Get the name of the symbol.
                const char *symbol_name = elf_get_symbol_name(header, section_header, symbol);
                if (symbol_name) {
                    // Check the symbol name.
                    if (strcmp(symbol_name, name) == 0) {
                        return symbol;
                    }
                }
            }
        }
    }
    return NULL;
}

// ============================================================================
// DUMP FUNCTIONS
// ============================================================================

static inline void elf_dump_section_headers(elf_header_t *header)
{
    pr_debug("[Nr] Name                 Type            Addr     Off    Size   ES Flg Lk Inf Al\n");
    for (unsigned idx = 0; idx < header->shnum; ++idx) {
        // Get the section header.
        elf_section_header_t *section_header = elf_get_section_header(header, idx);
        // Get the section header name.
        const char *section_header_name = elf_get_section_header_name(header, section_header);
        // Dump the information.
        pr_debug("[%2d] %-20s %-15s %08x %06x %06x %2u %3u %2u %3u %2u\n",
                 idx, section_header_name, elf_section_header_type_to_string(section_header->type),
                 section_header->addr, section_header->offset, section_header->size,
                 section_header->entsize, section_header->flags, section_header->link,
                 section_header->info, section_header->addralign);
    }
}

static inline void elf_dump_symbol_table(elf_header_t *header)
{
    for (unsigned i = 0; i < header->shnum; ++i) {
        // Get the section header.
        elf_section_header_t *section_header = elf_get_section_header(header, i);
        if (section_header->type != SHT_SYMTAB)
            continue;
        // Count the number of entries.
        uint32_t symtab_entries = section_header->size / section_header->entsize;
        // Get the addresss of the symbol table.
        elf_symbol_t *symtab = (elf_symbol_t *)((uintptr_t)header + section_header->offset);
        // Dump the table.
        for (int j = 0; j < symtab_entries; ++j) {
            // Get the symbol.
            elf_symbol_t *symbol = &symtab[j];
            // Get the name of the symbol.
            const char *symbol_name = elf_get_symbol_name(header, section_header, symbol);
            if (symbol_name == NULL) {
                pr_err("Null symbol name.\n");
                return;
            }
            // Dump the symbol.
            pr_debug("[%4d] %08x %5d %-7s %-6s %-8s %3d %s\n", j, symbol->value, symbol->size,
                     elf_symbol_type_to_string(ELF32_ST_TYPE(symbol->info)),
                     elf_symbol_bind_to_string(ELF32_ST_BIND(symbol->info)),
                     "-", symbol->ndx, symbol_name);
        }
    }
}

// ============================================================================
// EXEC-RELATED FUNCTIONS
// ============================================================================

/// @brief Loads an ELF executable.
/// @param task The task for which we load the ELF.
/// @param file The ELF file.
/// @param header  The header of the ELF file.
/// @return The ELF entry.
static inline int elf_load_exec(elf_header_t *header, task_struct *task)
{
    pr_debug(" Type      | Mem. Size | File Size | VADDR\n");
    for (unsigned i = 0; i < header->phnum; ++i) {
        // Get the header.
        elf_program_header_t *program_header = elf_get_program_header(header, i);
        // Dump the information about the header.
        pr_debug(" %-9s | %9s | %9s | 0x%08x - 0x%08x\n",
                 elf_type_to_string(program_header->type),
                 to_human_size(program_header->memsz),
                 to_human_size(program_header->filesz),
                 program_header->vaddr,
                 program_header->vaddr + program_header->memsz);
        if (program_header->type == PT_LOAD) {
            uint32_t virt_addr     = create_vm_area(task->mm, program_header->vaddr, program_header->memsz, MM_USER | MM_RW | MM_COW, GFP_KERNEL);
            virt_map_page_t *vpage = virt_map_alloc(program_header->memsz);
            uint32_t dst_addr      = virt_map_vaddress(task->mm, vpage, virt_addr, program_header->memsz);

            // Load the memory area.
            memcpy((void *)dst_addr, (void *)((uintptr_t)header + program_header->offset), program_header->filesz);

            if (program_header->memsz > program_header->filesz) {
                uint32_t zmem_sz = program_header->memsz - program_header->filesz;
                memset((void *)(dst_addr + program_header->filesz), 0, zmem_sz);
            }
            virt_unmap_pg(vpage);
        }
    }
    return true;
}

int elf_load_file(task_struct *task, vfs_file_t *file, uint32_t *entry)
{
    // Open the file.
    if (file == NULL)
        return false;
    // Get the size of the file.
    stat_t stat_buf;
    if (vfs_fstat(file, &stat_buf) < 0) {
        pr_err("Failed to stat the file `%s`.\n", file->name);
        return false;
    }
    // Allocate the memory for the file.
    char *buffer = kmalloc(stat_buf.st_size);
    if (buffer == NULL) {
        pr_err("Failed to allocate %d bytes of memory for reading the file `%s`.\n", stat_buf.st_size, file->name);
        return false;
    }
    // Clean the memory.
    memset(buffer, 0, stat_buf.st_size);
    // Read the file.
    if (vfs_read(file, buffer, 0, stat_buf.st_size) != stat_buf.st_size) {
        pr_err("Failed to read %d bytes from the file `%s`.\n", stat_buf.st_size, file->name);
        goto return_error_free_buffer;
    }
    // The first thing inside the file is the ELF header.
    elf_header_t *header = (elf_header_t *)buffer;
    // Print header info.
    pr_debug("Type           : %s\n", elf_type_to_string(header->type));
    pr_debug("Version        : 0x%x\n", header->version);
    pr_debug("Entry          : 0x%x\n", header->entry);
    pr_debug("Headers offset : 0x%x\n", header->phoff);
    pr_debug("Headers count  : %d\n", header->phnum);
    // Check the elf header.
    if (!elf_check_file_header(header)) {
        pr_err("File %s is not a valid ELF file.\n", stat_buf.st_size, file->name);
        goto return_error_free_buffer;
    }
    // Check if the elf file is an executable.
    if (header->type != ET_EXEC) {
        pr_err("Elf file is not an executable.\n");
        goto return_error_free_buffer;
    }
    if (!elf_load_exec(header, task)) {
        pr_err("Failed to load the executable.\n");
        goto return_error_free_buffer;
    }

    // Set the entry.
    (*entry) = header->entry;

    kfree(buffer);
    return true;
return_error_free_buffer:
    kfree(buffer);
    return false;
}

int elf_check_file_type(vfs_file_t *file, Elf_Type type)
{
    // Open the file.
    if (file == NULL) {
        pr_err("Cannot find executable!");
        return 0;
    }
    // Set the reading position at the beginning of the file.
    vfs_lseek(file, 0, SEEK_SET);
    // Prepare the elf header.
    elf_header_t header;
    // By default we return failure.
    int ret = 0;
    // Read the header and check the file type.
    if (vfs_read(file, &header, 0, sizeof(elf_header_t)) != -1)
        if (elf_check_file_header(&header))
            ret = header.type == type;
    // Set the reading position at the beginning of the file.
    vfs_lseek(file, 0, SEEK_SET);
    return ret;
}

int elf_check_file_header(elf_header_t *header)
{
    if (!elf_check_magic_number(header)) {
        pr_err("Invalid ELF File.\n");
        return false;
    }
    if (header->ident[EI_CLASS] != ELFCLASS32) {
        pr_err("Unsupported ELF File Class.\n");
        return false;
    }
    if (header->ident[EI_DATA] != ELFDATA2LSB) {
        pr_err("Unsupported ELF File byte order.\n");
        return false;
    }
    if (header->machine != EM_386) {
        pr_err("Unsupported ELF File target.\n");
        return false;
    }
    if (header->ident[EI_VERSION] != EV_CURRENT) {
        pr_err("Unsupported ELF File version.\n");
        return false;
    }
    if (header->type != ET_EXEC) {
        pr_err("Unsupported ELF File type.\n");
        return false;
    }
    return true;
}

int elf_check_magic_number(elf_header_t *header)
{
    if (!header)
        return false;
    if (header->ident[EI_MAG0] != ELFMAG0) {
        pr_err("ELF Header EI_MAG0 incorrect.\n");
        return false;
    }
    if (header->ident[EI_MAG1] != ELFMAG1) {
        pr_err("ELF Header EI_MAG1 incorrect.\n");
        return false;
    }
    if (header->ident[EI_MAG2] != ELFMAG2) {
        pr_err("ELF Header EI_MAG2 incorrect.\n");
        return false;
    }
    if (header->ident[EI_MAG3] != ELFMAG3) {
        pr_err("ELF Header EI_MAG3 incorrect.\n");
        return false;
    }
    return true;
}

const char *elf_type_to_string(int type)
{
    if (type == PT_LOAD)
        return "LOAD";
    if (type == PT_DYNAMIC)
        return "DYNAMIC";
    if (type == PT_INTERP)
        return "INTERP";
    if (type == PT_NOTE)
        return "NOTE";
    if (type == PT_SHLIB)
        return "SHLIB";
    if (type == PT_PHDR)
        return "PHDR";
    if (type == PT_EH_FRAME)
        return "EH_FRAME";
    if (type == PT_GNU_STACK)
        return "GNU_STACK";
    if (type == PT_GNU_RELRO)
        return "GNU_RELRO";
    if (type == PT_LOPROC)
        return "LOPROC";
    if (type == PT_HIPROC)
        return "HIPROC";
    return "NULL";
}

const char *elf_section_header_type_to_string(int type)
{
    if (type == SHT_PROGBITS)
        return "PROGBITS";
    if (type == SHT_SYMTAB)
        return "SYMTAB";
    if (type == SHT_STRTAB)
        return "STRTAB";
    if (type == SHT_RELA)
        return "RELA";
    if (type == SHT_NOBITS)
        return "NOBITS";
    if (type == SHT_REL)
        return "REL";
    return "NULL";
}

const char *elf_symbol_type_to_string(int type)
{
    if (type == STT_NOTYPE)
        return "NOTYPE";
    if (type == STT_OBJECT)
        return "OBJECT";
    if (type == STT_FUNC)
        return "FUNC";
    return "-1";
}

const char *elf_symbol_bind_to_string(int bind)
{
    if (bind == STB_LOCAL)
        return "LOCAL";
    if (bind == STB_GLOBAL)
        return "GLOBAL";
    if (bind == STB_WEAK)
        return "WEAK";
    return "-1";
}
