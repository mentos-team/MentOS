///                MentOS, The Mentoring Operating system project
/// @file poweroff.c
/// @brief  
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "commands.h"
#include "stdio.h"
#include "reboot.h"
#include "unistd.h"

void cmd_poweroff(int argc, char **argv)
{
    (void) argc;
    (void) argv;
    printf("Executing power-off...\n");
    reboot(LINUX_REBOOT_MAGIC1,
           LINUX_REBOOT_MAGIC2,
           LINUX_REBOOT_CMD_POWER_OFF,
           NULL);
}
