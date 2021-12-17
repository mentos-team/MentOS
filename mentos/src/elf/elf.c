///                MentOS, The Mentoring Operating system project
/// @file elf.c
/// @brief Function for multiboot support.
/// @copyright (c) 2014-2021 This file is distributed under the MIT License.
/// See LICENSE.md for details.

/// Change the header.
#define __DEBUG_HEADER__ "[ELF   ]"
#define __DEBUG_LEVEL__  100

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

/// @brief Reads the elf header from file.
/// @param file the file from which we extract the elf header.
/// @param header where we store the content we read.
/// @return true on success, false on failure.
static inline bool_t read_elf_header(vfs_file_t *file, elf_header_t *header)
{
    size_t expected = sizeof(elf_header_t);
    ssize_t nread   = vfs_read(file, header, 0, expected);
    if (nread != expected) {
        pr_err("Failed to read elf header (read: %ld, expected: %ld)\n", nread, expected);
        return false;
    }
    return true;
}

/// @brief Reads the program header from file.
/// @param file The file from which we extract the program header.
/// @param header  A pointer to the ELF header.
/// @param idx  The index of the program header.
/// @param phdr Where we store the content we read.
/// @return The amount of bytes we read.
static inline ssize_t read_elf_program_header(vfs_file_t *file, const elf_header_t *header, unsigned idx, elf_program_header_t *phdr)
{
    return vfs_read(file, phdr, header->phoff + header->phentsize * idx, sizeof(elf_program_header_t));
}

/// @brief Reads the section header from file.
/// @param file The file from which we extract the section header.
/// @param header  A pointer to the ELF header.
/// @param idx  The index of the section header.
/// @param shdr Where we store the content we read.
/// @return The amount of bytes we read.
static inline ssize_t read_elf_section_header(vfs_file_t *file, const elf_header_t *header, unsigned idx, elf_section_header_t *shdr)
{
    return vfs_read(file, shdr, header->shoff + header->shentsize * idx, sizeof(elf_section_header_t));
}

/// @brief Reads the symbol from file.
/// @param file   The file from which we extract the symbol.
/// @param shdr   A pointer to the ELF symbol table header.
/// @param idx    The index of the symbol.
/// @param symbol Where we store the content we read.
/// @return The amount of bytes we read.
static inline ssize_t read_elf_symbol(vfs_file_t *file, const elf_section_header_t *shdr, unsigned idx, elf_symbol_t *symbol)
{
    // TODO: Here it should use `shdr->entsize`.
    return vfs_read(file, symbol, shdr->offset + sizeof(elf_symbol_t) * idx, sizeof(elf_symbol_t));
}

/// @brief Reads the symbol from file.
/// @param file   The file from which we extract the symbol.
/// @param shdr   A pointer to the ELF symbol table header.
/// @param idx    The index of the symbol.
/// @param symbol Where we store the content we read.
/// @return The amount of bytes we read.
static inline ssize_t read_elf_symbol_name(vfs_file_t *file, const elf_section_header_t *shdr, unsigned offset, char *name, size_t name_len)
{
    return vfs_read(file, name, shdr->offset + offset, name_len);
}

/// @brief Reads all program headers from file.
/// @param file the file from which we extract the program header.
/// @param header a pointer to the ELF header.
/// @return The program headers, free them when finished.
static inline elf_program_header_t *read_elf_program_headers(vfs_file_t *file, const elf_header_t *header)
{
    assert(sizeof(elf_program_header_t) == header->phentsize);
    // Compute the size.
    size_t size = header->phnum * header->phentsize;
    // Allocate the memory for the headers.
    elf_program_header_t *program_headers = (elf_program_header_t *)kmalloc(size);
    if (program_headers == NULL) {
        pr_err("Failed to allocate memory for program headers.\n");
        return NULL;
    }
    // Clean up the memory.
    memset(program_headers, 0, size);
    // Read the headers.
    if (vfs_read(file, program_headers, header->phoff, size) != size) {
        pr_err("Failed to read program headers.\n");
        kfree(program_headers);
        return NULL;
    }
    return program_headers;
}

/// @brief Reads all section headers from file.
/// @param file the file from which we extract the section header.
/// @param header a pointer to the ELF header.
/// @return The section headers, free them when finished.
static inline elf_section_header_t *read_elf_section_headers(vfs_file_t *file, const elf_header_t *header)
{
    assert(sizeof(elf_section_header_t) == header->shentsize);
    // Compute the size.
    size_t size = header->shnum * header->shentsize;
    // Allocate the memory for the headers.
    elf_section_header_t *section_headers = (elf_section_header_t *)kmalloc(size);
    if (section_headers == NULL) {
        pr_err("Failed to allocate memory for section headers.\n");
        return NULL;
    }
    // Clean up the memory.
    memset(section_headers, 0, size);
    // Read the headers.
    if (vfs_read(file, section_headers, header->shoff, size) != size) {
        pr_err("Failed to read section headers.\n");
        kfree(section_headers);
        return NULL;
    }
    return section_headers;
}

static inline char *read_elf_strtable(
    vfs_file_t *file,
    const elf_header_t *header,
    const elf_section_header_t *section_header)
{
    // Check if the header points to a string table.
    if (section_header->type != SHT_STRTAB) {
        pr_err("The header is not a STRTAB but a `%s` instead.\n", elf_type_to_string(section_header->type));
        return NULL;
    }
    // Allocate the memory for the string table.
    char *strtab = kmalloc(section_header->size);
    // Initialize the memory.
    memset(strtab, 0, section_header->size);
    // Read the string table.
    if (vfs_read(file, strtab, section_header->offset, section_header->size) != section_header->size) {
        pr_err("Failed to read the string table at %d.\n", section_header->offset);
        kfree(strtab);
        return NULL;
    }
    return strtab;
}

static inline elf_symbol_t *read_elf_symtable(
    vfs_file_t *file,
    const elf_header_t *header,
    const elf_section_header_t *section_header)
{
    // Check if the header points to a string table.
    if (section_header->type != SHT_SYMTAB) {
        pr_err("The header is not a SYMTAB but a `%s` instead.\n", elf_type_to_string(section_header->type));
        return NULL;
    }
    if (sizeof(elf_symbol_t) != section_header->entsize) {
        pr_err("Entities inside the symbol table have wrong size, %ld instead of %ld.\n",
               section_header->entsize, sizeof(elf_symbol_t));
        return NULL;
    }
    // Allocate the memory for the string table.
    elf_symbol_t *symtab = kmalloc(section_header->size);
    // Initialize the memory.
    memset(symtab, 0, section_header->size);
    // Read the string table.
    if (vfs_read(file, symtab, section_header->offset, section_header->size) != section_header->size) {
        pr_err("Failed to read the symbol table at %d.\n", section_header->offset);
        kfree(symtab);
        return NULL;
    }
    return symtab;
}

static inline const elf_section_header_t *elf_find_section_header(
    const elf_header_t *header,
    const elf_section_header_t *section_headers,
    const char *shstrtab,
    const char *name)
{
    for (unsigned i = 0; i < header->shnum; ++i)
        if (strcmp(shstrtab + section_headers[i].name, name) == 0)
            return &section_headers[i];
    return NULL;
}

static inline const elf_symbol_t *elf_find_symbol(
    const elf_section_header_t *symbol_table_header,
    const elf_symbol_t *symbol_table,
    const char *symbol_table_strtab,
    const char *name)
{
    // Compute the entries in the symbol table.
    uint32_t symtab_entries = symbol_table_header->size / sizeof(elf_symbol_t);
    // Search the symbol.
    for (unsigned i = 0; i < symtab_entries; ++i)
        if (strcmp(symbol_table_strtab + symbol_table[i].name, name) == 0)
            return &symbol_table[i];
    return NULL;
}

static inline int elf_load_sigreturn(
    const elf_section_header_t *symbol_table_header,
    const elf_symbol_t *symbol_table,
    const char *symbol_table_strtab,
    task_struct *task)
{
    const elf_symbol_t *sigreturn = elf_find_symbol(symbol_table_header, symbol_table, symbol_table_strtab, "sigreturn");
    if (sigreturn == NULL) {
        pr_err("Failed to find `sigreturn`!\n");
        return false;
    }
    return true;
}

/// @brief Loads an ELF executable.
/// @param task The task for which we load the ELF.
/// @param file The ELF file.
/// @param header  The header of the ELF file.
/// @return The ELF entry.
static inline int elf_load_exec(
    task_struct *task,
    vfs_file_t *file,
    const elf_header_t *header,
    const elf_program_header_t *phdrs)
{
#if 0
    pr_debug(" Type      | Mem. Size | File Size | VADDR\n");
    for (unsigned i = 0; i < header->phnum; ++i) {
        // Get the header.
        pr_debug(" %-9s | %9s | %9s | 0x%08x - 0x%08x\n",
                 elf_type_to_string(phdrs[i].type),
                 to_human_size(phdrs[i].memsz),
                 to_human_size(phdrs[i].filesz),
                 phdrs[i].vaddr, phdrs[i].vaddr + phdrs[i].memsz);
        if (phdrs[i].type == PT_LOAD) {
            uint32_t virt_addr     = create_vm_area(task->mm, phdrs[i].vaddr, phdrs[i].memsz, MM_USER | MM_RW | MM_COW, GFP_KERNEL);
            virt_map_page_t *vpage = virt_map_alloc(phdrs[i].memsz);
            uint32_t dst_addr      = virt_map_vaddress(task->mm, vpage, virt_addr, phdrs[i].memsz);

            // Load the memory area.
            vfs_read(file, (void *)dst_addr, phdrs[i].offset, phdrs[i].filesz);

            if (phdrs[i].memsz > phdrs[i].filesz) {
                uint32_t zmem_sz = phdrs[i].memsz - phdrs[i].filesz;
                memset((void *)(dst_addr + phdrs[i].filesz), 0, zmem_sz);
            }
            virt_unmap_pg(vpage);
        }
    }
#endif
    return 0;
}

static inline void dump_elf_section_headers(
    vfs_file_t *file,
    const elf_header_t *header,
    const elf_section_header_t *shdrs,
    const char *shstrtab)
{
    pr_debug("[Nr] Name                 Type            Addr     Off    Size   ES Flg Lk Inf Al\n");
    for (int i = 0; i < header->shnum; ++i) {
        pr_debug("[%2d] %-20s %-15s %08x %06x %06x %2u %3u %2u %3u %2u\n",
                 i, shstrtab + shdrs[i].name, elf_section_header_type_to_string(shdrs[i].type),
                 shdrs[i].addr, shdrs[i].offset, shdrs[i].size,
                 shdrs[i].entsize, shdrs[i].flags, shdrs[i].link, shdrs[i].info, shdrs[i].addralign);
    }
}

static inline void dump_elf_symbol_table(
    vfs_file_t *file,
    const elf_header_t *header,
    const elf_section_header_t *symtab_header,
    const char *symtab_strtab)
{
    // Count the number of entries.
    uint32_t symtab_entries = symtab_header->size / sizeof(elf_symbol_t);
    pr_debug("Symbol table '.symtab' contains %d entries (%d/%d):\n", symtab_entries, symtab_header->size, sizeof(elf_symbol_t));
    pr_debug("[ Nr ]    Value  Size Type    Bind   Vis      Ndx Name\n");
    elf_symbol_t symbol;
    for (int i = 0; i < symtab_entries; ++i) {
        if (read_elf_symbol(file, symtab_header, i, &symbol) == -1) {
            pr_err("Failed to read the elf symbol at index %d.\n", i);
            continue;
        }
        pr_debug("[%4d] %08x %5d %-7s %-6s %-8s %3d %s\n", i, symbol.value, symbol.size,
                 elf_symbol_type_to_string(ELF32_ST_TYPE(symbol.info)),
                 elf_symbol_bind_to_string(ELF32_ST_BIND(symbol.info)),
                 "-",
                 symbol.ndx,
                 symtab_strtab + symbol.name);
    }
}

int elf_load_file(task_struct *task, vfs_file_t *file, uint32_t *entry)
{
    // Open the file.
    if (file == NULL)
        return false;

    // Read the elf header.
    elf_header_t header;
    if (!read_elf_header(file, &header))
        return false;

    // Print header info.
    pr_debug("Version        : 0x%x\n", header.version);
    pr_debug("Entry          : 0x%x\n", header.entry);
    pr_debug("Headers offset : 0x%x\n", header.phoff);
    pr_debug("Headers count  : %d\n", header.phnum);

    // Check the elf header.
    if (!elf_check_file_header(&header))
        return false;

    // Check if the elf file is an executable.
    if (header.type != ET_EXEC) {
        pr_err("Elf file is not an executable.\n");
        return false;
    }

    // Load section headers.
    elf_section_header_t *section_headers = read_elf_section_headers(file, &header);
    if (section_headers == NULL) {
        pr_err("Failed to load section headers.\n");
        return false;
    }

    // Load program headers.
    elf_program_header_t *program_headers = read_elf_program_headers(file, &header);
    if (program_headers == NULL) {
        pr_err("Failed to load program headers.\n");
        goto free_section_headers;
    }

    // Get the header pointing to the section headers string table.
    elf_section_header_t *shstrtab_hdr = &section_headers[header.shstrndx];
    // Read the section headers string table.
    char *shstrtab = read_elf_strtable(file, &header, shstrtab_hdr);
    if (shstrtab == NULL) {
        pr_err("Cannot retrieve the symbol table.\n");
        goto free_program_headers;
    }

    // Find the symbol table header.
    const elf_section_header_t *symtab_header = elf_find_section_header(&header, section_headers, shstrtab, ".symtab");
    if (symtab_header == NULL) {
        pr_err("Cannot find symbol table.\n");
        goto free_string_table;
    }

    // Find the string table header associated with the symbol table header.
    const elf_section_header_t *symtab_strtab_header = &section_headers[symtab_header->link];
    if (symtab_strtab_header == NULL) {
        pr_err("Cannot find strtab of symbol table.\n");
        goto free_string_table;
    }

    // Load the symbol table.
    elf_symbol_t *symtab = read_elf_symtable(file, &header, symtab_header);
    if (symtab == NULL) {
        pr_err("Cannot load symbol table.\n");
        goto free_symbol_table_strtab;
    }

    // Read the symbol table section header.
    char *symtab_strtab = read_elf_strtable(file, &header, symtab_strtab_header);
    if (symtab_strtab == NULL) {
        pr_err("Cannot retrieve the symbol table.\n");
        goto free_symbol_table;
    }

    if (elf_load_sigreturn(symtab_header, symtab, symtab_strtab, task) == -1) {
        pr_err("Failed to load `sigreturn`.\n");
        goto free_symbol_table;
    }
    dump_elf_section_headers(file, &header, section_headers, shstrtab);
    dump_elf_symbol_table(file, &header, symtab_header, symtab_strtab);


free_symbol_table:
    kfree(symtab_strtab);
free_symbol_table_strtab:
    kfree(symtab);
free_string_table:
    kfree(shstrtab);
free_program_headers:
    kfree(program_headers);
free_section_headers:
    kfree(section_headers);

    while (true) {}
#if 0
    return false;

    while (true) {}

    //dump_elf_symbol_table(file, &header);
    if (elf_load_exec(task, file, &header, phdrs) == -1) {
        // Free up the memory.
        kfree(shdrs);
        kfree(phdrs);
        return 0;
    }
    // Set the entry.
    (*entry) = header.entry;

    // Free up the memory.
    kfree(shdrs);
    kfree(phdrs);

    while (true) {}
#endif
    return 1;
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