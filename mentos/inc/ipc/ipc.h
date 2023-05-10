/// @file ipc.h
/// @brief Vital IPC structures and functions kernel side.
/// @copyright (c) 2014-2023 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "sys/ipc.h"

#ifndef __KERNEL__
#error "How did you include this file... include `libc/inc/sys/ipc.h` instead!"
#endif

struct ipc_perm register_ipc(key_t key);
