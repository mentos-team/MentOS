/// @file ps2.c
/// @brief PS/2 drivers.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

// Setup the logging for this file (do this before any other include).
#include "sys/kernel_levels.h"           // Include kernel log levels.
#define __DEBUG_HEADER__ "[PS/2  ]"      ///< Change header.
#define __DEBUG_LEVEL__  LOGLEVEL_NOTICE ///< Set log level.
#include "io/debug.h"                    // Include debugging functions.

#include "drivers/ps2.h"
#include "io/port_io.h"
#include "proc_access.h"
#include "stdbool.h"
#include "sys/bitops.h"

/// @defgroup PS2_IO_PORTS PS/2 I/O Ports
/// @{
#define PS2_DATA   0x60 ///< Data signal line.
#define PS2_STATUS 0x64 ///< Status and command signal line.
/// @}

/// @defgroup PS2_CONTROLLER_COMMANDS PS/2 Controller Commands
/// @{
#define PS2_CTRL_TEST_CONTROLLER   0xAA ///< Command to test the PS/2 controller; returns 0x55 for pass, 0xFC for fail.
#define PS2_CTRL_P1_ENABLE         0xAE ///< Command to enable the first PS/2 port; does not return a response.
#define PS2_CTRL_P1_DISABLE        0xAD ///< Command to disable the first PS/2 port; does not return a response.
#define PS2_CTRL_P1_TEST           0xAB ///< Command to test the first PS/2 port; returns status results.
#define PS2_CTRL_P2_ENABLE         0xA8 ///< Command to enable the second PS/2 port; does not return a response.
#define PS2_CTRL_P2_DISABLE        0xA7 ///< Command to disable the second PS/2 port; does not return a response.
#define PS2_CTRL_P2_TEST           0xA9 ///< Command to test the second PS/2 port; applicable only if both ports are supported.
#define PS2_CTRL_READ_OUTPUT_PORT  0xD0 ///< Reads the current state of the output port.
#define PS2_CTRL_WRITE_OUTPUT_PORT 0xD1 ///< Writes to the output port, controls system reset line and other signals.
#define PS2_CTRL_READ_RAM_BYTE_0   0x20 ///< Reads the configuration byte from PS/2 controller RAM.
#define PS2_CTRL_WRITE_RAM_BYTE_0  0x60 ///< Writes to the configuration byte in PS/2 controller RAM.
#define PS2_CTRL_P1_RESET          0xFE ///< Resets the first PS/2 port.
/// @}

/// @defgroup PS2_DEVICE_COMMANDS PS/2 Device (Keyboard) Commands
/// @{
#define PS2_DEV_RESET         0xFF ///< Resets the device (keyboard or mouse), triggers self-test.
#define PS2_DEV_DISABLE_SCAN  0xF5 ///< Disables scanning, stops the device from sending scancodes.
#define PS2_DEV_ENABLE_SCAN   0xF4 ///< Enables scanning, allowing the device to send scancodes.
#define PS2_DEV_SET_DEFAULTS  0xF6 ///< Sets the device to its default settings.
#define PS2_DEV_SET_LED       0xED ///< Sets the keyboard LED state (Caps Lock, Num Lock, Scroll Lock).
#define PS2_DEV_SCAN_CODE_SET 0xF0 ///< Selects the scancode set (requires additional byte to specify the set).
/// @}

/// @defgroup PS2_DEVICE_RESPONSES PS/2 Device Responses
/// @{
#define PS2_DEV_SELF_TEST_PASS    0xAA ///< Self-test passed (sent after a reset or power-up).
#define PS2_DEV_SET_TYPEMATIC_ACK 0xFA ///< Acknowledges the Set Typematic Rate/Delay command.
#define PS2_DEV_OVERRUN           0xFF ///< Indicates a buffer overrun during communication.
#define PS2_ECHO_RES              0xEE ///< Response indicating the controller received an "echo" command (0xEE).
#define PS2_TEST_FAIL1            0xFC ///< Response indicating self-test failure (after 0xFF reset command or power-up).
#define PS2_TEST_FAIL2            0xFD ///< Response indicating self-test failure (after 0xFF reset command or power-up).
#define PS2_RESEND                0xFE ///< Response requesting the controller to resend the last command sent.
/// @}

/// @defgroup PS2_STATUS_REGISTER_FLAGS PS/2 Status Register Flags
/// @{
#define PS2_STATUS_OUTPUT_FULL  0x01 ///< Output buffer is full, data is available to be read.
#define PS2_STATUS_INPUT_FULL   0x02 ///< Input buffer is full, cannot send another command until it's clear.
#define PS2_STATUS_SYSTEM       0x04 ///< "System" flag, distinguishes between system and non-system events.
#define PS2_STATUS_COMMAND      0x08 ///< 1 if data in input buffer is a command, 0 if it's data.
#define PS2_STATUS_TIMEOUT      0x40 ///< Timeout error has occurred.
#define PS2_STATUS_PARITY_ERROR 0x80 ///< Parity error occurred during communication.
/// @}

// PS/2 Controller Configuration Byte
// Bit | Meaning
//  0  | First PS/2 port interrupt (1 = enabled, 0 = disabled)
//  1  | Second PS/2 port interrupt (1 = enabled, 0 = disabled, only if 2 PS/2 ports supported)
//  2  | System Flag (1 = system passed POST, 0 = your OS shouldn't be running)
//  3  | Should be zero
//  4  | First PS/2 port clock (1 = disabled, 0 = enabled)
//  5  | Second PS/2 port clock (1 = disabled, 0 = enabled, only if 2 PS/2 ports supported)
//  6  | First PS/2 port translation (1 = enabled, 0 = disabled)
//  7  | Must be zero

void ps2_write_data(unsigned char data)
{
    unsigned int timeout = 100000;

    // Wait for the input buffer to be empty before sending data (with timeout).
    while ((inportb(PS2_STATUS) & PS2_STATUS_INPUT_FULL) && --timeout) {
        pause();
    }

    if (!timeout) {
        pr_warning("ps2_write_data: timeout waiting for input buffer\n");
        return;
    }

    outportb(PS2_DATA, data);
}

void ps2_write_command(unsigned char command)
{
    unsigned int timeout = 100000;

    // Wait for the input buffer to be empty before sending data (with timeout).
    while ((inportb(PS2_STATUS) & PS2_STATUS_INPUT_FULL) && --timeout) {
        pause();
    }

    if (!timeout) {
        pr_warning("ps2_write_command: timeout waiting for input buffer\n");
        return;
    }

    // Write the command to the PS/2 data register.
    outportb(PS2_STATUS, command);
}

unsigned char ps2_read_data(void)
{
    unsigned int timeout = 1000000;

    // Wait until the output buffer is not full (data is available, with timeout).
    while (!(inportb(PS2_STATUS) & PS2_STATUS_OUTPUT_FULL) && --timeout) {
        pause();
    }

    if (!timeout) {
        pr_warning("ps2_read_data: timeout waiting for output buffer\n");
        return 0xFF;
    }

    // Read and return the data from the PS/2 data register.
    return inportb(PS2_DATA);
}

/// @brief Reads the PS2 controller status.
/// @return the PS2 controller status.
static inline unsigned char __ps2_get_controller_status(void)
{
    ps2_write_command(PS2_CTRL_READ_RAM_BYTE_0);
    return ps2_read_data();
}

/// @brief Sets the PS2 controller status.
/// @param status the PS2 controller status.
static inline void __ps2_set_controller_status(unsigned char status)
{
    ps2_write_command(PS2_CTRL_WRITE_RAM_BYTE_0);
    ps2_write_data(status);
}

/// @brief Checks if the PS2 controller is dual channel.
/// @return 1 if dual channel, 0 otherwise.
static inline int __ps2_is_dual_channel(void)
{
    // Bit 5 is the clock-disable for the second port when a second port exists.
    // If that bit stays set even after enabling the second port, we assume single channel.
    const unsigned char status = __ps2_get_controller_status();
    return bit_check(status, 5) == 0;
}

/// @brief Enables the first PS2 port.
static inline void __ps2_enable_first_port(void) { ps2_write_command(PS2_CTRL_P1_ENABLE); }

/// @brief Enables the second PS2 port.
static inline void __ps2_enable_second_port(void) { ps2_write_command(PS2_CTRL_P2_ENABLE); }

/// @brief Disables the first PS2 port.
static inline void __ps2_disable_first_port(void) { ps2_write_command(PS2_CTRL_P1_DISABLE); }

/// @brief Disables the second PS2 port.
static inline void __ps2_disable_second_port(void) { ps2_write_command(PS2_CTRL_P2_DISABLE); }

/// @brief Writes a byte of data to the first PS/2 port (typically for a keyboard).
/// @param byte The value to write to the first PS/2 port.
static inline void __ps2_write_first_port(unsigned char byte)
{
    // Directly write the specified byte to the first PS/2 port.
    ps2_write_data(byte);
}

/// @brief Writes a byte of data to the second PS/2 port (typically for a mouse).
/// @param byte The value to write to the second PS/2 port.
static inline void __ps2_write_second_port(unsigned char byte)
{
    // Send the command to direct the next byte to the second PS/2 port.
    ps2_write_command(0xD4);

    // Write the specified byte to the second PS/2 port.
    ps2_write_data(byte);
}

/// @brief Returns the string describing the received response.
/// @param response the response received from the PS2 device.
/// @return the string describing the received response.
static const char *__ps2_get_response_error_message(unsigned response)
{
    if (response == 0x01) {
        return "clock line stuck low";
    }
    if (response == 0x02) {
        return "clock line stuck high";
    }
    if (response == 0x03) {
        return "data line stuck low";
    }
    if (response == 0x04) {
        return "data line stuck high";
    }
    return "unknown error";
}

int ps2_initialize(void)
{
    unsigned char status;
    unsigned char response;
    bool_t dual;
    unsigned int flush_timeout;

    // Pre-init: aggressively flush any stale data from BIOS/bootloader
    pr_debug("Initial aggressive buffer flush...\n");
    for (int flush_retry = 0; flush_retry < 10; flush_retry++) {
        unsigned int retry = 100;
        while (retry-- > 0) {
            if (inportb(PS2_STATUS) & PS2_STATUS_OUTPUT_FULL) {
                inportb(PS2_DATA); // Read and discard
            } else {
                break;
            }
        }
    }

    status = __ps2_get_controller_status();
    pr_debug("Initial Status   : %s (%3d | %02x)\n", dec_to_binary(status, 8), status, status);

    // ========================================================================
    // Step 1: Disable Devices
    // So that any PS/2 devices can't send data at the wrong time and mess up
    // your initialisation; start by sending a command 0xAD and command 0xA7 to
    // the PS/2 controller. If the controller is a "single channel" device, it
    // will ignore the "command 0xA7".

    pr_debug("Disabling first port...\n");
    __ps2_disable_first_port();
    // Small delay to allow command to take effect
    for (volatile int i = 0; i < 10000; i++) {
        pause();
    }

    pr_debug("Disabling second port...\n");
    __ps2_disable_second_port();
    // Small delay to allow command to take effect
    for (volatile int i = 0; i < 10000; i++) {
        pause();
    }

    // ========================================================================
    // Step 2: Flush The Output Buffer
    // Sometimes (e.g. due to interrupt controlled initialisation causing a lost
    // IRQ) data can get stuck in the PS/2 controller's output buffer. To guard
    // against this, now that the devices are disabled (and can't send more data
    // to the output buffer) it can be a good idea to flush the controller's
    // output buffer. There's 2 ways to do this - poll bit 0 of the Status
    // Register (while reading from IO Port 0x60 if/when bit 0 becomes set), or
    // read from IO Port 0x60 without testing bit 0. Either way should work (as
    // you're discarding the data and don't care what it was).

    pr_debug("Flushing the output buffer...\n");
    // Flush the output buffer with timeout to prevent infinite loops
    // Only read if output buffer is marked as full
    flush_timeout = 100;
    while (flush_timeout-- > 0) {
        if (inportb(PS2_STATUS) & PS2_STATUS_OUTPUT_FULL) {
            inportb(PS2_DATA); // Read and discard
        } else {
            break; // Buffer is empty, we're done
        }
    }

    // ========================================================================
    // Step 3: Set the Controller Configuration Byte
    // Because some bits of the Controller Configuration Byte are "unknown",
    // this means reading the old value (command 0x20), changing some bits, then
    // writing the changed value back (command 0x60). You want to disable all
    // IRQs and disable translation (clear bits 0, 1 and 6).
    // While you've got the Configuration Byte, test if bit 5 was set. If it was
    // clear, then you know it can't be a "dual channel" PS/2 controller
    // (because the second PS/2 port should be disabled).

    // Get the status.
    status = __ps2_get_controller_status();
    pr_debug("Disable IRQs, enable clocks, and enable translation...\n");
    // Clear bits 0 and 1 (IRQs off) and bit 4 (P1 clock on).
    // Keep bit 6 SET to enable translation (convert set 2 to set 1).
    bit_clear_assign(status, 0);
    bit_clear_assign(status, 1);
    bit_clear_assign(status, 4);
    bit_set_assign(status, 6); // Enable translation
    __ps2_set_controller_status(status);
    pr_debug("Status   : %s (%3d | %02x)\n", dec_to_binary(status, 8), status, status);

    // ========================================================================
    // Step 4: Perform Controller Self Test
    // To test the PS/2 controller, send command 0xAA to it. Then wait for its
    // response and check it replied with 0x55. Note: this can reset the PS/2
    // controller on some hardware (tested on a 2016 laptop). At the very least,
    // the Controller Configuration Byte should be restored for compatibility
    // with such hardware. You can either determine the correct value yourself
    // based on the above table or restore the value read before issuing 0xAA.

    // Send 0xAA to the controller.
    ps2_write_command(PS2_CTRL_TEST_CONTROLLER);
    // Read the response.
    response = ps2_read_data();
    if (response == PS2_TEST_FAIL1) {
        pr_err("Self-test failed : 0x%02x\n", response);
        return 1;
    }
    // The self-test can reset the controller, so always restore the configuration.
    __ps2_set_controller_status(status);
    // Flush the output buffer after self-test as it can generate spurious data (with timeout).
    flush_timeout = 100;
    while (flush_timeout-- > 0) {
        if (inportb(PS2_STATUS) & PS2_STATUS_OUTPUT_FULL) {
            inportb(PS2_DATA); // Read and discard
        } else {
            break; // Buffer is empty
        }
    }

    // ========================================================================
    // Step 5: Determine If There Are 2 Channels
    // Enable the second PS/2 port and read the Controller Configuration Byte
    // again. Now, bit 5 of the Controller Configuration Byte should be clear -
    // if it's set then you know it can't be a "dual channel" PS/2 controller
    // (because the second PS/2 port should be enabled). If it is a dual channel
    // device, disable the second PS/2 port again.

    // Enable the second port.
    __ps2_enable_second_port();
    // Read the status.
    status = __ps2_get_controller_status();
    // Check if it is a dual channel PS/2 device (bit 5 clear after enabling second clock).
    dual   = !bit_check(status, 5);
    if (dual) {
        pr_debug("Recognized a `dual channel` PS/2 controller...\n");
        __ps2_disable_second_port();
        // Ensure second clock is enabled in the config byte for later use.
        bit_clear_assign(status, 5);
        __ps2_set_controller_status(status);
    } else {
        pr_debug("Recognized a `single channel` PS/2 controller...\n");
    }

    // ========================================================================
    // Step 6: Perform Interface Tests
    // This step tests the PS/2 ports. Use command 0xAB to test the first PS/2
    // port, then check the result. Then (if it's a "dual channel" controller)
    // use command 0xA9 to test the second PS/2 port, then check the result.
    // At this stage, check to see how many PS/2 ports are left. If there aren't
    // any that work you can just give up (display errors and terminate the PS/2
    // Controller driver).
    // Note: If one of the PS/2 ports on a dual PS/2 controller fails, then you
    // can still keep using/supporting the other PS/2 port.

    ps2_write_command(PS2_CTRL_P1_TEST);
    response = ps2_read_data();
    // Response 0x00 = success, 0x01-0x04 = specific line failure
    if ((response >= 0x01) && (response <= 0x04)) {
        pr_err("Interface test failed on first port : %02x\n", response);
        pr_err("Reason: %s.\n", __ps2_get_response_error_message(response));
        return 1;
    }
    // If it is a dual channel, check the second port.
    if (dual) {
        ps2_write_command(PS2_CTRL_P2_TEST);
        response = ps2_read_data();
        if ((response >= 0x01) && (response <= 0x04)) {
            pr_err("Interface test failed on second port : %02x\n", response);
            pr_err("Reason: %s.\n", __ps2_get_response_error_message(response));
            return 1;
        }
    }

    // ========================================================================
    // Step 7: Enable Devices
    // Enable any PS/2 port that exists and works.
    // If you're using IRQs (recommended), also enable interrupts for any
    // (usable) PS/2 ports in the Controller Configuration Byte (set bit 0 for
    // the first PS/2 port, and/or bit 1 for the second PS/2 port, then set it
    // with command 0x60).

    // Enable the first port.
    __ps2_enable_first_port();
    // Enable the second port.
    if (dual) {
        __ps2_enable_second_port();
    }
    // Get the status.
    status = __ps2_get_controller_status();
    pr_debug("Status   : %s (%3d | %02x)\n", dec_to_binary(status, 8), status, status);
    // Enable IRQs and clocks, keep translation enabled.
    bit_set_assign(status, 0);   // IRQ for first port
    bit_clear_assign(status, 4); // Ensure first clock enabled
    if (dual) {
        bit_set_assign(status, 1);   // IRQ for second port
        bit_clear_assign(status, 5); // Ensure second clock enabled
    }
    bit_set_assign(status, 6); // Keep translation ON (set 2 -> set 1)
    __ps2_set_controller_status(status);

    // ========================================================================
    // Step 8: Reset Devices
    // All PS/2 devices should support the "reset" command (which is a command
    // for the device, and not a command for the PS/2 Controller).
    // To send the reset, just send the byte 0xFF to each (usable) device. The
    // device/s will respond with 0xFA (success) or 0xFC (failure), or won't
    // respond at all (no device present). If your code supports "hot-plug PS/2
    // devices" (see later), then you can assume each device is "not present"
    // and let the hot-plug code figure out that the device is present if/when
    // 0xFA or 0xFC is received on a PS/2 port.

    // Before resetting devices, flush any stale data in the buffer.
    pr_debug("Flushing buffer before device reset...\n");
    flush_timeout = 100;
    while (flush_timeout-- > 0) {
        if (inportb(PS2_STATUS) & PS2_STATUS_OUTPUT_FULL) {
            inportb(PS2_DATA); // Read and discard
        } else {
            break;
        }
    }

    // Reset first port.
    pr_debug("Resetting first PS/2 port...\n");
    __ps2_write_first_port(0xFF);
    // Give device time to respond
    for (volatile int i = 0; i < 50000; i++) {
        pause();
    }
    // Wait for `command acknowledged`.
    response = ps2_read_data();
    pr_debug("First port reset response: 0x%02x\n", response);
    if (response == 0xFF) {
        // Timeout - device not present, this is acceptable.
        pr_debug("First PS/2 port: no device present (timeout).\n");
    } else if (response == PS2_DEV_SET_TYPEMATIC_ACK) {
        // Device acknowledged reset (or resend), wait for self-test response.
        pr_debug("First port reset acknowledged, waiting for self-test...\n");
        // Give device time to complete self-test
        for (volatile int i = 0; i < 100000; i++) {
            pause();
        }
        response = ps2_read_data();
        pr_debug("First port self-test response: 0x%02x\n", response);
        if (response == PS2_DEV_SELF_TEST_PASS) {
            pr_debug("First PS/2 port: device reset successful.\n");
        } else if (response == PS2_TEST_FAIL1 || response == PS2_TEST_FAIL2) {
            pr_debug("First PS/2 port: device self-test failed (0x%02x).\n", response);
        } else if (response == 0xFF) {
            pr_debug("First PS/2 port: timeout waiting for self-test response.\n");
        } else {
            pr_debug("First PS/2 port: unexpected response to reset (0x%02x).\n", response);
        }
    } else {
        pr_debug("First PS/2 port: unexpected response to reset (0x%02x).\n", response);
    }

    // Reset second port (only if dual channel).
    if (dual) {
        pr_debug("Resetting second PS/2 port...\n");
        __ps2_write_second_port(0xFF);
        // Give device time to respond
        for (volatile int i = 0; i < 50000; i++) {
            pause();
        }
        // Wait for `command acknowledged`.
        response = ps2_read_data();
        pr_debug("Second port reset response: 0x%02x\n", response);
        if (response == 0xFF) {
            // Timeout - device not present, this is acceptable.
            pr_debug("Second PS/2 port: no device present (timeout).\n");
        } else if (response == PS2_DEV_SET_TYPEMATIC_ACK) {
            // Device acknowledged reset, wait for self-test response.
            pr_debug("Second port reset acknowledged, waiting for self-test...\n");
            // Give device time to complete self-test
            for (volatile int i = 0; i < 100000; i++) {
                pause();
            }
            response = ps2_read_data();
            pr_debug("Second port self-test response: 0x%02x\n", response);
            if (response == PS2_DEV_SELF_TEST_PASS) {
                pr_debug("Second PS/2 port: device reset successful.\n");
            } else if (response == PS2_TEST_FAIL1 || response == PS2_TEST_FAIL2) {
                pr_debug("Second PS/2 port: device self-test failed (0x%02x).\n", response);
            } else if (response == 0xFF) {
                pr_debug("Second PS/2 port: timeout waiting for self-test response.\n");
            } else {
                pr_debug("Second PS/2 port: unexpected response to reset (0x%02x).\n", response);
            }
        } else {
            pr_debug("Second PS/2 port: unexpected response to reset (0x%02x).\n", response);
        }
    }

    // Get the final status.
    status = __ps2_get_controller_status();
    pr_debug("Status   : %s (%3d | %02x)\n", dec_to_binary(status, 8), status, status);

    pr_debug("Flushing the output buffer...\n");
    // Final flush with timeout
    flush_timeout = 100;
    while (flush_timeout-- > 0) {
        if (inportb(PS2_STATUS) & PS2_STATUS_OUTPUT_FULL) {
            inportb(PS2_DATA); // Read and discard
        } else {
            break; // Buffer is empty
        }
    }

    // ========================================================================
    // Step 9: PS/2 initialization complete
    // The PS/2 controller is now configured with interrupts enabled in its
    // config byte. IRQ handlers will enable the corresponding PIC IRQs when
    // they are installed (keyboard_initialize, mouse_install, etc).

    pr_notice("PS/2 controller initialized successfully.\n");

    return 0;
}
