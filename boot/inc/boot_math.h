/// @file   boot_math.h
/// @brief  Small integer helpers for the bootloader.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#define boot_min(left, right) (((left) < (right)) ? (left) : (right))
#define boot_max(left, right) (((left) > (right)) ? (left) : (right))
