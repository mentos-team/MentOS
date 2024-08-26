/// @file ata.c
/// @brief Advanced Technology Attachment (ATA) and Advanced Technology Attachment Packet Interface (ATAPI) drivers.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.
/// @addtogroup ata
/// @{

// Setup the logging for this file (do this before any other include).
#include "sys/kernel_levels.h"           // Include kernel log levels.
#define __DEBUG_HEADER__ "[ATA   ]"      ///< Change header.
#define __DEBUG_LEVEL__  LOGLEVEL_NOTICE ///< Set log level.
#include "io/debug.h"                    // Include debugging functions.

#include "drivers/ata/ata.h"
#include "drivers/ata/ata_types.h"

#include "descriptor_tables/isr.h"
#include "devices/pci.h"
#include "fcntl.h"
#include "fs/vfs.h"
#include "hardware/pic8259.h"
#include "io/port_io.h"
#include "klib/spinlock.h"
#include "mem/kheap.h"
#include "process/wait.h"
#include "stdio.h"
#include "string.h"
#include "sys/errno.h"
#include "system/panic.h"

/// @brief IDENTIFY device data (response to 0xEC).
typedef struct ata_identity_t {
    /// Word      0 : General configuration.
    struct {
        /// Reserved.
        uint16_t reserved1 : 1;
        /// This member is no longer used.
        uint16_t retired3 : 1;
        /// Indicates that the response was incomplete.
        uint16_t response_incomplete : 1;
        /// This member is no longer used.
        uint16_t retired2 : 3;
        /// Indicates when set to 1 that the device is fixed.
        uint16_t fixed_fevice : 1;
        /// Indicates when set to 1 that the media is removable.
        uint16_t removable_media : 1;
        /// This member is no longer used.
        uint16_t retired1 : 7;
        /// Indicates when set to 1 that the device is an ATA device.
        uint16_t device_type : 1;
    } general_configuration;
    /// Indicates the number of cylinders on the device.
    uint16_t num_cylinders;
    /// Specific configuration.
    uint16_t specific_configuration;
    /// Number of logical heads on the device.
    uint16_t num_heads;
    /// This member is no longer used.
    uint16_t retired1[2];
    /// Indicates the number of sectors per track.
    uint16_t num_sectors_per_track;
    /// Contains the first ID of the device's vendor.
    uint16_t vendor_unique1[3];
    /// Word  10-19 : Contains the serial number of the device.
    uint8_t serial_number[20];
    /// Word  20-22 : Unused.
    uint16_t unused2[3];
    /// Word  23-26 : Contains the revision number of the device's firmware.
    uint8_t firmware_revision[8];
    /// Word  27-46 : Contains the device's model number.
    uint8_t model_number[40];
    /// Word     47 : Maximum number of sectors that shall be transferred per interrupt.
    uint8_t maximum_block_transfer;
    /// Word     48 : Unused.
    uint8_t unused3;
    /// Word  49-50 :
    struct {
        /// Unused.
        uint8_t current_long_physical_sector_alignment : 2;
        /// Reserved.
        uint8_t reserved_byte49 : 6;
        /// Indicates that the device supports DMA operations.
        uint8_t dma_supported : 1;
        /// Indicates that the device supports logical block addressing.
        uint8_t lba_supported : 1;
        /// Indicates when set to 1 that I/O channel ready is disabled for the device.
        uint8_t io_rdy_disable : 1;
        /// Indicates when set to 1 that I/O channel ready is supported by the device.
        uint8_t io_rdy_supported : 1;
        /// Reserved.
        uint8_t reserved1 : 1;
        /// Indicates when set to 1 that the device supports standby timers.
        uint8_t stand_by_timer_support : 1;
        /// Reserved.
        uint8_t reserved2 : 2;
        /// Reserved.
        uint16_t reserved_word50;
    } capabilities;
    /// Word  51-52 : Obsolete.
    uint16_t unused4[2];
    /// Word     53 : Bit 0 = obsolete; Bit 1 = words 70:64 valid; bit 2 = word 88 valid.
    uint16_t valid_ext_data;
    /// Word  54-58 : Obsolete.
    uint16_t unused5[5];
    /// Word     59 : Indicates the multisector setting.
    uint8_t current_multisector_setting;
    /// Indicates when TRUE that the multisector setting is valid.
    uint8_t multisector_setting_valid : 1;
    /// Reserved.
    uint8_t reserved_byte59 : 3;
    /// The device supports the sanitize command.
    uint8_t sanitize_feature_supported : 1;
    /// The device supports cryptographic erase.
    uint8_t crypto_scramble_ext_command_supported : 1;
    /// The device supports block overwrite.
    uint8_t overwrite_ext_command_supported : 1;
    /// The device supports block erase.
    uint8_t block_erase_ext_command_supported : 1;
    /// Word  60-61 : Contains the total number of 28 bit LBA addressable sectors on the drive.
    uint32_t sectors_28;
    /// Word  62-99 : We do not care for these right now.
    uint16_t unused6[38];
    /// Word 100-103: Contains the total number of 48 bit addressable sectors on the drive.
    uint64_t sectors_48;
    /// Word 104-256: We do not care for these right now.
    uint16_t unused7[152];
} ata_identity_t;

/// @brief Physical Region Descriptor Table (PRDT) entry.
/// @details
/// The physical memory region to be transferred is described by a Physical
/// Region Descriptor (PRD). The data transfer will proceed until all regions
/// described by the PRDs in the table have been transferred.
/// Each Physical Region Descriptor entry is 8 bytes in length.
///         |    byte 3  |  byte 2  |  byte 1  |  byte 0    |
/// Dword 0 |  Memory Region Physical Base Address [31:1] |0|
/// Dword 1 |  EOT | reserved       | Byte Count   [15:1] |0|
typedef struct prdt_t {
    /// The first 4 bytes specify the byte address of a physical memory region.
    unsigned int physical_address;
    /// The next two bytes specify the count of the region in bytes (64K byte limit per region).
    unsigned short byte_count;
    /// Bit 7 of the last byte indicates the end of the table
    unsigned short end_of_table;
} prdt_t;

/// @brief Stores information about an ATA device.
typedef struct ata_device_t {
    /// Name of the device.
    char name[NAME_MAX];
    /// Name of the device.
    char path[PATH_MAX];
    /// Does the device support ATA Packet Interface (ATAPI).
    ata_device_type_t type;
    /// The "I/O" port base.
    uint16_t io_base;
    /// I/O registers.
    struct {
        /// [R/W] Data Register. Read/Write PIO data bytes (16-bit).
        uint16_t data;
        /// [R  ] Error Register. Read error generated by the last ATA command executed (8-bit).
        uint16_t error;
        /// [  W] Features Register. Used to control command specific interface features (8-bit).
        uint16_t feature;
        /// [R/W] Sector Count Register. Number of sectors to read/write (0 is a special value) (8-bit).
        uint16_t sector_count;
        /// [R/W] Sector Number Register. This is CHS/LBA28/LBA48 specific (8-bit).
        uint16_t lba_lo;
        /// [R/W] Cylinder Low Register. Partial Disk Sector address (8-bit).
        uint16_t lba_mid;
        /// [R/W] Cylinder High Register. Partial Disk Sector address (8-bit).
        uint16_t lba_hi;
        /// [R/W] Drive / Head Register. Used to select a drive and/or head. Supports extra address/flag bits (8-bit).
        uint16_t hddevsel;
        /// [R  ] Status Register. Used to read the current status (8-bit).
        uint16_t status;
        /// [  W] Command Register. Used to send ATA commands to the device (8-bit).
        uint16_t command;
    } io_reg;
    /// The "Control" port base.
    uint16_t io_control;
    /// If the device is connected to the primary bus.
    bool_t primary : 8;
    /// If the device is connected to the secondary bus.
    bool_t secondary : 8;
    /// If the device is master.
    bool_t master : 8;
    /// If the device is slave.
    bool_t slave : 8;
    /// The device identity data.
    ata_identity_t identity;
    /// Bus Master Register. The "address" of the Bus Master Register is stored
    /// in BAR4, in the PCI Configuration Space of the disk controller. The Bus
    /// Master Register is generally a set of 16 sequential IO ports. It can
    /// also be a 16 byte memory mapped space.
    struct {
        /// The command byte has only 2 operational bits. All the rest should be
        /// 0. Bit 0 (value = 1) is the Start/Stop bit. Setting the bit puts the
        /// controller in DMA mode for that ATA channel, and it starts at the
        /// beginning of the respective PRDT. Clearing the bit terminates DMA
        /// mode for that ATA channel. If the controller was in the middle of a
        /// transfer, the remaining data is thrown away. Also, the controller
        /// does not remember how far it got in the PRDT. That information is
        /// lost, if the OS does not save it. The bit must be cleared when a
        /// transfer completes.
        /// Bit 3 (value = 8) is the Read/Write bit. This bit is a huge problem.
        /// The disk controller does not automatically detect whether the next
        /// disk operation is a read or write. You have to tell it, in advance,
        /// by setting this bit. Note that when reading from the disk, you must
        /// set this bit to 1, and clear it when writing to the disk. You must
        /// first stop DMA transfers (by clearing bit 0) before you can change
        /// the Read/Write bit! Please note all the bad consequences of clearing
        /// bit 0, above! The controller loses its place in the PRDT.
        /// In essence, this means that each PRDT must consist exclusively of
        /// either read or write entries. You set the Read/Write bit in advance,
        /// then "use up" the entire PRDT -- before you can do the opposite
        /// operation.
        unsigned command;
        /// The bits in the status byte are not usually useful. However, you are
        /// required to read it after every IRQ on disk reads anyway. Reading this
        /// byte may perform a necessary final cache flush of the DMA data to
        /// memory.
        unsigned status;
        /// Physical Region Descriptor Table (PRDT). The PRDT must be uint32_t
        /// aligned, contiguous in physical memory, and cannot cross a 64K boundary.
        unsigned prdt;
    } bmr;
    /// @brief Direct Memroy Access (DMA) variables.
    struct {
        /// Pointer to the first entry of the PRDT.
        prdt_t *prdt;
        /// Physical address of the first entry of the PRDT.
        uintptr_t prdt_phys;
        /// Pointer to the DMA memory area.
        uint8_t *start;
        /// Physical address of the DMA memory area.
        uintptr_t start_phys;
    } dma;
    /// Device root file.
    vfs_file_t *fs_root;
    /// For device lock.
    spinlock_t lock;
} ata_device_t;

#define ATA_SECTOR_SIZE 512 ///< The sector size.
#define ATA_DMA_SIZE    512 ///< The size of the DMA area.

/// @brief Keeps track of the incremental letters for the ATA drives.
static char ata_drive_char = 'a';
/// @brief Keeps track of the incremental number for removable media.
static int cdrom_number = 0;
/// @brief We store the ATA pci address here.
static uint32_t ata_pci = 0x00000000;

/// @brief The ATA primary master control register locations.
static ata_device_t ata_primary_master = {
    .io_base = 0x1F0,
    .io_reg  = {
         .data         = 0x1F0 + 0x00,
         .error        = 0x1F0 + 0x01,
         .feature      = 0x1F0 + 0x01,
         .sector_count = 0x1F0 + 0x02,
         .lba_lo       = 0x1F0 + 0x03,
         .lba_mid      = 0x1F0 + 0x04,
         .lba_hi       = 0x1F0 + 0x05,
         .hddevsel     = 0x1F0 + 0x06,
         .status       = 0x1F0 + 0x07,
         .command      = 0x1F0 + 0x07,
    },
    .io_control = 0x3F6,
    .primary    = 1,
    .secondary  = 0,
    .master     = 1,
    .slave      = 0
};

/// @brief The ATA primary slave control register locations.
static ata_device_t ata_primary_slave = {
    .io_base = 0x1F0,
    .io_reg  = {
         .data         = 0x1F0 + 0x00,
         .error        = 0x1F0 + 0x01,
         .feature      = 0x1F0 + 0x01,
         .sector_count = 0x1F0 + 0x02,
         .lba_lo       = 0x1F0 + 0x03,
         .lba_mid      = 0x1F0 + 0x04,
         .lba_hi       = 0x1F0 + 0x05,
         .hddevsel     = 0x1F0 + 0x06,
         .status       = 0x1F0 + 0x07,
         .command      = 0x1F0 + 0x07,
    },
    .io_control = 0x3F6,
    .primary    = 1,
    .secondary  = 0,
    .master     = 0,
    .slave      = 1
};

/// @brief The ATA secondary master control register locations.
static ata_device_t ata_secondary_master = {
    .io_base = 0x170,
    .io_reg  = {
         .data         = 0x170 + 0x00,
         .error        = 0x170 + 0x01,
         .feature      = 0x170 + 0x01,
         .sector_count = 0x170 + 0x02,
         .lba_lo       = 0x170 + 0x03,
         .lba_mid      = 0x170 + 0x04,
         .lba_hi       = 0x170 + 0x05,
         .hddevsel     = 0x170 + 0x06,
         .status       = 0x170 + 0x07,
         .command      = 0x170 + 0x07,
    },
    .io_control = 0x376,
    .primary    = 0,
    .secondary  = 1,
    .master     = 1,
    .slave      = 0
};

/// @brief The ATA secondary slave control register locations.
static ata_device_t ata_secondary_slave = {
    .io_base = 0x170,
    .io_reg  = {
         .data         = 0x170 + 0x00,
         .error        = 0x170 + 0x01,
         .feature      = 0x170 + 0x01,
         .sector_count = 0x170 + 0x02,
         .lba_lo       = 0x170 + 0x03,
         .lba_mid      = 0x170 + 0x04,
         .lba_hi       = 0x170 + 0x05,
         .hddevsel     = 0x170 + 0x06,
         .status       = 0x170 + 0x07,
         .command      = 0x170 + 0x07,
    },
    .io_control = 0x376,
    .primary    = 0,
    .secondary  = 1,
    .master     = 0,
    .slave      = 1
};

// == SUPPORT FUNCTIONS =======================================================

/// @brief Returns the set of ATA errors as a string.
/// @param error the variable containing all the error flags.
/// @return the string with the list of errors.
static inline const char *ata_get_device_error_str(uint8_t error)
{
    static char str[50] = { 0 };
    memset(str, 0, sizeof(str));
    if (error & ata_err_amnf) {
        strcat(str, "amnf,");
    }
    if (error & ata_err_tkznf) {
        strcat(str, "tkznf,");
    }
    if (error & ata_err_abrt) {
        strcat(str, "abrt,");
    }
    if (error & ata_err_mcr) {
        strcat(str, "mcr,");
    }
    if (error & ata_err_idnf) {
        strcat(str, "idnf,");
    }
    if (error & ata_err_mc) {
        strcat(str, "mc,");
    }
    if (error & ata_err_unc) {
        strcat(str, "unc,");
    }
    if (error & ata_err_bbk) {
        strcat(str, "bbk,");
    }
    return str;
}

/// @brief Returns the device status as a string.
/// @param status the device status.
/// @return the device status as string.
static inline const char *ata_get_device_status_str(uint8_t status)
{
    static char str[50] = { 0 };
    memset(str, 0, sizeof(str));
    if (status & ata_status_err) {
        strcat(str, "err,");
    }
    if (status & ata_status_idx) {
        strcat(str, "idx,");
    }
    if (status & ata_status_corr) {
        strcat(str, "corr,");
    }
    if (status & ata_status_drq) {
        strcat(str, "drq,");
    }
    if (status & ata_status_srv) {
        strcat(str, "srv,");
    }
    if (status & ata_status_df) {
        strcat(str, "df,");
    }
    if (status & ata_status_rdy) {
        strcat(str, "rdy,");
    }
    if (status & ata_status_bsy) {
        strcat(str, "bsy,");
    }
    return str;
}

/// @brief Returns the device configuration as string.
/// @param dev the devce.
/// @return the device configuration as string.
static inline const char *ata_get_device_settings_str(ata_device_t *dev)
{
    return (dev->primary) ? ((dev->master) ? "Primary Master" : "Primary Slave") : ((dev->master) ? "Secondary Master" : "Secondary Slave");
}

/// @brief Returns the device type as string.
/// @param type the device type value.
/// @return the device type as string.
static inline const char *ata_get_device_type_str(ata_device_type_t type)
{
    if (type == ata_dev_type_pata) {
        return "pata";
    }
    if (type == ata_dev_type_sata) {
        return "sata";
    }
    if (type == ata_dev_type_patapi) {
        return "patapi";
    }
    if (type == ata_dev_type_satapi) {
        return "satapi";
    }
    if (type == ata_dev_type_unknown) {
        return "unknown";
    }
    return "no_device";
}

/// @brief Dumps on debugging output the device data.
/// @param dev the device to dump.
static inline void ata_dump_device(ata_device_t *dev)
{
    pr_debug("[%s : %s] %s (%s)\n",
             ata_get_device_settings_str(dev), ata_get_device_type_str(dev->type), dev->name, dev->path);
    pr_debug("    io_control : %4u\n", dev->io_control);
    pr_debug("    io_reg (io_base : %4u) {\n", dev->io_base);
    pr_debug("        data   : %4u, error   : %4u, feature : %4u, sector_count : %4u\n",
             dev->io_reg.data, dev->io_reg.error, dev->io_reg.feature, dev->io_reg.sector_count);
    pr_debug("        lba_lo : %4u, lba_mid : %4u, lba_hi  : %4u, hddevsel     : %4u\n",
             dev->io_reg.lba_lo, dev->io_reg.lba_mid, dev->io_reg.lba_hi, dev->io_reg.hddevsel);
    pr_debug("        status : %4u, command : %4u\n", dev->io_reg.status, dev->io_reg.command);
    pr_debug("    }\n");
    pr_debug("    identity {\n");
    pr_debug("        general_configuration {\n");
    pr_debug("            response_incomplete : %4u, fixed_fevice : %4u\n",
             dev->identity.general_configuration.response_incomplete,
             dev->identity.general_configuration.fixed_fevice);
    pr_debug("            removable_media     : %4u, device_type  : %4u\n",
             dev->identity.general_configuration.removable_media,
             dev->identity.general_configuration.device_type);
    pr_debug("        }\n");
    pr_debug("        num_cylinders          : %u\n", dev->identity.num_cylinders);
    pr_debug("        num_heads              : %u\n", dev->identity.num_heads);
    pr_debug("        num_sectors_per_track  : %u\n", dev->identity.num_sectors_per_track);
    pr_debug("        serial_number          : %s\n", dev->identity.serial_number);
    pr_debug("        firmware_revision      : %s\n", dev->identity.firmware_revision);
    pr_debug("        model_number           : %s\n", dev->identity.model_number);
    pr_debug("        maximum_block_transfer : %u\n", dev->identity.maximum_block_transfer);
    pr_debug("        capabilities {\n");
    pr_debug("            current_long_physical_sector_alignment : %u\n", dev->identity.capabilities.current_long_physical_sector_alignment);
    pr_debug("            reserved_byte49                        : %u\n", dev->identity.capabilities.reserved_byte49);
    pr_debug("            dma_supported                          : %u\n", dev->identity.capabilities.dma_supported);
    pr_debug("            lba_supported                          : %u\n", dev->identity.capabilities.lba_supported);
    pr_debug("            io_rdy_disable                         : %u\n", dev->identity.capabilities.io_rdy_disable);
    pr_debug("            io_rdy_supported                       : %u\n", dev->identity.capabilities.io_rdy_supported);
    pr_debug("            stand_by_timer_support                 : %u\n", dev->identity.capabilities.stand_by_timer_support);
    pr_debug("            reserved_word50                        : %u\n", dev->identity.capabilities.reserved_word50);
    pr_debug("        }\n");
    pr_debug("        valid_ext_data                        : %u\n", dev->identity.valid_ext_data);
    pr_debug("        current_multisector_setting           : %u\n", dev->identity.current_multisector_setting);
    pr_debug("        multisector_setting_valid             : %u\n", dev->identity.multisector_setting_valid);
    pr_debug("        reserved_byte59                       : %u\n", dev->identity.reserved_byte59);
    pr_debug("        sanitize_feature_supported            : %u\n", dev->identity.sanitize_feature_supported);
    pr_debug("        crypto_scramble_ext_command_supported : %u\n", dev->identity.crypto_scramble_ext_command_supported);
    pr_debug("        overwrite_ext_command_supported       : %u\n", dev->identity.overwrite_ext_command_supported);
    pr_debug("        block_erase_ext_command_supported     : %u\n", dev->identity.block_erase_ext_command_supported);
    pr_debug("        sectors_28                            : %u\n", dev->identity.sectors_28);
    pr_debug("        sectors_48                            : %u\n", dev->identity.sectors_48);
    pr_debug("    }\n");
    pr_debug("    bmr {\n");
    pr_debug("        command : %6u, status : %6u, prdt : %6u\n", dev->bmr.command, dev->bmr.status, dev->bmr.prdt);
    pr_debug("    }\n");
    pr_debug("    dma {\n");
    pr_debug("        prdt  : 0x%p (Ph: 0x%p)\n", dev->dma.prdt, dev->dma.prdt_phys);
    pr_debug("        start : 0x%p (Ph: 0x%p)\n", dev->dma.start, dev->dma.start_phys);
    pr_debug("    }\n");
}

/// @brief Waits for 400 nanoseconds.
/// @param dev the device on which we wait.
static inline void ata_io_wait(ata_device_t *dev)
{
    inportb(dev->io_control);
    inportb(dev->io_control);
    inportb(dev->io_control);
    inportb(dev->io_control);
}

/// @brief Wait until the status bits selected through the mask are zero.
/// @param dev the device we need to wait for.
/// @param mask the mask we use to access those bits.
/// @param timeout the maximum number of cycles we are going to wait.
/// @return 1 on success, 0 if it times out.
static inline int ata_status_wait_not(ata_device_t *dev, long mask, long timeout)
{
    uint8_t status;
    do {
        status = inportb(dev->io_reg.status);
    } while (((status & mask) == mask) && (--timeout > 0));
    if (timeout > 0) {
        return 0;
    }
    return 1;
}

/// @brief Wait until the status bits selected through the mask are one.
/// @param dev the device we need to wait for.
/// @param mask the mask we use to access those bits.
/// @param timeout the maximum number of cycles we are going to wait.
/// @return 1 on success, 0 if it times out.
static inline int ata_status_wait_for(ata_device_t *dev, long mask, long timeout)
{
    uint8_t status;
    do {
        status = inportb(dev->io_reg.status);
    } while (((status & mask) != mask) && (--timeout > 0));
    if (timeout > 0) {
        return 0;
    }
    return 1;
}

/// @brief Prints the status and error information about the device.
/// @param dev the device for which we print the information.
static inline void ata_print_status_error(ata_device_t *dev)
{
    uint8_t error = inportb(dev->io_reg.error), status = inportb(dev->io_reg.status);
    if (error) {
        pr_err("[%s] Device error [%s] status [%s]\n",
               ata_get_device_settings_str(dev),
               ata_get_device_error_str(error),
               ata_get_device_status_str(status));
    }
}

/// @brief Ge the maximum offset for the given device.
/// @param dev
/// @return uint64_t
static inline uint64_t ata_max_offset(ata_device_t *dev)
{
    if (dev->identity.sectors_48) {
        return dev->identity.sectors_48 * ATA_SECTOR_SIZE;
    }
    if (dev->identity.sectors_28) {
        return dev->identity.sectors_28 * ATA_SECTOR_SIZE;
    }
    pr_warning("Neither sectors_48 nor sectors_28 are set.\n");
    return 0;
}

/// @brief Fixes all ATA-related strings.
/// @param str string to fix.
/// @param len length of the string.
static inline void ata_fix_string(char *str, size_t len)
{
    char tmp;
    for (size_t i = 0; i < len; i += 2) {
        tmp        = str[i + 1];
        str[i + 1] = str[i];
        str[i]     = tmp;
    }
    str[len] = 0;
}

/// @brief Performs a soft reset of the device.
/// @details "For non-ATAPI drives, the only method a driver has of resetting a
/// drive after a major error is to do a "software reset" on the bus. Set bit 2
/// (SRST, value = 4) in the proper Control Register for the bus. This will
/// reset both ATA devices on the bus."
/// @param dev the device on which we perform the soft reset.
static inline void ata_soft_reset(ata_device_t *dev)
{
    pr_debug("[%s] Performing ATA soft reset...\n", ata_get_device_settings_str(dev));
    ata_print_status_error(dev);
    // Write reset.
    outportb(dev->io_control, ata_control_srst);
    // Flush.
    inportb(dev->io_control);
    // Wait for the soft reset to complete.
    ata_io_wait(dev);
    // Reset the bus to normal operation.
    outportb(dev->io_control, ata_control_zero);
    // Flush.
    inportb(dev->io_control);
    // Wait until master drive is ready again.
    ata_status_wait_not(dev, ata_status_bsy | ata_status_drq, 100000);
}

/// @brief Creates the DMA memory area used to write and read on the device.
/// @param size the size of the DMA memory area.
/// @param physical the physical address of the DMA memory area.
/// @return the logical address of the DMA memory area.
static inline uintptr_t ata_dma_malloc(size_t size, uintptr_t *physical)
{
    // Get the page order to accomodate the size.
    uint32_t order           = find_nearest_order_greater(0, size);
    page_t *page             = _alloc_pages(GFP_KERNEL, order);
    *physical                = get_physical_address_from_page(page);
    uintptr_t lowmem_address = get_lowmem_address_from_page(page);
    pr_debug("Size requirement is %d, which results in an order %d\n", size, order);
    pr_debug("Allocated page is at       : 0x%p\n", page);
    pr_debug("The physical address is at : 0x%p\n", physical);
    pr_debug("The lowmem address is at   : 0x%p\n", lowmem_address);
    return lowmem_address;
}

/// @brief Emables bus mastering, allowing Direct Memory Access (DMA) transactions.
static inline void ata_dma_enable_bus_mastering(void)
{
    uint32_t pci_cmd = pci_read_32(ata_pci, PCI_COMMAND);
    if (bit_check(pci_cmd, pci_command_bus_master)) {
        pr_warning("Bus mastering already enabled.\n");
    } else {
        // Set the bit for bus mastering.
        bit_set_assign(pci_cmd, pci_command_bus_master);
        // Write the PCI command field.
        pci_write_32(ata_pci, PCI_COMMAND, pci_cmd);
        // Check that the bus mastering is enabled.
        pci_cmd = pci_read_32(ata_pci, PCI_COMMAND);
        if (!bit_check(pci_cmd, pci_command_bus_master)) {
            pr_warning("Bus mastering is not correctly set.\n");
            kernel_panic("Failed ATA initialization.");
        }
    }
}

/// @brief Initialize the bmr field of the ATA device.
/// @param dev the device to initialize.
/// @details
/// When you want to retrieve the actual base address of a BAR, be sure to mask
/// the lower bits.
/// For 16-bit Memory Space BARs, you calculate (BAR[x] & 0xFFF0).
/// For 32-bit Memory Space BARs, you calculate (BAR[x] & 0xFFFFFFF0).
static inline void ata_dma_initialize_bus_mastering_address(ata_device_t *dev)
{
    uint32_t address = pci_read_32(ata_pci, PCI_BASE_ADDRESS_4);
    // To distinguish between memory space BARs and I/O space BARs, you can
    // check the value of the lowest bit. memory space BARs has always a 0,
    // while I/O space BARs has always a 1.
    if (!bit_check(address, 0)) {
        pr_warning("[%s] Failed to initialize BUS Mastering.\n", ata_get_device_settings_str(dev));
        kernel_panic("Failed ATA initialization.");
    }
    /// When you want to retrieve the actual base address of a BAR, be sure to
    /// mask the lower bits, for I/O space BARs you calculate (BAR & 0xFFFFFFFC).
    address &= 0xFFFFFFFC;
    // Differentiate between primary or secondary ATA bus.
    if (dev->primary) {
        dev->bmr.command = address + 0x0;
        dev->bmr.status  = address + 0x2;
        dev->bmr.prdt    = address + 0x4;
    } else {
        dev->bmr.command = address + 0x8;
        dev->bmr.status  = address + 0xA;
        dev->bmr.prdt    = address + 0xC;
    }
}

// == ATA DEVICE MANAGEMENT ===================================================

/// @brief Detects the type of device.
/// @param dev the device for which we are checking the type.
/// @return the device type.
static inline ata_device_type_t ata_detect_device_type(ata_device_t *dev)
{
    pr_debug("[%s] Detecting device type...\n", ata_get_device_settings_str(dev));
    // Select the drive.
    outportb(dev->io_reg.hddevsel, 0xA0 | (dev->slave << 4U));
    // Wait for the command to work.
    ata_io_wait(dev);
    // Select the ATA device.
    outportb(dev->io_base + 1, 1);
    // Disable IRQs.
    outportb(dev->io_control, 0);
    // Select the device.
    outportb(dev->io_reg.hddevsel, 0xA0 | (dev->slave << 4U));
    // Wait 400ns for the command to work.
    ata_io_wait(dev);
    // The host is prohibited from writing the Features, Sector Count, Sector
    // Number, Cylinder Low, Cylinder High, or Device/Head registers when either
    // BSY or DRQ is set in the Status Register. Any write to the Command
    // Register when BSY or DRQ is set is ignored unless the write is to issue a
    // Device Reset command.
    if (ata_status_wait_not(dev, ata_status_bsy | ata_status_drq, 100000)) {
        ata_print_status_error(dev);
        return 1;
    }
    // ATA specs say these values must be zero before sending IDENTIFY.
    outportb(dev->io_reg.sector_count, 0);
    outportb(dev->io_reg.lba_lo, 0);
    outportb(dev->io_reg.lba_mid, 0);
    outportb(dev->io_reg.lba_hi, 0);
    // Request the device identity.
    outportb(dev->io_reg.command, ata_command_pata_ident);
    // Wait for the device to become non-busy, and ready.
    if (ata_status_wait_not(dev, ata_status_bsy & ~(ata_status_drq | ata_status_rdy), 100000)) {
        ata_print_status_error(dev);
        return 1;
    }
    // Read the identity.
    inportsw(dev->io_reg.data, (uint16_t *)&dev->identity, (sizeof(ata_identity_t) / sizeof(uint16_t)));
    // Fix the serial.
    ata_fix_string((char *)&dev->identity.serial_number, count_of(dev->identity.serial_number) - 1);
    // Fix the firmware.
    ata_fix_string((char *)&dev->identity.firmware_revision, count_of(dev->identity.firmware_revision) - 1);
    // Fix the model.
    ata_fix_string((char *)&dev->identity.model_number, count_of(dev->identity.model_number) - 1);
    // Get the "signature bytes" by reading low and high cylinder register.
    uint8_t lba_lo  = inportb(dev->io_reg.lba_hi);
    uint8_t lba_mid = inportb(dev->io_reg.lba_mid);
    uint8_t lba_hi  = inportb(dev->io_reg.lba_hi);
    // Differentiate ATA, ATAPI, SATA and SATAPI.
    if ((lba_mid == 0x00) && (lba_hi == 0x00)) {
        return ata_dev_type_pata;
    }
    if ((lba_mid == 0x3C) && (lba_hi == 0xC3)) {
        return ata_dev_type_sata;
    }
    if ((lba_mid == 0x14) && (lba_hi == 0xEB)) {
        return ata_dev_type_patapi;
    }
    if ((lba_mid == 0x69) && (lba_hi == 0x96)) {
        return ata_dev_type_satapi;
    }
    if ((lba_mid == 0xFF) && (lba_hi == 0xFF)) {
        return ata_dev_type_no_device;
    }
    return ata_dev_type_unknown;
}

/// @brief Initialises the given device.
/// @param dev the device to initialize.
/// @return 0 on success, 1 on error.
static bool_t ata_device_init(ata_device_t *dev)
{
    pr_debug("[%s] Initializing ATA device...\n", ata_get_device_settings_str(dev));
    // Check the status of the device.
    if (ata_status_wait_for(dev, ata_status_drq | ata_status_rdy, 100000)) {
        ata_print_status_error(dev);
        return 1;
    }
    // Initialize the bus mastering addresses.
    ata_dma_initialize_bus_mastering_address(dev);
    // Check the status of the device.
    if (ata_status_wait_for(dev, ata_status_drq | ata_status_rdy, 100000)) {
        ata_print_status_error(dev);
        return 1;
    }
    // Allocate the memory for the Physical Region Descriptor Table (PRDT).
    dev->dma.prdt = (prdt_t *)ata_dma_malloc(sizeof(prdt_t), &dev->dma.prdt_phys);
    // Allocate the memory for the Direct Memory Access (DMA).
    dev->dma.start = (uint8_t *)ata_dma_malloc(ATA_DMA_SIZE, &dev->dma.start_phys);
    // Initialize the table, specifying the physical address of the DMA.
    dev->dma.prdt->physical_address = dev->dma.start_phys;
    // The size of the DMA.
    dev->dma.prdt->byte_count = ATA_DMA_SIZE;
    // Set the EOT to 1.
    dev->dma.prdt->end_of_table = 0x8000;
    // Print the device data.
    ata_dump_device(dev);
    return 0;
}

// == ATA SECTOR READ/WRITE FUNCTIONS =========================================

/// @brief Reads an ATA sector.
/// @param dev the device on which we perform the read.
/// @param lba_sector the sector where we write.
/// @param buffer the buffer we are writing.
static void ata_device_read_sector(ata_device_t *dev, uint32_t lba_sector, uint8_t *buffer)
{
    // Check if we are trying to perform the read on the correct drive type.
    if ((dev->type != ata_dev_type_pata) && (dev->type != ata_dev_type_sata)) {
        return;
    }
    // pr_debug("ata_device_read_sector(dev: %p, lba_sector: %d, buffer: %p)\n", dev, lba_sector, buffer);
    spinlock_lock(&dev->lock);

    // Wait for the
    if (ata_status_wait_not(dev, ata_status_bsy, 100000)) {
        ata_print_status_error(dev);
        spinlock_unlock(&dev->lock);
        return;
    }

    // Reset bus master register's command register.
    outportb(dev->bmr.command, 0x00);

    // Set the PRDT.
    outportl(dev->bmr.prdt, dev->dma.prdt_phys);

    // Enable error, irq status.
    outportb(dev->bmr.status, inportb(dev->bmr.status) | 0x04 | 0x02);

    // Set read.
    outportb(dev->bmr.command, 0x08);

    if (ata_status_wait_not(dev, ata_status_bsy, 100000)) {
        ata_print_status_error(dev);
        spinlock_unlock(&dev->lock);
        return;
    }

    outportb(dev->io_control, 0x00);
    outportb(dev->io_reg.hddevsel, 0xe0 | (dev->slave << 4));
    ata_io_wait(dev);
    outportb(dev->io_reg.feature, 0x00);

    outportb(dev->io_reg.sector_count, 0);
    outportb(dev->io_reg.lba_lo, (lba_sector & 0xff000000) >> 24);
    outportb(dev->io_reg.lba_mid, (lba_sector & 0xff00000000) >> 32);
    outportb(dev->io_reg.lba_hi, (lba_sector & 0xff0000000000) >> 40);

    outportb(dev->io_reg.sector_count, 1);
    outportb(dev->io_reg.lba_lo, (lba_sector & 0x000000ff) >> 0);
    outportb(dev->io_reg.lba_mid, (lba_sector & 0x0000ff00) >> 8);
    outportb(dev->io_reg.lba_hi, (lba_sector & 0x00ff0000) >> 16);

    if (ata_status_wait_not(dev, ata_status_bsy & ~ata_status_rdy, 100000)) {
        ata_print_status_error(dev);
        spinlock_unlock(&dev->lock);
        return;
    }

    // Write the READ_DMA to the command register (0xC8)
    outportb(dev->io_reg.command, ata_dma_command_read);

    ata_io_wait(dev);

    outportb(dev->bmr.command, 0x08 | 0x01);

    while (1) {
        int status  = inportb(dev->bmr.status);
        int dstatus = inportb(dev->io_reg.status);
        if (!(status & 0x04)) {
            continue;
        }
        if (!(dstatus & ata_status_bsy)) {
            break;
        }
    }

    // Copy from DMA buffer to output buffer.
    memcpy(buffer, dev->dma.start, ATA_DMA_SIZE);

    // Inform device we are done.
    outportb(dev->bmr.status, inportb(dev->bmr.status) | 0x04 | 0x02);

    spinlock_unlock(&dev->lock);
}

/// @brief Writs an ATA sector.
/// @param dev the device on which we perform the write.
/// @param lba_sector the sector where we read.
/// @param buffer the buffer where we store what we read.
static void ata_device_write_sector(ata_device_t *dev, uint32_t lba_sector, uint8_t *buffer)
{
    spinlock_lock(&dev->lock);

    // Copy the buffer over to the DMA area
    memcpy(dev->dma.start, buffer, ATA_DMA_SIZE);

    // Reset bus master register's command register
    outportb(dev->bmr.command, 0);

    // Set prdt
    outportl(dev->bmr.prdt, dev->dma.prdt_phys);

    // Enable error, irq status.
    outportb(dev->bmr.status, inportb(dev->bmr.status) | 0x04 | 0x02);

    // Select drive
    if (ata_status_wait_not(dev, ata_status_bsy, 100000)) {
        ata_print_status_error(dev);
        spinlock_unlock(&dev->lock);
        return;
    }

    outportb(dev->io_reg.hddevsel, 0xe0 | dev->slave << 4 | (lba_sector & 0x0f000000) >> 24);

    if (ata_status_wait_not(dev, ata_status_bsy, 100000)) {
        ata_print_status_error(dev);
        spinlock_unlock(&dev->lock);
        return;
    }

    // Set sector counts and LBAs
    outportb(dev->io_reg.feature, 0x00);
    outportb(dev->io_reg.sector_count, 1);
    outportb(dev->io_reg.lba_lo, (lba_sector & 0x000000ff) >> 0);
    outportb(dev->io_reg.lba_mid, (lba_sector & 0x0000ff00) >> 8);
    outportb(dev->io_reg.lba_hi, (lba_sector & 0x00ff0000) >> 16);

    if (ata_status_wait_not(dev, ata_status_bsy, 100000)) {
        ata_print_status_error(dev);
        spinlock_unlock(&dev->lock);
        return;
    }

    // Notify that we are starting DMA writing.
    outportb(dev->io_reg.command, ata_dma_command_write);

    // Start DMA Writing.
    outportb(dev->bmr.command, 0x1);

    // Wait for dma write to complete.
    while (1) {
        int status  = inportb(dev->bmr.status);
        int dstatus = inportb(dev->io_reg.status);
        if (!(status & 0x04)) {
            continue;
        }
        if (!(dstatus & 0x80)) {
            break;
        }
    }

    // Inform device we are done.
    outportb(dev->bmr.status, inportb(dev->bmr.status) | 0x04 | 0x02);

    spinlock_unlock(&dev->lock);
}

// == VFS CALLBACKS ===========================================================

/// @brief Implements the open function for an ATA device.
/// @param path the phat to the device we want to open.
/// @param flags we ignore these.
/// @param mode we currently ignore this.
/// @return the VFS file associated with the device.
static vfs_file_t *ata_open(const char *path, int flags, mode_t mode)
{
    pr_debug("ata_open(%s, %d, %d)\n", path, flags, mode);
    ata_device_t *dev = NULL;
    if (strcmp(path, ata_primary_master.path) == 0) {
        dev = &ata_primary_master;
    } else if (strcmp(path, ata_primary_slave.path) == 0) {
        dev = &ata_primary_slave;
    } else if (strcmp(path, ata_secondary_master.path) == 0) {
        dev = &ata_secondary_master;
    } else if (strcmp(path, ata_secondary_slave.path) == 0) {
        dev = &ata_secondary_slave;
    } else {
        return NULL;
    }
    if (dev->fs_root) {
        ++dev->fs_root->count;
        return dev->fs_root;
    }
    return NULL;
}

/// @brief Closes an ATA device.
/// @param file the VFS file associated with the ATA device.
/// @return 0 on success, it panics on failure.
static int ata_close(vfs_file_t *file)
{
    pr_debug("ata_close(%p)\n", file);
    // Get the device from the VFS file.
    ata_device_t *dev = (ata_device_t *)file->device;
    // Check the device.
    if (dev == NULL) {
        kernel_panic("Device not set.");
    }
    //
    if ((dev == &ata_primary_master) || (dev == &ata_primary_slave) ||
        (dev == &ata_secondary_master) || (dev == &ata_secondary_slave)) {
        --file->count;
    }
    return 0;
}

/// @brief Reads from an ATA device.
/// @param file the VFS file associated with the ATA device.
/// @param buffer the buffer where we store what we read.
/// @param offset the offset where we want to read.
/// @param size the size of the buffer.
/// @return the number of read characters.
static ssize_t ata_read(vfs_file_t *file, char *buffer, off_t offset, size_t size)
{
    // pr_debug("ata_read(file: 0x%p, buffer: 0x%p, offest: %8d, size: %8d)\n", file, buffer, offset, size);
    // Prepare a static support buffer.
    static char support_buffer[ATA_SECTOR_SIZE];
    // Get the device from the VFS file.
    ata_device_t *dev = (ata_device_t *)file->device;
    // Check the device.
    if (dev == NULL) {
        kernel_panic("Device not set.");
    }

    if ((dev->type == ata_dev_type_pata) || (dev->type == ata_dev_type_sata)) {
        uint32_t start_block  = offset / ATA_SECTOR_SIZE;
        uint32_t start_offset = offset % ATA_SECTOR_SIZE;
        uint32_t end_block    = (offset + size - 1) / ATA_SECTOR_SIZE;
        uint32_t end_offset   = (offset + size - 1) % ATA_SECTOR_SIZE;
        uint32_t prefix_size  = (ATA_SECTOR_SIZE - start_offset);
        uint32_t postfix_size = (offset + size) % ATA_SECTOR_SIZE;
        uint32_t max_offset   = ata_max_offset(dev);
        uint32_t x_offset     = 0;

        // Check if with the offset we are exceeding the size.
        if (offset > max_offset) {
            pr_warning("The offset is exceeding the disk size (%d > %d)\n", offset, max_offset);
            ata_dump_device(dev);
            // Get the error and status information of the device.
            uint8_t error = inportb(dev->io_reg.error), status = inportb(dev->io_reg.status);
            pr_err("Device error  : %s\n", ata_get_device_error_str(error));
            pr_err("Device status : %s\n", ata_get_device_status_str(status));
            return 0;
        }

        // Check if we are going to reading over the size.
        if ((offset + size) > max_offset) {
            size = max_offset - offset;
        }

        if (start_offset) {
            ata_device_read_sector(dev, start_block, (uint8_t *)support_buffer);
            memcpy(buffer, (void *)((uintptr_t)support_buffer + start_offset), prefix_size);
            x_offset += prefix_size;
            ++start_block;
        }

        if (postfix_size && (start_block <= end_block)) {
            ata_device_read_sector(dev, end_block, (uint8_t *)support_buffer);
            memcpy((void *)((uintptr_t)buffer + size - postfix_size), support_buffer, postfix_size);
            --end_block;
        }

        while (start_block <= end_block) {
            ata_device_read_sector(dev, start_block, (uint8_t *)((uintptr_t)buffer + x_offset));
            x_offset += ATA_SECTOR_SIZE;
            ++start_block;
        }
    } else if ((dev->type == ata_dev_type_patapi) || (dev->type == ata_dev_type_satapi)) {
        pr_warning("ATAPI and SATAPI drives are not currently supported.\n");
        size = -EPERM;
    }
    return size;
}

/// @brief Writes on an ATA device.
/// @param file the VFS file associated with the ATA device.
/// @param buffer the buffer we use to write.
/// @param offset the offset where we want to write.
/// @param size the size of the buffer.
/// @return the number of written characters.
static ssize_t ata_write(vfs_file_t *file, const void *buffer, off_t offset, size_t size)
{
    pr_debug("ata_write(%p, %p, %d, %d)\n", file, buffer, offset, size);
    // Prepare a static support buffer.
    static char support_buffer[ATA_SECTOR_SIZE];
    // Get the device from the VFS file.
    ata_device_t *dev = (ata_device_t *)file->device;
    // Check the device.
    if (dev == NULL) {
        kernel_panic("Device not set.");
    }

    if ((dev->type == ata_dev_type_pata) || (dev->type == ata_dev_type_sata)) {
        uint32_t start_block  = offset / ATA_SECTOR_SIZE;
        uint32_t start_offset = offset % ATA_SECTOR_SIZE;
        uint32_t end_block    = (offset + size - 1) / ATA_SECTOR_SIZE;
        uint32_t end_offset   = (offset + size - 1) % ATA_SECTOR_SIZE;
        uint32_t prefix_size  = (ATA_SECTOR_SIZE - start_offset);
        uint32_t postfix_size = (offset + size) % ATA_SECTOR_SIZE;
        uint32_t max_offset   = ata_max_offset(dev);
        uint32_t x_offset     = 0;

        // Check if with the offset we are exceeding the size.
        if (offset > max_offset) {
            return 0;
        }

        // Check if we are going to readoing over the size.
        if (offset + size > max_offset) {
            size = max_offset - offset;
        }
        if (start_offset) {
            ata_device_read_sector(dev, start_block, (uint8_t *)support_buffer);
            memcpy((void *)((uintptr_t)support_buffer + (start_offset)), buffer, prefix_size);
            ata_device_write_sector(dev, start_block, (uint8_t *)support_buffer);
            x_offset += prefix_size;
            ++start_block;
        }

        if (postfix_size && (start_block <= end_block)) {
            ata_device_read_sector(dev, end_block, (uint8_t *)support_buffer);
            memcpy(support_buffer, (void *)((uintptr_t)buffer + size - postfix_size), postfix_size);
            ata_device_write_sector(dev, end_block, (uint8_t *)support_buffer);
            --end_block;
        }

        while (start_block <= end_block) {
            ata_device_write_sector(dev, start_block, (uint8_t *)((uintptr_t)buffer + x_offset));
            x_offset += ATA_SECTOR_SIZE;
            ++start_block;
        }
    } else if ((dev->type == ata_dev_type_patapi) || (dev->type == ata_dev_type_satapi)) {
        pr_warning("ATAPI and SATAPI drives are not currently supported.\n");
        size = -EPERM;
    }
    return size;
}

/// @brief Stats an ATA device.
/// @param dev the ATA device.
/// @param stat the stat buffer.
/// @return 0 on success.
static int _ata_stat(const ata_device_t *dev, stat_t *stat)
{
    if (dev && dev->fs_root) {
        pr_debug("_ata_stat(%p, %p)\n", dev, stat);
        stat->st_dev   = 0;
        stat->st_ino   = 0;
        stat->st_mode  = dev->fs_root->mask;
        stat->st_uid   = dev->fs_root->uid;
        stat->st_gid   = dev->fs_root->gid;
        stat->st_atime = dev->fs_root->atime;
        stat->st_mtime = dev->fs_root->mtime;
        stat->st_ctime = dev->fs_root->ctime;
        stat->st_size  = dev->fs_root->length;
    }
    return 0;
}

/// @brief Retrieves information concerning the file at the given position.
/// @param file the file.
/// @param stat the structure where the information are stored.
/// @return 0 if success.
static int ata_fstat(vfs_file_t *file, stat_t *stat)
{
    return _ata_stat(file->device, stat);
}

/// @brief Retrieves information concerning the file at the given position.
/// @param path the path where the file resides.
/// @param stat the structure where the information are stored.
/// @return 0 if success.
static int ata_stat(const char *path, stat_t *stat)
{
    super_block_t *sb = vfs_get_superblock(path);
    if (sb && sb->root) {
        return _ata_stat(sb->root->device, stat);
    }
    return -1;
}

// == VFS ENTRY GENERATION ====================================================

/// @brief The mount call-back, which prepares everything and calls the actual
/// ATA mount function.
/// @param path the path where the filesystem should be mounted.
/// @param device the device we mount.
/// @return the VFS file of the filesystem.
static vfs_file_t *ata_mount_callback(const char *path, const char *device)
{
    pr_err("mount_callback(%s, %s): ATA has no mount callback!\n", path, device);
    return NULL;
}

/// Filesystem information.
static file_system_type ata_file_system_type = {
    .name     = "ata",
    .fs_flags = 0,
    .mount    = ata_mount_callback
};

/// Filesystem general operations.
static vfs_sys_operations_t ata_sys_operations = {
    .mkdir_f   = NULL,
    .rmdir_f   = NULL,
    .stat_f    = ata_stat,
    .creat_f   = NULL,
    .symlink_f = NULL,
};

/// ATA filesystem file operations.
static vfs_file_operations_t ata_fs_operations = {
    .open_f     = ata_open,
    .unlink_f   = NULL,
    .close_f    = ata_close,
    .read_f     = ata_read,
    .write_f    = ata_write,
    .lseek_f    = NULL,
    .stat_f     = ata_fstat,
    .ioctl_f    = NULL,
    .getdents_f = NULL,
    .readlink_f = NULL,
};

/// @brief Creates a VFS file, starting from an ATA device.
/// @param dev the ATA device.
/// @return a pointer to the VFS file on success, NULL on failure.
static vfs_file_t *ata_device_create(ata_device_t *dev)
{
    // Create the file.
    vfs_file_t *file = kmem_cache_alloc(vfs_file_cache, GFP_KERNEL);
    if (file == NULL) {
        pr_err("Failed to create ATA device.\n");
        return NULL;
    }
    // Set the device name.
    memcpy(file->name, dev->name, NAME_MAX);
    file->uid   = 0;
    file->gid   = 0;
    file->mask  = 0x2000 | 0600;
    file->atime = sys_time(NULL);
    file->mtime = sys_time(NULL);
    file->ctime = sys_time(NULL);
    // Set the device.
    file->device = dev;
    // Re-set the flags.
    file->flags = DT_BLK;
    // Change the operations.
    file->sys_operations = &ata_sys_operations;
    file->fs_operations  = &ata_fs_operations;
    return file;
}

/// @brief Detects and mount the given ATA device.
/// @param dev the device we want to handle.
/// @return the type of device.
static ata_device_type_t ata_device_detect(ata_device_t *dev)
{
    // Perform a soft reset.
    ata_soft_reset(dev);
    // Detect the device type.
    ata_device_type_t type = ata_detect_device_type(dev);
    // Parallel ATA drive, or emulated SATA.
    if ((type == ata_dev_type_pata) || (type == ata_dev_type_sata)) {
        pr_debug("[%s] Found %s device...\n", ata_get_device_settings_str(dev), ata_get_device_type_str(type));
        // Device type supported, set it.
        dev->type = type;
        // Initialize the spinlock.
        spinlock_init(&dev->lock);
        // Set the device name.
        sprintf(dev->name, "hd%c", ata_drive_char);
        // Set the device path.
        sprintf(dev->path, "/dev/hd%c", ata_drive_char);
        // Initialize the drive.
        if (ata_device_init(dev)) {
            pr_debug("[%s] Skip device...\n", ata_get_device_settings_str(dev));
            return ata_dev_type_unknown;
        }
        // Create the filesystem entry for the drive.
        dev->fs_root = ata_device_create(dev);
        // Check if we failed to create the filesystem entry.
        if (!dev->fs_root) {
            pr_alert("Failed to create ata device!\n");
            return ata_dev_type_unknown;
        }
        // Update the filesystem entry with the length of the device.
        dev->fs_root->length = ata_max_offset(dev);
        // Try to mount the drive.
        if (!vfs_register_superblock(dev->fs_root->name, dev->path, &ata_file_system_type, dev->fs_root)) {
            pr_alert("Failed to mount ata device!\n");
            // Free the memory.
            kmem_cache_free(dev->fs_root);
            return ata_dev_type_unknown;
        }
        // Increment the drive letter.
        ++ata_drive_char;
    } else if ((type == ata_dev_type_patapi) || (type == ata_dev_type_satapi)) {
        pr_debug("[%s] ATAPI and SATAPI drives are not currently supported...\n", ata_get_device_settings_str(dev));
        type = ata_dev_type_no_device;
    } else if (type == ata_dev_type_no_device) {
        pr_debug("[%s] Found no device...\n", ata_get_device_settings_str(dev));
    }
    return type;
}

// == IRQ HANDLERS ============================================================
/// @param f The interrupt stack frame.
static void ata_irq_handler_master(pt_regs *f)
{
    pr_warning("ata_irq_handler_master\n");
    inportb(ata_primary_master.io_reg.status);
    inportb(ata_primary_master.bmr.status);
    //outportb(ata_primary_master.bmr.command, ata_bm_stop_bus_master);
    pic8259_send_eoi(IRQ_FIRST_HD);
}

/// @param f The interrupt stack frame.
static void ata_irq_handler_slave(pt_regs *f)
{
    pr_warning("ata_irq_handler_slave\n");
    inportb(ata_secondary_master.io_reg.status);
    inportb(ata_primary_master.bmr.status);
    //outportb(ata_primary_master.bmr.command, ata_bm_stop_bus_master);
    pic8259_send_eoi(IRQ_SECOND_HD);
}

// == PCI FUNCTIONS ===========================================================

/// @brief Used while scanning the PCI interface.
/// @param device the device we want to find.
/// @param vendorid its vendor ID.
/// @param deviceid its device ID.
/// @param extra the devoce once we find it.
static void pci_find_ata(uint32_t device, uint16_t vendorid, uint16_t deviceid, void *extra)
{
    // Intel Corporation AND (IDE Interface OR PIIX4 IDE)
    if ((vendorid == 0x8086) && (deviceid == 0x7010 || deviceid == 0x7111)) {
        *((uint32_t *)extra) = device;
        pci_dump_device_data(device, vendorid, deviceid);
    }
}

// == INITIALIZE/FINALIZE ATA =================================================

int ata_initialize(void)
{
    // Search for ATA devices.
    pci_scan(&pci_find_ata, -1, &ata_pci);

    // Register the filesystem.
    vfs_register_filesystem(&ata_file_system_type);

    // Install the IRQ handlers.
    irq_install_handler(IRQ_FIRST_HD, ata_irq_handler_master, "IDE Master");
    irq_install_handler(IRQ_SECOND_HD, ata_irq_handler_slave, "IDE Slave");

    // Enable bus mastering.
    ata_dma_enable_bus_mastering();

    ata_device_detect(&ata_primary_master);
    ata_device_detect(&ata_primary_slave);
    ata_device_detect(&ata_secondary_master);
    ata_device_detect(&ata_secondary_slave);

    return 0;
}

int ata_finalize(void)
{
    return 0;
}

/// @}
