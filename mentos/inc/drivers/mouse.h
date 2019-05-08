///                MentOS, The Mentoring Operating system project
/// @file   mouse.h
/// @brief  Driver for *PS2* Mouses.
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

/* The mouse starts sending automatic packets when the mouse moves or is
 * clicked.
 */
#include <kernel.h>

#define MOUSE_ENABLE_PACKET         0xF4

/// The mouse stops sending automatic packets.
#define MOUSE_DISABLE_PACKET        0xF5

/// Disables streaming, sets the packet rate to 100 per second, and
/// resolution to 4 pixels per mm.
#define MOUSE_USE_DEFAULT_SETTINGS  0xF6

/// @brief Sets up the mouse by installing the mouse handler into IRQ12.
void mouse_install();

/// @brief Enable the mouse driver.
void mouse_enable();

/// @brief Disable the mouse driver.
void mouse_disable();

/// @brief      Mouse wait for a command.
/// @param type 1 for sending - 0 for receiving.
void mouse_waitcmd(unsigned char type);

/// @brief      Send data to mouse.
/// @param data The data to send.
void mouse_write(unsigned char data);

/// @brief  Read data from mouse.
/// @return The data received from mouse.
unsigned char mouse_read();

/// @brief The mouse handler.
void mouse_isr(register_t *r);
