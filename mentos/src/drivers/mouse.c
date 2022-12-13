/// @file mouse.c
/// @brief  Driver for *PS2* Mouses.
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.is distributed under the MIT License.
/// @addtogroup mouse
/// @{

// Include the kernel log levels.
#include "sys/kernel_levels.h"
/// Change the header.
#define __DEBUG_HEADER__ "[MOUSE ]"
/// Set the log level.
#define __DEBUG_LEVEL__ LOGLEVEL_NOTICE

#include "drivers/mouse.h"
#include "descriptor_tables/isr.h"
#include "hardware/pic8259.h"
#include "io/port_io.h"

/// The mouse starts sending automatic packets.
#define MOUSE_ENABLE_PACKET 0xF4
/// The mouse stops sending automatic packets.
#define MOUSE_DISABLE_PACKET 0xF5
/// Disables streaming, sets the packet rate to 100 per second, and resolution to 4 pixels per mm.
#define MOUSE_USE_DEFAULT_SETTINGS 0xF6

/// Mouse ISR cycle.
static uint8_t mouse_cycle = 0;
/// Mouse communication data.
static int8_t mouse_bytes[3];
/// Mouse x position.
static int32_t mouse_x = (800 / 2);
/// Mouse y position.
static int32_t mouse_y = (600 / 2);

/// @brief      Mouse wait for a command.
/// @param type 1 for sending - 0 for receiving.
static void __mouse_waitcmd(unsigned char type)
{
    register unsigned int _time_out = 100000;
    if (type == 0) {
        // DATA.
        while (_time_out--) {
            if ((inportb(0x64) & 1) == 1) {
                return;
            }
        }
        return;
    } else {
        while (_time_out--) // SIGNALS
        {
            if ((inportb(0x64) & 2) == 0) {
                return;
            }
        }
        return;
    }
}

/// @brief      Send data to mouse.
/// @param data The data to send.
static void __mouse_write(unsigned char data)
{
    __mouse_waitcmd(1);
    outportb(0x64, 0xD4);
    __mouse_waitcmd(1);
    outportb(0x60, data);
}

/// @brief  Read data from mouse.
/// @return The data received from mouse.
static unsigned char __mouse_read()
{
    __mouse_waitcmd(0);
    return inportb(0x60);
}

/// @brief The interrupt service routine of the mouse.
/// @param f The interrupt stack frame.
static void __mouse_isr(pt_regs *f)
{
    (void)f;
    // Get the input bytes.
    mouse_bytes[mouse_cycle++] = (char)inportb(0x60);
    if (mouse_cycle == 3) {
        // Reset the mouse cycle.
        mouse_cycle = 0;
        // ----------------------------
        // Get the X coordinates.
        // ----------------------------
        if ((mouse_bytes[0] & 0x40) == 0) {
            // Bit number 4 of the first byte (value 0x10) indicates that
            // delta X (the 2nd byte) is a negative number, if it is set.
            if ((mouse_bytes[0] & 0x10) == 0) {
                mouse_x -= mouse_bytes[1];
            } else {
                mouse_x += mouse_bytes[1];
            }
        } else {
            // Overflow.
            mouse_x += mouse_bytes[1] / 2;
        }
        // ----------------------------
        // Get the Y coordinates.
        // ----------------------------
        if ((mouse_bytes[0] & 0x80) == 0) {
            // Bit number 5 of the first byte (value 0x20) indicates that
            // delta Y (the 3rd byte) is a negative number, if it is set.
            if ((mouse_bytes[0] & 0x20) == 0) {
                mouse_y -= mouse_bytes[2];
            } else {
                mouse_y += mouse_bytes[2];
            }
        } else {
            // Overflow.
            mouse_y -= mouse_bytes[2] / 2;
        }
        // ----------------------------
        // Apply cursor constraint (800x600).
        // ----------------------------
        if (mouse_x <= 0) {
            mouse_x = 0;
        } else if (mouse_x >= (800 - 16)) {
            mouse_x = 800 - 16;
        }
        if (mouse_y <= 0) {
            mouse_y = 0;
        } else if (mouse_y >= (600 - 24)) {
            mouse_y = 600 - 24;
        }
        // Print the position.
        // pr_default("\rX: %d | Y: %d\n", mouse_x, mouse_y);

        // Move the cursor.
        // video_set_cursor(mouse_x, mouse_y);

        // Here a problem is detected, if the mouse moves
        // Pressed keys are detected.
        // Detecting keystrokes.
        // Center pressed.
        if ((mouse_bytes[0] & 0x04) == 0) {
            // pr_default(LNG_MOUSE_MID);
        }
        // Right pressed.
        if ((mouse_bytes[0] & 0x02) == 0) {
            // pr_default(LNG_MOUSE_RIGHT);
        }
        // Left pressed.
        if ((mouse_bytes[0] & 0x01) == 0) {
            // pr_default(LNG_MOUSE_LEFT);
        }
    }
    pic8259_send_eoi(IRQ_MOUSE);
}

/// @brief Enable the mouse driver.
static void __mouse_enable()
{
    // Enable the mouse interrupts.
    pic8259_irq_enable(IRQ_MOUSE);
    // Disable the mouse.
    __mouse_write(MOUSE_ENABLE_PACKET);
    // Acknowledge.
    __mouse_read();
}

/// @brief Disable the mouse driver.
static void __mouse_disable()
{
    // Disable the mouse interrupts.
    pic8259_irq_disable(IRQ_MOUSE);
    // Disable the mouse.
    __mouse_write(MOUSE_DISABLE_PACKET);
    // Acknowledge.
    __mouse_read();
}

int mouse_initialize()
{
    // Enable the auxiliary mouse device.
    __mouse_waitcmd(1);
    outportb(0x64, 0xA8);

    // Enable the interrupts.
    __mouse_waitcmd(1);
    outportb(0x64, 0x20);
    __mouse_waitcmd(0);
    uint8_t status_byte = (inportb(0x60) | 2);
    __mouse_waitcmd(1);
    outportb(0x64, 0x60);
    __mouse_waitcmd(1);
    outportb(0x60, status_byte);

    // Tell the mouse to use default settings.
    __mouse_write(MOUSE_USE_DEFAULT_SETTINGS);
    // Acknowledge.
    __mouse_read();

    // Setup the mouse handler.
    irq_install_handler(IRQ_MOUSE, __mouse_isr, "mouse");

    // Enable the mouse.
    __mouse_enable();
    return 0;
}

int mouse_finalize()
{
    // Uninstall the IRQ.
    irq_uninstall_handler(IRQ_MOUSE, __mouse_isr);
    
    // Disable the mouse.
    __mouse_disable();
    return 0;
}

/// @}
