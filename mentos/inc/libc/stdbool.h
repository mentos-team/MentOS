///                MentOS, The Mentoring Operating system project
/// @file stdbool.h
/// @brief
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

/// @brief Define boolean value.
typedef enum bool_t
{
    /// [0] False.
    false,
    /// [1] True.
    true
} __attribute__ ((__packed__)) bool_t;
