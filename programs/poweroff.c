/// @file poweroff.c
/// @brief  
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <sys/reboot.h>
#include <sys/unistd.h>
#include <stdio.h>

int main(int argc, char** argv)
{
    printf("Executing power-off...\n");
    reboot(
        LINUX_REBOOT_MAGIC1,
        LINUX_REBOOT_MAGIC2,
        LINUX_REBOOT_CMD_POWER_OFF,
        NULL);
    return 0;
}
