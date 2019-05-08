///                MentOS, The Mentoring Operating system project
/// @file   fpu.h
/// @brief  Floating Point Unit (FPU).
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "stdint.h"
#include "stdbool.h"

// TODO: doxygen comment.
/// @brief
void switch_fpu();

// TODO: doxygen comment.
/// @brief
void unswitch_fpu();

// TODO: doxygen comment.
/// @brief
/// @return
bool_t fpu_install();
