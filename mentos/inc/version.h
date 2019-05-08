///                MentOS, The Mentoring Operating system project
/// @file   version.h
/// @brief  Version information.
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

/// The name of the operating system.
#define OS_NAME "MentOS"

/// The site of the operating system.
#define OS_SITEURL "..."

/// Major version of the operating system.
#define OS_MAJOR_VERSION 0

/// Minor version of the operating system.
#define OS_MINOR_VERSION 3

/// Micro version of the operating system.
#define OS_MICRO_VERSION 0

/// Helper to transform the given argument into a string.
#define OS_STR_HELPER(x) #x

/// Helper to transform the given argument into a string.
#define OS_STR(x) OS_STR_HELPER(x)

/// Complete version of the operating system.
#define OS_VERSION \
    OS_STR(OS_MAJOR_VERSION) "." \
    OS_STR(OS_MINOR_VERSION) "." \
    OS_STR(OS_MICRO_VERSION)
