///                MentOS, The Mentoring Operating system project
/// @file utsname.c
/// @brief Functions used to provide information about the machine & OS.
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "string.h"
#include "utsname.h"
#include "version.h"

int uname(utsname_t *os_infos)
{
    // Uname code goes here.
    strcpy(os_infos->sysname, OS_NAME);
    strcpy(os_infos->version, OS_VERSION);
    strcpy(os_infos->nodename, "testbed");
    strcpy(os_infos->machine, "i686");

    return 0;
}
