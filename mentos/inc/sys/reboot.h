///                MentOS, The Mentoring Operating system project
/// @file reboot.h
/// @brief
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

// Magic values required to use _reboot() system call.
#define LINUX_REBOOT_MAGIC1 0xfee1dead

#define LINUX_REBOOT_MAGIC2 672274793

#define LINUX_REBOOT_MAGIC2A 85072278

#define LINUX_REBOOT_MAGIC2B 369367448

#define LINUX_REBOOT_MAGIC2C 537993216

// Commands accepted by the _reboot() system call.
/// Restart system using default command and mode.
#define LINUX_REBOOT_CMD_RESTART 0x01234567

/// Stop OS and give system control to ROM monitor, if any.
#define LINUX_REBOOT_CMD_HALT 0xCDEF0123

/// Ctrl-Alt-Del sequence causes RESTART command.
#define LINUX_REBOOT_CMD_CAD_ON 0x89ABCDEF

/// Ctrl-Alt-Del sequence sends SIGINT to init task.
#define LINUX_REBOOT_CMD_CAD_OFF 0x00000000

/// Stop OS and remove all power from system, if possible.
#define LINUX_REBOOT_CMD_POWER_OFF 0x4321FEDC

/// Restart system using given command string.
#define LINUX_REBOOT_CMD_RESTART2 0xA1B2C3D4

/// Suspend system using software suspend if compiled in.
#define LINUX_REBOOT_CMD_SW_SUSPEND 0xD000FCE2

/// Restart system using a previously loaded Linux kernel
#define LINUX_REBOOT_CMD_KEXEC 0x45584543
