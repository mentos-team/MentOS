/// @file   boot_paging.h
/// @brief  Minimal i386 paging structures used before the kernel takes over.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "stdint.h"

#define PAGE_SHIFT 12U
#define PAGE_SIZE  (1U << PAGE_SHIFT)
#define BOOT_PAGE_ENTRIES 1024U

typedef struct boot_page_dir_entry {
    unsigned int present : 1;
    unsigned int rw : 1;
    unsigned int user : 1;
    unsigned int w_through : 1;
    unsigned int cache : 1;
    unsigned int accessed : 1;
    unsigned int reserved : 1;
    unsigned int page_size : 1;
    unsigned int global : 1;
    unsigned int available : 3;
    unsigned int frame : 20;
} boot_page_dir_entry_t;

typedef struct boot_page_table_entry {
    unsigned int present : 1;
    unsigned int rw : 1;
    unsigned int user : 1;
    unsigned int w_through : 1;
    unsigned int cache : 1;
    unsigned int accessed : 1;
    unsigned int dirty : 1;
    unsigned int zero : 1;
    unsigned int global : 1;
    unsigned int available : 3;
    unsigned int frame : 20;
} boot_page_table_entry_t;

typedef struct boot_page_table {
    boot_page_table_entry_t pages[BOOT_PAGE_ENTRIES];
} __attribute__((aligned(PAGE_SIZE))) boot_page_table_t;

typedef struct boot_page_directory {
    boot_page_dir_entry_t entries[BOOT_PAGE_ENTRIES];
} __attribute__((aligned(PAGE_SIZE))) boot_page_directory_t;
