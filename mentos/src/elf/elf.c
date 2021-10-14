///                MentOS, The Mentoring Operating system project
/// @file elf.c
/// @brief Function for multiboot support.
/// @copyright (c) 2014-2021 This file is distributed under the MIT License.
/// See LICENSE.md for details.

/// Change the header.
#define __DEBUG_HEADER__ "[ELF   ]"

#include "elf/elf.h"

#include "process/scheduler.h"
#include "mem/vmem_map.h"
#include "process/process.h"
#include "string.h"
#include "stddef.h"
#include "misc/debug.h"
#include "stdio.h"
#include "mem/slab.h"
#include "fs/vfs.h"

/// @brief Reads the program header from file.
/// @param file The file from which we extract the program header.
/// @param hdr  A pointer to the ELF header.
/// @param idx  The index of the program header.
/// @param phdr Where we store the content we read.
/// @return The amount of bytes we read.
static inline ssize_t read_elf_program_header(vfs_file_t *file, elf_header_t *hdr, unsigned idx, elf_program_header_t *phdr)
{
    return vfs_read(file, phdr, hdr->phoff + hdr->phentsize * idx, sizeof(elf_program_header_t));
}

/// @brief Reads the section header from file.
/// @param file The file from which we extract the section header.
/// @param hdr  A pointer to the ELF header.
/// @param idx  The index of the section header.
/// @param shdr Where we store the content we read.
/// @return The amount of bytes we read.
static inline ssize_t read_elf_section_header(vfs_file_t *file, elf_header_t *hdr, unsigned idx, elf_section_header_t *shdr)
{
    return vfs_read(file, shdr, hdr->shoff + hdr->shentsize * idx, sizeof(elf_program_header_t));
}

/// @brief Reads the symbol from file.
/// @param file   The file from which we extract the symbol.
/// @param shdr   A pointer to the ELF symbol table header.
/// @param idx    The index of the symbol.
/// @param symbol Where we store the content we read.
/// @return The amount of bytes we read.
static inline ssize_t read_elf_symbol(vfs_file_t *file, elf_section_header_t *shdr, unsigned idx, elf_symbol_t *symbol)
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
static inline ssize_t read_elf_symbol_name(vfs_file_t *file, elf_section_header_t *shdr, unsigned offset, char *name, size_t name_len)
{
    return vfs_read(file, name, shdr->offset + offset, name_len);
}

static inline int elf_find_section_header(vfs_file_t *file, elf_header_t *hdr, int type, elf_section_header_t *shdr)
{
    for (int i = 0; i < hdr->shnum; ++i) {
        if (read_elf_section_header(file, hdr, i, shdr) == -1) {
            pr_err("Failed to read section header at index %d.\n", i);
            return -1;
        }
        if (shdr->type == type)
            return 0;
        memset(shdr, 0, sizeof(elf_section_header_t));
    }
    return -1;
}

static inline char *elf_get_strtable(vfs_file_t *file, elf_header_t *hdr, int ndx)
{
    if (ndx == SHT_NULL)
        return NULL;
    elf_section_header_t shdr;
    if (read_elf_section_header(file, hdr, ndx, &shdr) == -1) {
        pr_err("Failed to read section header at index %d.\n", ndx);
        return NULL;
    }
    char *strtable = kmalloc(shdr.size);
    memset(strtable, 0, shdr.size);
    if (vfs_read(file, strtable, shdr.offset, shdr.size) == -1) {
        pr_err("Failed to read the string table at %d.\n", shdr.offset);
        return NULL;
    }
#if 0
    dbg_putchar('{');
    for (int i = 0; i < shdr.size; ++i) {
        pr_debug("[%4d] `%c`", i, strtable[i]);
        if (strtable[i] == 0)
            pr_debug("\n");
    }
    dbg_putchar('}');
    dbg_putchar('\n');
#endif
    return strtable;
}

static inline int elf_load_sigreturn(task_struct *task, vfs_file_t *file, elf_header_t *hdr)
{
    elf_section_header_t shdr;
    if (elf_find_section_header(file, hdr, SHT_SYMTAB, &shdr) == -1)
        return -1;
    char *strtable = elf_get_strtable(file, hdr, shdr.link);
    if (strtable == NULL)
        return -1;
    uint32_t symtab_entries = shdr.size / sizeof(elf_symbol_t);
    elf_symbol_t symbol;
    for (int i = 0; i < symtab_entries; ++i) {
        if (read_elf_symbol(file, &shdr, i, &symbol) == -1) {
            pr_err("Failed to read the elf symbol at index %d.\n", i);
            break;
        }
        if (strcmp(strtable + symbol.name, "sigreturn") == 0) {
            task->sigreturn_eip = symbol.value;
            pr_debug("Found `sigreturn` at index %d with EIP = %p.\n", i, symbol.value);
            kfree(strtable);
            return 0;
        }
    }
    pr_emerg("Failed to find `sigreturn`!\n");
    kfree(strtable);
    return -1;
}

/// @brief Loads an ELF executable.
/// @param task The task for which we load the ELF.
/// @param file The ELF file.
/// @param hdr  The header of the ELF file.
/// @return The ELF entry.
static inline int elf_load_exec(task_struct *task, vfs_file_t *file, elf_header_t *hdr)
{
    elf_program_header_t phdr;
    pr_debug(" Type      | Mem. Size | File Size | VADDR\n");
    for (int idx = 0; idx < hdr->phnum; ++idx) {
        // Get the header.
        if (read_elf_program_header(file, hdr, idx, &phdr) == -1) {
            pr_err("Failed to read program header at index %d.\n", idx);
            return -1;
        }
        pr_debug(" %-9s | %9s | %9s | 0x%08x - 0x%08x\n",
                 elf_type_to_string(phdr.type),
                 to_human_size(phdr.memsz),
                 to_human_size(phdr.filesz),
                 phdr.vaddr, phdr.vaddr + phdr.memsz);
        if (phdr.type == PT_LOAD) {
            uint32_t virt_addr     = create_vm_area(task->mm, phdr.vaddr, phdr.memsz, MM_USER | MM_RW | MM_COW, GFP_KERNEL);
            virt_map_page_t *vpage = virt_map_alloc(phdr.memsz);
            uint32_t dst_addr      = virt_map_vaddress(task->mm, vpage, virt_addr, phdr.memsz);

            // Load the memory area.
            vfs_read(file, (void *)dst_addr, phdr.offset, phdr.filesz);

            if (phdr.memsz > phdr.filesz) {
                uint32_t zmem_sz = phdr.memsz - phdr.filesz;
                memset((void *)(dst_addr + phdr.filesz), 0, zmem_sz);
            }
            virt_unmap_pg(vpage);
        }
    }
    return 0;
}

static inline void dump_elf_section_headers(vfs_file_t *file, elf_header_t *hdr)
{
    char *strtable = elf_get_strtable(file, hdr, hdr->shstrndx);
    if (strtable == NULL)
        return;
    pr_debug("[Nr] Name                 Type            Addr     Off    Size   ES Flg Lk Inf Al\n");
    elf_section_header_t shdr;
    for (int i = 0; i < hdr->shnum; ++i) {
        if (read_elf_section_header(file, hdr, i, &shdr) == -1) {
            pr_err("Failed to read section header at index %d.\n", i);
        }
        pr_debug("[%2d] %-20s %-15s %08x %06x %06x %2u %3u %2u %3u %2u\n",
                 i, strtable + shdr.name, elf_section_header_type_to_string(shdr.type),
                 shdr.addr, shdr.offset, shdr.size,
                 shdr.entsize, shdr.flags, shdr.link, shdr.info, shdr.addralign);
    }
    kfree(strtable);
}

static inline void dump_elf_symbol_table(vfs_file_t *file, elf_header_t *hdr)
{
    elf_section_header_t shdr;
    if (elf_find_section_header(file, hdr, SHT_SYMTAB, &shdr) == -1)
        return;

    char *strtable = elf_get_strtable(file, hdr, shdr.link);
    if (strtable == NULL)
        return;

    //     Count the number of entries.
    uint32_t symtab_entries = shdr.size / sizeof(elf_symbol_t);
    pr_debug("Symbol table '.symtab' contains %d entries (%d/%d):\n", symtab_entries, shdr.size, sizeof(elf_symbol_t));
    pr_debug("[ Nr ]    Value  Size Type    Bind   Vis      Ndx Name\n");
    elf_symbol_t symbol;
    for (int i = 0; i < symtab_entries; ++i) {
        if (read_elf_symbol(file, &shdr, i, &symbol) == -1) {
            pr_err("Failed to read the elf symbol at index %d.\n", i);
        }
        pr_debug("[%4d] %08x %5d %-7s %-6s %-8s %3d %s\n", i, symbol.value, symbol.size,
                 elf_symbol_type_to_string(ELF32_ST_TYPE(symbol.info)),
                 elf_symbol_bind_to_string(ELF32_ST_BIND(symbol.info)),
                 "-",
                 symbol.ndx,
                 strtable + symbol.name);
    }
    kfree(strtable);
}

int elf_load_file(task_struct *task, vfs_file_t *file, uint32_t *entry)
{
    // Open the file.
    if (file == NULL) {
        pr_err("Cannot find executable!");
        return 0;
    }
    elf_header_t hdr;
    // Set the reading position at the beginning of the file.
    vfs_lseek(file, 0, SEEK_SET);
    // Read the header.
    if (vfs_read(file, &hdr, 0, sizeof(elf_header_t)) != -1) {
        if (elf_check_file_header(&hdr)) {
            pr_debug("Version        : 0x%x\n", hdr.version);
            pr_debug("Entry          : 0x%x\n", hdr.entry);
            pr_debug("Headers offset : 0x%x\n", hdr.phoff);
            pr_debug("Headers count  : %d\n", hdr.phnum);
            //dump_elf_section_headers(file, &hdr);
            //dump_elf_symbol_table(file, &hdr);
            if (hdr.type == ET_EXEC) {
                if (elf_load_sigreturn(task, file, &hdr) == -1) {
                    return 0;
                }
                if (elf_load_exec(task, file, &hdr) == -1) {
                    return 0;
                }
                // Set the entry.
                (*entry) = hdr.entry;
                return 1;
            } else {
                pr_err("ELF type not supported.\n");
            }
        } else {
            pr_err("ELF file cannot be loaded.\n");
        }
    } else {
        pr_err("Filed to read ELF header.\n");
    }
    return 0;
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
    elf_header_t hdr;
    // By default we return failure.
    int ret = 0;
    // Read the header and check the file type.
    if (vfs_read(file, &hdr, 0, sizeof(elf_header_t)) != -1)
        if (elf_check_file_header(&hdr))
            ret = hdr.type == type;
    // Set the reading position at the beginning of the file.
    vfs_lseek(file, 0, SEEK_SET);
    return ret;
}

int elf_check_file_header(elf_header_t *hdr)
{
    if (!elf_check_magic_number(hdr)) {
        pr_err("Invalid ELF File.\n");
        return 0;
    }
    if (hdr->ident[EI_CLASS] != ELFCLASS32) {
        pr_err("Unsupported ELF File Class.\n");
        return 0;
    }
    if (hdr->ident[EI_DATA] != ELFDATA2LSB) {
        pr_err("Unsupported ELF File byte order.\n");
        return 0;
    }
    if (hdr->machine != EM_386) {
        pr_err("Unsupported ELF File target.\n");
        return 0;
    }
    if (hdr->ident[EI_VERSION] != EV_CURRENT) {
        pr_err("Unsupported ELF File version.\n");
        return 0;
    }
    if (hdr->type != ET_EXEC) {
        pr_err("Unsupported ELF File type.\n");
        return 0;
    }
    return 1;
}

int elf_check_magic_number(elf_header_t *hdr)
{
    if (!hdr)
        return 0;
    if (hdr->ident[EI_MAG0] != ELFMAG0) {
        pr_err("ELF Header EI_MAG0 incorrect.\n");
        return 0;
    }
    if (hdr->ident[EI_MAG1] != ELFMAG1) {
        pr_err("ELF Header EI_MAG1 incorrect.\n");
        return 0;
    }
    if (hdr->ident[EI_MAG2] != ELFMAG2) {
        pr_err("ELF Header EI_MAG2 incorrect.\n");
        return 0;
    }
    if (hdr->ident[EI_MAG3] != ELFMAG3) {
        pr_err("ELF Header EI_MAG3 incorrect.\n");
        return 0;
    }
    return 1;
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