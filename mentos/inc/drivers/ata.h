///                MentOS, The Mentoring Operating system project
// @file ata.h
/// @brief ATA values and data structures.
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "stdint.h"

/// @defgroup ata_defines The ATA configuration registers.
/// @{

/// @defgroup status_register Status register bits
/// @{

/// Device Busy
#define ATA_STAT_BUSY 0x80
/// Device Ready
#define ATA_STAT_READY 0x40
/// Device Fault
#define ATA_STAT_FAULT 0x20
/// Device Seek Complete
#define ATA_STAT_SEEK 0x10
/// Data Request (ready)
#define ATA_STAT_DRQ 0x08
/// Corrected Data Error
#define ATA_STAT_CORR 0x04
/// Vendor specific
#define ATA_STAT_INDEX 0x02
/// Error
#define ATA_STAT_ERR 0x01

/// @}

/// @defgroup device_registers Device / Head Register Bits
/// @{

#define ATA_DEVICE(x) ((x & 1) << 4)
#define ATA_LBA 0xE0

/// @}

/// @defgroup ata_commands ATA Commands
/// @{

/// Read Sectors (with retries)
#define ATA_CMD_READ 0x20
/// Read Sectors (no  retries)
#define ATA_CMD_READN 0x21
/// Write Sectores (with retries)
#define ATA_CMD_WRITE 0x30
/// Write Sectors  (no  retries)
#define ATA_CMD_WRITEN 0x31
/// Read Verify  (with retries)
#define ATA_CMD_VRFY 0x40
/// Read verify  (no  retries)
#define ATA_CMD_VRFYN 0x41
/// Seek
#define ATA_CMD_SEEK 0x70
/// Execute Device Diagnostic
#define ATA_CMD_DIAG 0x90
/// Initialize Device Parameters
#define ATA_CMD_INIT 0x91
/// Read Multiple
#define ATA_CMD_RD_MULT 0xC4
/// Write Multiple
#define ATA_CMD_WR_MULT 0xC5
/// Set Multiple Mode
#define ATA_CMD_SETMULT 0xC6
/// Read DMA (with retries)
#define ATA_CMD_RD_DMA 0xC8
/// Read DMS (no  retries)
#define ATA_CMD_RD_DMAN 0xC9
/// Write DMA (with retries)
#define ATA_CMD_WR_DMA 0xCA
/// Write DMA (no  retires)
#define ATA_CMD_WR_DMAN 0xCB
/// Identify Device
#define ATA_CMD_IDENT 0xEC
#define ATA_CMD_CH_FLSH 0xE7
/// Set Features
#define ATA_CMD_SETF 0xEF
/// Check Power Mode
#define ATA_CMD_CHK_PWR 0xE5

/// @}

/// @defgroup atapi_commands ATAPI Commands
/// @{

#define ATAPI_CMD_PACKET 0xA0
#define ATAPI_CMD_ID_PCKT 0xA1

/// @}

#define ATA_IDENT_DEVICETYPE 0
#define ATA_IDENT_CYLINDERS 2
#define ATA_IDENT_HEADS 6
#define ATA_IDENT_SECTORS 12
#define ATA_IDENT_SERIAL 20
#define ATA_IDENT_MODEL 54
#define ATA_IDENT_CAPABILITIES 98
#define ATA_IDENT_FIELDVALID 106
#define ATA_IDENT_MAX_LBA 120
#define ATA_IDENT_COMMANDSETS 164
#define ATA_IDENT_MAX_LBA_EXT 200

#define IDE_ATA 0x00
#define IDE_ATAPI 0x01

#define ATA_MASTER 0x00
#define ATA_SLAVE 0x01

#define ATA_REG_DATA 0x00
#define ATA_REG_ERROR 0x01
#define ATA_REG_FEATURES 0x01
#define ATA_REG_SECCOUNT0 0x02
#define ATA_REG_LBA0 0x03
#define ATA_REG_LBA1 0x04
#define ATA_REG_LBA2 0x05
#define ATA_REG_HDDEVSEL 0x06
#define ATA_REG_COMMAND 0x07
#define ATA_REG_STATUS 0x07
#define ATA_REG_SECCOUNT1 0x08
#define ATA_REG_LBA3 0x09
#define ATA_REG_LBA4 0x0A
#define ATA_REG_LBA5 0x0B
#define ATA_REG_CONTROL 0x0C
#define ATA_REG_ALTSTATUS 0x0C
#define ATA_REG_DEVADDRESS 0x0D

// Channels:
#define ATA_PRIMARY 0x00
#define ATA_SECONDARY 0x01

// Directions:
#define ATA_READ 0x00
#define ATA_WRITE 0x01

/// @}

typedef struct {
	uint16_t base;
	uint16_t ctrl;
	uint16_t bmide;
	uint16_t nien;
} ide_channel_regs_t;

typedef struct {
	uint8_t reserved;
	uint8_t channel;
	uint8_t drive;
	uint16_t type;
	uint16_t signature;
	uint16_t capabilities;
	uint32_t command_sets;
	uint32_t size;
	uint8_t model[41];
} ide_device_t;

typedef struct {
	uint8_t status;
	uint8_t chs_first_sector[3];
	uint8_t type;
	uint8_t chs_last_sector[3];
	uint32_t lba_first_sector;
	uint32_t sector_count;
} partition_t;

typedef struct {
	uint16_t flags;
	uint16_t unused1[9];
	char serial[20];
	uint16_t unused2[3];
	char firmware[8];
	char model[40];
	uint16_t sectors_per_int;
	uint16_t unused3;
	uint16_t capabilities[2];
	uint16_t unused4[2];
	uint16_t valid_ext_data;
	uint16_t unused5[5];
	uint16_t size_of_rw_mult;
	uint32_t sectors_28;
	uint16_t unused6[38];
	uint64_t sectors_48;
	uint16_t unused7[152];
} __attribute__((packed)) ata_identify_t;

typedef struct {
	uint8_t boostrap[446];
	partition_t partitions[4];
	uint8_t signature[2];
} __attribute__((packed)) mbr_t;

int ata_initialize();

int ata_finalize();
