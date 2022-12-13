/// @file module.h
/// @brief
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "multiboot.h"
#include "stdint.h"

/// The maximum number of modules.
#define MAX_MODULES 10

extern multiboot_module_t modules[MAX_MODULES];

/// @brief Ininitialize the modules.
/// @param header Multiboot info used to initialize the modules.
/// @return 1 on success, 0 on error.
int init_modules(multiboot_info_t *header);

/// @brief Relocates modules to virtual mapped low memory, to allow physical
///        unmapping of the first part of the ram.
/// @return 1 on success, 0 on failure.
int relocate_modules();

/// @brief Returns the address where the modules end.
/// @return Address after the modules.
uintptr_t get_address_after_modules();
