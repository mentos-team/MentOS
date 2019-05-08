///                MentOS, The Mentoring Operating system project
/// @file sys.c
/// @brief
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "sys.h"
#include "stdio.h"
#include "errno.h"
#include "mutex.h"
#include "reboot.h"
#include "stdatomic.h"

static void machine_power_off()
{
    while (true)
    {
        cpu_relax();
    }
}

/// @brief Shutdown everything and perform a clean system power_off.
static void kernel_power_off()
{
//    kernel_shutdown_prepare(SYSTEM_POWER_OFF);
//    if (pm_power_off_prepare)
//        {
//            pm_power_off_prepare();
//        }
//    migrate_to_reboot_cpu();
//    syscore_shutdown();
    printf("Power down\n");
//    kmsg_dump(KMSG_DUMP_POWEROFF);
    machine_power_off();
}

int sys_reboot(int magic1, int magic2, unsigned int cmd, void *arg)
{
    static mutex_t reboot_mutex;

    // For safety, we require "magic" arguments.
    if (magic1 != LINUX_REBOOT_MAGIC1       ||
        (magic2 != LINUX_REBOOT_MAGIC2      &&
         magic2 != LINUX_REBOOT_MAGIC2A     &&
         magic2 != LINUX_REBOOT_MAGIC2B     &&
         magic2 != LINUX_REBOOT_MAGIC2C))
    {
        return -EINVAL;
    }

    mutex_lock(&reboot_mutex, 0);

    switch (cmd)
    {
        case LINUX_REBOOT_CMD_RESTART:
            break;
        case LINUX_REBOOT_CMD_CAD_ON:
            break;
        case LINUX_REBOOT_CMD_CAD_OFF:
            break;
        case LINUX_REBOOT_CMD_HALT:
            break;
        case LINUX_REBOOT_CMD_POWER_OFF:
            kernel_power_off();
            break;
        case LINUX_REBOOT_CMD_RESTART2:
            break;
        case LINUX_REBOOT_CMD_KEXEC:
            break;
        case LINUX_REBOOT_CMD_SW_SUSPEND:
            break;
        default:
            return -EINVAL;
    }
    mutex_unlock(&reboot_mutex);

    return 0;
}
