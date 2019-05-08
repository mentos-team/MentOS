///                MentOS, The Mentoring Operating system project
/// @file   fdc.h
/// @brief  Definitions about the floppy.
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

/// @brief Floppy Disk Controller (FDC) registers.
typedef enum fdc_registers_t
{
    STATUS_REGISTER_A = 0x3F0,
    ///< This register is read-only and monitors the state of the
    ///< interrupt pin and several disk interface pins.
        STATUS_REGISTER_B = 0x3F1,
    ///< This register is read-only and monitors the state of several
    ///< isk interface pins.
        DOR = 0x3F2,
    ///< The Digital Output Register contains the drive select and
    ///< motor enable bits, a reset bit and a DMA GATE bit.
        TAPE_DRIVE_REGISTER = 0x3F3,
    ///< This register allows the user to assign tape support to a
    ///< particular drive during initialization.
        MAIN_STATUS_REGISTER = 0x3F4,
    ///< The Main Status Register is a read-only register and is used
    ///< for controlling command input and result output for all commands.
        DATARATE_SELECT_REGISTER = 0x3F4,
    ///< This register is included for compatibility with the 82072
    ///< floppy controller and is write-only.
        DATA_FIFO = 0x3F5,
    ///< All command parameter information and disk data transfers go
    ///< through the FIFO.
        DIGITAL_INPUT_REGISTER = 0x3F7,
    ///< This register is read only in all modes.
        CONFIGURATION_CONTROL_REGISTER = 0x3F7
    ///< This register sets the datarate and is write only.
} fdc_registers_t;

/// @brief Allows to enable the motor.
/// @details The motor enable pins are directly controlled via the DOR and
///          are a function of the mapping based on BOOTSEL bits in the TDR.
void fdc_enable_motor(void);

/// @brief Allows to disable the motor.
void fdc_disable_motor(void);
