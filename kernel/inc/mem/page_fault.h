/// @file page_fault.h
/// @brief Page fault handler interface.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "kernel.h"

/// @brief Initializes the page fault handler.
/// @return 0 on success, -1 on failure.
int init_page_fault(void);

/// @brief Handles a page fault.
/// @param f The interrupt stack frame.
void page_fault_handler(pt_regs_t *f);
