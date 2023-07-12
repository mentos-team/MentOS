/// @file ata_types.h
/// @brief Data types for managing Advanced Technology Attachment (ATA) devices.
/// @copyright (c) 2014-2023 This file is distributed under the MIT License.
/// See LICENSE.md for details.
/// @addtogroup ata
/// @{

#pragma once

/// @brief ATA Error Bits
typedef enum {
    ata_err_amnf  = (1 << 0), ///< Address mark not found.
    ata_err_tkznf = (1 << 1), ///< Track zero not found.
    ata_err_abrt  = (1 << 2), ///< Aborted command.
    ata_err_mcr   = (1 << 3), ///< Media change request.
    ata_err_idnf  = (1 << 4), ///< ID not found.
    ata_err_mc    = (1 << 5), ///< Media changed.
    ata_err_unc   = (1 << 6), ///< Uncorrectable data error.
    ata_err_bbk   = (1 << 7), ///< Bad Block detected.
} ata_error_t;

/// @brief ATA Status Bits
typedef enum {
    ata_status_err  = (1 << 0), ///< Indicates an error occurred.
    ata_status_idx  = (1 << 1), ///< Index. Always set to zero.
    ata_status_corr = (1 << 2), ///< Corrected data. Always set to zero.
    ata_status_drq  = (1 << 3), ///< Set when the drive has PIO data to transfer, or is ready to accept PIO data.
    ata_status_srv  = (1 << 4), ///< Overlapped Mode Service Request.
    ata_status_df   = (1 << 5), ///< Drive Fault Error (does not set ERR).
    ata_status_rdy  = (1 << 6), ///< Bit is clear when drive is spun down, or after an error. Set otherwise.
    ata_status_bsy  = (1 << 7), ///< The drive is preparing to send/receive data (wait for it to clear).
} ata_status_t;

/// @brief ATA Control Bits
typedef enum {
    ata_control_zero = 0x00, ///< Always set to zero.
    ata_control_nien = 0x02, ///< Set this to stop the current device from sending interrupts.
    ata_control_srst = 0x04, ///< Set, then clear (after 5us), this to do a "Software Reset" on all ATA drives on a bus, if one is misbehaving.
    ata_control_hob  = 0x80, ///< Set this to read back the High Order Byte (HOB) of the last LBA48 value sent to an IO port.
} ata_control_t;

/// @brief Types of ATA devices.
typedef enum {
    ata_dev_type_unknown,   ///< Device type not recognized.
    ata_dev_type_no_device, ///< No device detected.
    ata_dev_type_pata,      ///< Parallel ATA drive.
    ata_dev_type_sata,      ///< Serial ATA drive.
    ata_dev_type_patapi,    ///< Parallel ATAPI drive.
    ata_dev_type_satapi     ///< Serial ATAPI drive.
} ata_device_type_t;

/// @brief Values used to manage bus mastering.
typedef enum {
    ata_bm_stop_bus_master  = 0x00, ///< Halts bus master operations of the controller.
    ata_bm_start_bus_master = 0x01, ///< Enables bus master operation of the controller.
} ata_bus_mastering_command_t;

/// @brief DMA specific commands.
typedef enum {
    ata_dma_command_read           = 0xC8, ///< Read DMA with retries (28 bit LBA).
    ata_dma_command_read_no_retry  = 0xC9, ///< Read DMA without retries (28 bit LBA).
    ata_dma_command_write          = 0xCA, ///< Write DMA with retries (28 bit LBA).
    ata_dma_command_write_no_retry = 0xCB, ///< Write DMA without retries (28 bit LBA).
} ata_dma_command_t;

/// @brief ATA identity commands.
typedef enum {
    ata_command_pata_ident   = 0xEC, ///< Identify Device.
    ata_command_patapi_ident = 0xA1, ///< Identify Device.
} ata_identity_command_t;

/// @}
