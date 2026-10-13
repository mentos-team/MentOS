/// @file types.h
/// @brief Fixed-width types shared by the Multiboot boundary.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

/* The bootloader is compiled with -nostdinc and cannot include the kernel's
 * freestanding libc headers.  The kernel side already has the canonical
 * definitions, so only the bootloader gets the local ABI vocabulary. */
#ifdef BOOTLOADER_BUILD
typedef signed char int8_t;
typedef unsigned char uint8_t;
typedef short int16_t;
typedef unsigned short uint16_t;
typedef int int32_t;
typedef unsigned int uint32_t;
typedef long long int64_t;
typedef unsigned long long uint64_t;
typedef signed int intptr_t;
typedef unsigned int uintptr_t;
#else
#include "stdint.h"
#endif

#define UINT32_MAX (+4294967295U)

#ifndef NULL
#define NULL ((void *)0)
#endif
