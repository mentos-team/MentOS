///                MentOS, The Mentoring Operating system project
/// @file utsname.h
/// @brief Functions used to provide information about the machine & OS.
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

/// Maximum length of the string used by utsname.
#define SYS_LEN 257

/// @brief Holds information concerning the machine and the os.
typedef struct utsname_t
{
    /// The name of the system.
    char sysname[SYS_LEN];

    /// The name of the node.
    char nodename[SYS_LEN];

    /// The version of the OS.
    char version[SYS_LEN];

    /// The name of the machine.
    char machine[SYS_LEN];
} utsname_t;

/// @brief Sets the values of os_infos.
int uname(utsname_t *os_infos);
