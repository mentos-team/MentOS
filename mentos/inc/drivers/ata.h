/// @file ata.h
/// @brief Drivers for the Advanced Technology Attachment (ATA) devices.
/// @details
/// IDE is a keyword which refers to the electrical specification of the cables
///  which connect ATA drives (like hard drives) to another device. The drives
///  use the ATA (Advanced Technology Attachment) interface. An IDE cable also
///  can terminate at an IDE card connected to PCI.
/// ATAPI is an extension to ATA (recently renamed to PATA) which adds support
///  for the SCSI command set.
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.
/// @addtogroup drivers Device Drivers
/// @{
/// @addtogroup ata Advanced Technology Attachment (ATA)
/// @brief Drivers for the Advanced Technology Attachment (ATA) devices.
/// @{

#pragma once

/// @brief Initializes the ATA drivers.
/// @return 0 on success, 1 on error.
int ata_initialize();

/// @brief De-initializes the ATA drivers.
/// @return 0 on success, 1 on error.
int ata_finalize();

/// @}
/// @}
