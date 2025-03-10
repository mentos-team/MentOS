/// @file reboot.h
/// @brief Defines the values required to issue a reboot.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

/// Magic values required to use _reboot() system call.
#define LINUX_REBOOT_MAGIC1  0xfee1dead
/// Magic values required to use _reboot() system call.
#define LINUX_REBOOT_MAGIC2  672274793
/// Magic values required to use _reboot() system call.
#define LINUX_REBOOT_MAGIC2A 85072278
/// Magic values required to use _reboot() system call.
#define LINUX_REBOOT_MAGIC2B 369367448
/// Magic values required to use _reboot() system call.
#define LINUX_REBOOT_MAGIC2C 537993216

// Commands accepted by the _reboot() system call.
/// Restart system using default command and mode.
#define LINUX_REBOOT_CMD_RESTART    0x01234567
/// Stop OS and give system control to ROM monitor, if any.
#define LINUX_REBOOT_CMD_HALT       0xCDEF0123
/// Ctrl-Alt-Del sequence causes RESTART command.
#define LINUX_REBOOT_CMD_CAD_ON     0x89ABCDEF
/// Ctrl-Alt-Del sequence sends SIGINT to init task.
#define LINUX_REBOOT_CMD_CAD_OFF    0x00000000
/// Stop OS and remove all power from system, if possible.
#define LINUX_REBOOT_CMD_POWER_OFF  0x4321FEDC
/// Restart system using given command string.
#define LINUX_REBOOT_CMD_RESTART2   0xA1B2C3D4
/// Suspend system using software suspend if compiled in.
#define LINUX_REBOOT_CMD_SW_SUSPEND 0xD000FCE2
/// Restart system using a previously loaded Linux kernel
#define LINUX_REBOOT_CMD_KEXEC      0x45584543

/// @brief Reboots the system, or enables/disables the reboot keystroke.
/// @param magic1 fails (with the error EINVAL) unless equals LINUX_REBOOT_MAGIC1.
/// @param magic2 fails (with the error EINVAL) unless equals LINUX_REBOOT_MAGIC2.
/// @param cmd The command to send to the reboot.
/// @param arg Argument passed with some specific commands.
/// @return For the values of cmd that stop or restart the system, a
///         successful call to reboot() does not return. For the other cmd
///         values, zero is returned on success. In all cases, -1 is
///         returned on failure, and errno is set appropriately.
int reboot(int magic1, int magic2, unsigned int cmd, void *arg);
