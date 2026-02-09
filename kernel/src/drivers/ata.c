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
#include "errno.h"
#include "fcntl.h"
#include "fs/vfs.h"
#include "hardware/pic8259.h"
#include "io/port_io.h"
#include "klib/spinlock.h"
#include "math.h"
#include "mem/alloc/zone_allocator.h"
#include "mem/mm/page.h"
#include "process/wait.h"
#include "stdbool.h"
#include "stdio.h"
#include "string.h"
#include "sys/bitops.h"
#include "system/panic.h"
#include "system/syscall.h"

/// @brief IDENTIFY device data (response to 0xEC).
typedef struct ata_identity {
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
typedef struct prdt {
    /// The first 4 bytes specify the byte address of a physical memory region.
    unsigned int physical_address;
    /// The next two bytes specify the count of the region in bytes (64K byte limit per region).
    unsigned short byte_count;
    /// Bit 7 of the last byte indicates the end of the table
    unsigned short end_of_table;
} prdt_t;

/// @brief Stores information about an ATA device.
typedef struct ata_device {
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

#define ATA_CMD_READ_PIO       0x20
#define ATA_CMD_READ_PIO_RETRY 0x21

#define ATA_BMR_CMD_START 0x01
#define ATA_BMR_CMD_READ  0x08

#define ATA_BMR_STATUS_ACTIVE 0x01
#define ATA_BMR_STATUS_ERROR  0x02
#define ATA_BMR_STATUS_IRQ    0x04

#define ATA_DMA_POLL_LIMIT 100000

/// @brief Keeps track of the incremental letters for the ATA drives.
static char ata_drive_char = 'a';
/// @brief Keeps track of the incremental number for removable media.
static int cdrom_number    = 0;
/// @brief We store the ATA pci address here.
static uint32_t ata_pci    = 0x00000000;

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
    static char str[50] = {0};
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
    static char str[50] = {0};
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
    return (dev->primary) ? ((dev->master) ? "Primary Master" : "Primary Slave")
                          : ((dev->master) ? "Secondary Master" : "Secondary Slave");
}

/// @brief Returns the device type as string.
/// @param type the device type value.
/// @return the device type as string.
static inline const char *ata_get_device_type_str(ata_device_type_t type)
{
    if (type == ata_dev_type_pata) {
        return "PATA";
    }
    if (type == ata_dev_type_sata) {
        return "SATA";
    }
    if (type == ata_dev_type_patapi) {
        return "PATAPI";
    }
    if (type == ata_dev_type_satapi) {
        return "SATAPI";
    }
    if (type == ata_dev_type_unknown) {
        return "UNKNOWN";
    }
    return "NONE";
}

/// @brief Dumps on debugging output the device data.
/// @param dev the device to dump.
static inline void ata_dump_device(ata_device_t *dev)
{
    pr_debug(
        "[%s : %s] %s (%s)\n", ata_get_device_settings_str(dev), ata_get_device_type_str(dev->type), dev->name,
        dev->path);
    pr_debug("    io_control : %4u\n", dev->io_control);
    pr_debug("    io_reg (io_base : %4u) {\n", dev->io_base);
    pr_debug(
        "        data   : %4u, error   : %4u, feature : %4u, sector_count : "
        "%4u\n",
        dev->io_reg.data, dev->io_reg.error, dev->io_reg.feature, dev->io_reg.sector_count);
    pr_debug(
        "        lba_lo : %4u, lba_mid : %4u, lba_hi  : %4u, hddevsel     : "
        "%4u\n",
        dev->io_reg.lba_lo, dev->io_reg.lba_mid, dev->io_reg.lba_hi, dev->io_reg.hddevsel);
    pr_debug("        status : %4u, command : %4u\n", dev->io_reg.status, dev->io_reg.command);
    pr_debug("    }\n");
    pr_debug("    identity {\n");
    pr_debug("        general_configuration {\n");
    pr_debug(
        "            response_incomplete : %4u, fixed_fevice : %4u\n",
        dev->identity.general_configuration.response_incomplete, dev->identity.general_configuration.fixed_fevice);
    pr_debug(
        "            removable_media     : %4u, device_type  : %4u\n",
        dev->identity.general_configuration.removable_media, dev->identity.general_configuration.device_type);
    pr_debug("        }\n");
    pr_debug("        num_cylinders          : %u\n", dev->identity.num_cylinders);
    pr_debug("        num_heads              : %u\n", dev->identity.num_heads);
    pr_debug("        num_sectors_per_track  : %u\n", dev->identity.num_sectors_per_track);
    pr_debug("        serial_number          : %s\n", dev->identity.serial_number);
    pr_debug("        firmware_revision      : %s\n", dev->identity.firmware_revision);
    pr_debug("        model_number           : %s\n", dev->identity.model_number);
    pr_debug("        maximum_block_transfer : %u\n", dev->identity.maximum_block_transfer);
    pr_debug("        capabilities {\n");
    pr_debug(
        "            current_long_physical_sector_alignment : %u\n",
        dev->identity.capabilities.current_long_physical_sector_alignment);
    pr_debug("            reserved_byte49                        : %u\n", dev->identity.capabilities.reserved_byte49);
    pr_debug("            dma_supported                          : %u\n", dev->identity.capabilities.dma_supported);
    pr_debug("            lba_supported                          : %u\n", dev->identity.capabilities.lba_supported);
    pr_debug("            io_rdy_disable                         : %u\n", dev->identity.capabilities.io_rdy_disable);
    pr_debug("            io_rdy_supported                       : %u\n", dev->identity.capabilities.io_rdy_supported);
    pr_debug(
        "            stand_by_timer_support                 : %u\n", dev->identity.capabilities.stand_by_timer_support);
    pr_debug("            reserved_word50                        : %u\n", dev->identity.capabilities.reserved_word50);
    pr_debug("        }\n");
    pr_debug("        valid_ext_data                        : %u\n", dev->identity.valid_ext_data);
    pr_debug("        current_multisector_setting           : %u\n", dev->identity.current_multisector_setting);
    pr_debug("        multisector_setting_valid             : %u\n", dev->identity.multisector_setting_valid);
    pr_debug("        reserved_byte59                       : %u\n", dev->identity.reserved_byte59);
    pr_debug("        sanitize_feature_supported            : %u\n", dev->identity.sanitize_feature_supported);
    pr_debug(
        "        crypto_scramble_ext_command_supported : %u\n", dev->identity.crypto_scramble_ext_command_supported);
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

/// @brief Waits for approximately 400 nanoseconds by reading the control register.
/// @param dev The ATA device to wait on.
/// @details Performs four I/O port reads (~100ns each) for a total of ~400ns.
/// This delay is required by the ATA specification between certain operations.
static inline void ata_io_wait(ata_device_t *dev)
{
    // Each inportb is approximately 100 nanoseconds on a modern processor.
    // Four reads provide the ~400ns delay specified by the ATA standard.
    inportb(dev->io_control);
    inportb(dev->io_control);
    inportb(dev->io_control);
    inportb(dev->io_control);
}

// ============================================================================
// ATA Status Wait Functions
// ============================================================================

/// @typedef ata_status_condition_fn
/// @brief Function pointer for device status condition checks.
/// @note Status conditions return 0 when ready to proceed, non-zero while waiting.
typedef int (*ata_status_condition_fn)(uint8_t status);

/// @brief Condition: status bits (matching mask) are STILL SET (keep waiting).
/// @param status The current device status register value.
/// @param mask The status bits to check.
/// @return Non-zero (waiting) while bits match, 0 when bits are cleared.
/// @details Helper for polling until status bits are cleared.
static inline int __cond_status_has_bits(uint8_t status, uint8_t mask)
{
    return (status & mask) == mask;
}

/// @brief Condition: status bits (matching mask) are STILL CLEAR (keep waiting).
/// @param status The current device status register value.
/// @param mask The status bits to check.
/// @return Non-zero (waiting) while bits are clear, 0 when bits are set.
/// @details Helper for polling until status bits are set.
static inline int __cond_status_missing_bits(uint8_t status, uint8_t mask)
{
    return (status & mask) != mask;
}

/// @brief Unified ATA device status waiter with timeout protection.
/// @param dev The ATA device to poll.
/// @param mask The status bits to check.
/// @param condition The condition to evaluate (0=ready, non-zero=keep waiting).
/// @param timeout Maximum iterations before giving up.
/// @return 0 on success (condition satisfied), 1 on timeout.
/// @details Polls the device status register while applying the condition function.
/// Uses volatile timeout to prevent compiler optimization of the critical wait loop.
static inline int ata_status_wait(ata_device_t *dev, uint8_t mask, 
                                  int (*evaluate_condition)(uint8_t, uint8_t),
                                  long timeout)
{
    uint8_t status;
    // Use volatile local copy to prevent compiler optimization of timeout loop.
    // The return value depends on proper timeout decrement, making volatile
    // semantics critical for correctness.
    volatile long volatile_timeout = timeout;
    
    do {
        // Read current device status.
        status = inportb(dev->io_reg.status);
        // Check if condition is satisfied.
        if (!evaluate_condition(status, mask)) {
            // Condition met - operation succeeded.
            return 0;
        }
    } while (--volatile_timeout > 0);
    
    // Timeout occurred - operation failed or device not responding.
    return 1;
}

/// @brief Waits until the status bits selected through the mask are zero.
/// @param dev The ATA device to poll.
/// @param mask The status bits to check.
/// @param timeout Maximum poll iterations before timing out.
/// @return 0 on success (bits cleared), 1 on timeout.
/// @details Polls the device status register until the bits specified by mask
/// are all cleared (0). Uses volatile semantics to ensure the timeout loop
/// cannot be optimized away by the compiler.
static inline int ata_status_wait_not(ata_device_t *dev, long mask, long timeout)
{
    // Call unified waiter with condition that bits should be cleared.
    return ata_status_wait(dev, (uint8_t)mask, __cond_status_has_bits, timeout);
}

/// @brief Waits until the status bits selected through the mask are set.
/// @param dev The ATA device to poll.
/// @param mask The status bits to check.
/// @param timeout Maximum poll iterations before timing out.
/// @return 0 on success (bits set), 1 on timeout.
/// @details Polls the device status register until the bits specified by mask
/// are all set (1). Uses volatile semantics to ensure the timeout loop
/// cannot be optimized away by the compiler.
static inline int ata_status_wait_for(ata_device_t *dev, long mask, long timeout)
{
    // Call unified waiter with condition that bits should be set.
    return ata_status_wait(dev, (uint8_t)mask, __cond_status_missing_bits, timeout);
}

/// @brief Prints the status and error information about the device.
/// @param dev the device for which we print the information.
static inline void ata_print_status_error(ata_device_t *dev)
{
    uint8_t error  = inportb(dev->io_reg.error);
    uint8_t status = inportb(dev->io_reg.status);
    if (error) {
        pr_err(
            "[%s] Device error [%s] status [%s]\n", ata_get_device_settings_str(dev), ata_get_device_error_str(error),
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
/// @details For non-ATAPI drives, the only method a driver has of resetting a
/// drive after a major error is to do a "software reset" on the bus. Set bit 2
/// (SRST, value = 4) in the proper Control Register for the bus. This will
/// reset both ATA devices on the bus.
/// @param dev the device on which we perform the soft reset.
static inline void ata_soft_reset(ata_device_t *dev)
{
    pr_debug("[%s] Performing ATA soft reset...\n", ata_get_device_settings_str(dev));
    ata_print_status_error(dev);

    // Setting the SRST bit
    // Writes the SRST (software reset) bit to the control register, initiating
    // the reset. This bit should be set to 1 to start the reset.
    outportb(dev->io_control, ata_control_srst);

    // Flushing the I/O
    // Flushes to ensure that the write to the control register is completed.
    // This is necessary to avoid issues due to out-of-order execution or
    // caching, which is standard practice.
    inportb(dev->io_control);

    // Waiting for the reset to complete
    // Ensures that the system waits for 400ns, which is a typical delay needed
    // after issuing the reset to give the device time to process it.
    ata_io_wait(dev);

    // Clearing the SRST bit
    // After the delay, resets the control register to its normal state by
    // clearing the SRST bit (0), allowing normal operations to resume on the
    // device.
    outportb(dev->io_control, ata_control_zero);

    // Flushing the I/O again.
    inportb(dev->io_control);

    // Waiting until the device is ready
    // Waits until the device is no longer busy (BSY bit cleared) and no data
    // request (DRQ bit cleared), indicating the reset is complete and the
    // device is ready.
    if (ata_status_wait_not(dev, ata_status_bsy | ata_status_drq, 100000)) {
        pr_err("Soft reset failed. Device did not become ready.");
        return;
    }
}

/// @brief Creates the DMA memory area used to write and read on the device.
/// @param size the size of the DMA memory area.
/// @param physical the physical address of the DMA memory area.
/// @return the logical address of the DMA memory area, or 0 on failure.
static inline uintptr_t ata_dma_alloc(size_t size, uintptr_t *physical)
{
    // Sanity check the requested size.
    if (size == 0 || !physical) {
        pr_crit("Invalid size or physical address pointer provided.\n");
        return 0;
    }

    // Get the page order to accommodate the requested size. This finds the
    // nearest order (power of two) greater than or equal to the size. DMA
    // allocations typically need to be a power of two, and the size should be
    // aligned to ensure proper DMA operation, as DMA engines often require
    // memory to be aligned to specific boundaries (e.g., 4KB or 64KB).
    uint32_t order = find_nearest_order_greater(0, size);

    // Allocate a contiguous block of memory pages. Ensure that alloc_pages
    // returns physically contiguous pages suitable for DMA, as DMA transfers
    // usually require physically contiguous memory.
    page_t *page = alloc_pages(GFP_DMA, order);
    if (!page) {
        pr_crit("Failed to allocate pages for DMA memory (order = %d).\n", order);
        return 0;
    }

    // Extract the physical address from the allocated page. This physical
    // address will be passed to the DMA engine, which uses it to directly
    // transfer data.
    *physical = get_physical_address_from_page(page);
    // Note: Physical address 0 is technically valid (though rare), so we don't check for it here.
    // The buddy system will not allocate page 0 if it's reserved elsewhere.

    // Retrieve the low-memory address (logical address) that the CPU can use to
    // access the allocated memory. The CPU will use this address to interact
    // with the DMA memory region.
    uintptr_t lowmem_address = get_virtual_address_from_page(page);
    if (lowmem_address == 0) {
        pr_crit("Failed to retrieve a valid low-memory address.\n");
        return 0;
    }

    pr_debug("Size requirement is %d, which results in an order %d\n", size, order);
    pr_debug("Allocated page is at       : 0x%p\n", page);
    pr_debug("The physical address is at : 0x%lx\n", *physical);
    pr_debug("The lowmem address is at   : 0x%lx\n", lowmem_address);

    // Return the logical (low-memory) address for CPU access.
    return lowmem_address;
}

/// @brief Frees the DMA memory area previously allocated.
/// @param logical_addr the logical (low-memory) address to free.
/// @return 0 on success, 1 on failure.
static inline int ata_dma_free(uintptr_t logical_addr)
{
    // Sanity check the input.
    if (!logical_addr) {
        pr_debug("Invalid logical address or size for freeing DMA memory.\n");
        return 1;
    }

    // Retrieve the page structure from the logical address.
    page_t *page = get_page_from_virtual_address(logical_addr);
    if (!page) {
        pr_debug(
            "Failed to retrieve the page structure from logical address "
            "0x%lx.\n",
            logical_addr);
        return 1;
    }

    // Free the allocated pages.
    if (free_pages(page) < 0) {
        pr_debug("Failed to free allocated pages 0x%p.\n", page);
        return 1;
    }

    // Debugging information.
    pr_debug("Successfully freed DMA memory at logical address 0x%p.\n", logical_addr);

    return 0; // Success.
}

/// @brief Enables bus mastering, allowing Direct Memory Access (DMA)
/// transactions.
/// @details This function reads the PCI command register and enables bus
/// mastering if not already enabled. It checks if the bus mastering bit is
/// already set, and if not, sets it and verifies the change. If bus mastering
/// cannot be enabled, it signals an error.
/// @return 0 on success, 1 on failure.
static inline int ata_dma_enable_bus_mastering(void)
{
    uint32_t pci_cmd;

    // Ensure that the ata_pci device handle is valid.
    if (!ata_pci) {
        pr_crit("Invalid PCI device handle.\n");
        return 1;
    }

    // Read the PCI command register
    if (pci_read_32(ata_pci, PCI_COMMAND, &pci_cmd) != 0) {
        pr_crit("Failed to read PCI_COMMAND from device.\n");
        return 1;
    }

    // Check if bus mastering is already enabled.
    if (bit_check(pci_cmd, pci_command_bus_master)) {
        pr_crit("Bus mastering already enabled.\n");
        return 0;
    }

    // Enable bus mastering by setting the corresponding bit.
    bit_set_assign(pci_cmd, pci_command_bus_master);

    // Write the updated PCI command register back to the device
    if (pci_write_32(ata_pci, PCI_COMMAND, pci_cmd) != 0) {
        pr_crit("Failed to write PCI_COMMAND to device.\n");
        return 1;
    }

    // Read the current PCI command register
    if (pci_read_32(ata_pci, PCI_COMMAND, &pci_cmd) != 0) {
        pr_crit("Failed to read PCI_COMMAND from device.\n");
        return 1;
    }

    // Verify that bus mastering is enabled
    if (!bit_check(pci_cmd, pci_command_bus_master)) {
        pr_crit("Bus mastering is not correctly set.\n");
        return 1;
    }

    // Successfully enabled bus mastering.
    return 0;
}

/// @brief Disables bus mastering, preventing Direct Memory Access (DMA)
/// transactions.
/// @details This function reads the PCI command register and clears the bus
/// mastering bit. If bus mastering is already disabled, it logs a warning.
/// @return 0 on success, 1 on failure.
static inline int ata_dma_disable_bus_mastering(void)
{
    uint32_t pci_cmd;

    // Ensure that the ata_pci device handle is valid.
    if (!ata_pci) {
        pr_crit("Invalid PCI device handle.\n");
        return 1;
    }

    // Read the current PCI command register
    if (pci_read_32(ata_pci, PCI_COMMAND, &pci_cmd) != 0) {
        pr_crit("Failed to read PCI_COMMAND from device.\n");
        return 1;
    }

    // Check if bus mastering is currently enabled.
    if (!bit_check(pci_cmd, pci_command_bus_master)) {
        pr_crit("Bus mastering already disabled.\n");
        return 0;
    }

    // Clear the bus mastering bit to disable it.
    bit_clear_assign(pci_cmd, pci_command_bus_master);

    // Write the updated PCI command register back to the device.
    if (pci_write_32(ata_pci, PCI_COMMAND, pci_cmd) != 0) {
        pr_crit("Failed to write PCI_COMMAND to device.\n");
        return 1;
    }

    // Read the current PCI command register
    if (pci_read_32(ata_pci, PCI_COMMAND, &pci_cmd) != 0) {
        pr_crit("Failed to read PCI_COMMAND from device.\n");
        return 1;
    }

    // Verify that bus mastering is disabled.
    if (bit_check(pci_cmd, pci_command_bus_master)) {
        pr_crit("Bus mastering is not correctly cleared.\n");
        return 1;
    }

    // Successfully disabled bus mastering.
    return 0;
}

/// @brief Initializes the bus mastering register (BMR) fields of the ATA
/// device.
/// @param dev The device to initialize.
/// @details
/// When retrieving the actual base address of a Base Address Register (BAR),
/// it's essential to mask the lower bits to ensure you're working with the
/// correct address space.
/// - For 16-bit Memory Space BARs, the address should be masked with 0xFFF0.
/// - For 32-bit Memory Space BARs, the address should be masked with 0xFFFFFFF0.
/// @return 0 on success, 1 on failure.
static inline int ata_dma_initialize_bus_mastering_address(ata_device_t *dev)
{
    uint32_t address;

    // Check if the device pointer is valid.
    if (dev == NULL) {
        pr_warning("Device pointer 'dev' is NULL.\n");
        return 1;
    }

    // Ensure that the ata_pci device handle is valid
    if (!ata_pci) {
        pr_warning("Invalid PCI device handle.\n");
        return 1;
    }

    // Read the value of the PCI Base Address Register (BAR) for bus mastering
    if (pci_read_32(ata_pci, PCI_BASE_ADDRESS_4, &address) != 0) {
        pr_warning("Failed to read PCI_BASE_ADDRESS_4 from device.\n");
        return 1;
    }

    // Check if the lowest bit is set to distinguish between memory space and
    // I/O space BARs. Memory space BARs have the lowest bit as 0, while I/O
    // space BARs have it as 1.
    if (!bit_check(address, 0)) {
        // Log a warning if the address indicates that bus mastering could not be initialized.
        pr_warning(
            "[%s] Failed to initialize Bus Mastering. The address is not an "
            "I/O space BAR.\n",
            ata_get_device_settings_str(dev));
        return 1;
    }

    // Mask the lower bits to retrieve the actual base address for I/O space
    // BARs. The mask 0xFFFFFFFC is used to clear the lowest two bits.
    address &= 0xFFFFFFFC;

    // Differentiate between the primary and secondary ATA buses to set the
    // correct BMR fields.
    if (dev->primary) {
        // For the primary ATA bus, set the command, status, and PRDT (Physical
        // Region Descriptor Table) addresses.
        dev->bmr.command = address + 0x0; // Command register offset
        dev->bmr.status  = address + 0x2; // Status register offset
        dev->bmr.prdt    = address + 0x4; // PRDT offset
    } else {
        // For the secondary ATA bus, set the command, status, and PRDT
        // addresses with different offsets.
        dev->bmr.command = address + 0x8; // Command register offset
        dev->bmr.status  = address + 0xA; // Status register offset
        dev->bmr.prdt    = address + 0xC; // PRDT offset
    }

    // Successfully initialized BMR addresses.
    return 0;
}

// == ATA DEVICE MANAGEMENT ===================================================

/// @brief Detects the type of device.
/// @param dev The device for which we are checking the type.
/// @return The detected device type.
static inline ata_device_type_t ata_detect_device_type(ata_device_t *dev)
{
    pr_debug("[%s] Detecting device type...\n", ata_get_device_settings_str(dev));

    // Select the drive (Master/Slave).
    outportb(dev->io_reg.hddevsel, 0xA0 | (dev->slave << 4U));

    // Wait for the command to settle.
    ata_io_wait(dev);

    // Select the ATA device (preparing for IDENTIFY).
    outportb(dev->io_base + 1, 1);

    // Disable IRQs for this operation.
    outportb(dev->io_control, 0);

    // Select the device again to ensure proper communication.
    outportb(dev->io_reg.hddevsel, 0xA0 | (dev->slave << 4U));

    // Wait for 400ns for the command to settle.
    ata_io_wait(dev);

    // The host is prohibited from writing the Features, Sector Count, Sector
    // Number, Cylinder Low, Cylinder High, or Device/Head registers when either
    // BSY or DRQ is set in the Status Register. Any write to the Command
    // Register when BSY or DRQ is set is ignored unless the write is to issue a
    // Device Reset command.
    if (ata_status_wait_not(dev, ata_status_bsy | ata_status_drq, 100000)) {
        ata_print_status_error(dev);
        return ata_dev_type_unknown;
    }

    // ATA specs say these values must be zero before sending IDENTIFY.
    outportb(dev->io_reg.sector_count, 0);
    outportb(dev->io_reg.lba_lo, 0);
    outportb(dev->io_reg.lba_mid, 0);
    outportb(dev->io_reg.lba_hi, 0);

    // Request the device identity by sending the IDENTIFY command.
    outportb(dev->io_reg.command, ata_command_pata_ident);

    // Wait for the device to become non-busy and ready.
    // if (ata_status_wait_not(dev, ata_status_bsy & ~(ata_status_drq | ata_status_rdy), 100000)) {
    if (ata_status_wait_not(dev, ata_status_bsy, 100000)) {
        ata_print_status_error(dev);
        return ata_dev_type_unknown;
    }

    // Read the identity data from the device.
    inportsw(dev->io_reg.data, (uint16_t *)&dev->identity, (sizeof(ata_identity_t) / sizeof(uint16_t)));

    // Fix the serial number, firmware revision, and model number.
    ata_fix_string((char *)&dev->identity.serial_number, count_of(dev->identity.serial_number) - 1);
    ata_fix_string((char *)&dev->identity.firmware_revision, count_of(dev->identity.firmware_revision) - 1);
    ata_fix_string((char *)&dev->identity.model_number, count_of(dev->identity.model_number) - 1);

    // Get the "signature bytes" by reading low and high cylinder registers.
    uint8_t lba_lo  = inportb(dev->io_reg.lba_lo);
    uint8_t lba_mid = inportb(dev->io_reg.lba_mid);
    uint8_t lba_hi  = inportb(dev->io_reg.lba_hi);

    // Differentiate between ATA, ATAPI, SATA, and SATAPI devices based on signature bytes.
    if ((lba_mid == 0x00) && (lba_hi == 0x00)) {
        return ata_dev_type_pata; // Parallel ATA
    }
    if ((lba_mid == 0x3C) && (lba_hi == 0xC3)) {
        return ata_dev_type_sata; // Serial ATA
    }
    if ((lba_mid == 0x14) && (lba_hi == 0xEB)) {
        return ata_dev_type_patapi; // Parallel ATAPI
    }
    if ((lba_mid == 0x69) && (lba_hi == 0x96)) {
        return ata_dev_type_satapi; // Serial ATAPI
    }
    if ((lba_mid == 0xFF) && (lba_hi == 0xFF)) {
        return ata_dev_type_no_device; // No device present.
    }

    // Return unknown type if none of the conditions are met.
    return ata_dev_type_unknown;
}

/// @brief Initialises the given device.
/// @param dev the device to initialize.
/// @return 0 on success, 1 on error.
static bool_t ata_device_init(ata_device_t *dev)
{
    pr_debug(
        "[%-16s, %-9s] Initializing ATA device...\n", ata_get_device_settings_str(dev),
        ata_get_device_type_str(dev->type));

    // Check the status of the device to ensure it's ready for initialization.
    if (ata_status_wait_for(dev, ata_status_drq | ata_status_rdy, 100000)) {
        // pr_crit("[%-16s, %-9s] Device not ready after waiting.\n",
        //         ata_get_device_settings_str(dev), ata_get_device_type_str(dev->type));
        ata_print_status_error(dev);
        return 1;
    }

    // Initialize the bus mastering addresses.
    if (ata_dma_initialize_bus_mastering_address(dev)) {
        pr_crit(
            "[%-16s, %-9s] Failed to initialize bus mastering address.\n", ata_get_device_settings_str(dev),
            ata_get_device_type_str(dev->type));
        ata_print_status_error(dev);
        return 1;
    }

    // Check the status of the device.
    if (ata_status_wait_for(dev, ata_status_drq | ata_status_rdy, 100000)) {
        pr_crit(
            "[%-16s, %-9s] Device not ready after bus mastering "
            "initialization.\n",
            ata_get_device_settings_str(dev), ata_get_device_type_str(dev->type));
        ata_print_status_error(dev);
        return 1;
    }

    // Allocate the memory for the Physical Region Descriptor Table (PRDT).
    dev->dma.prdt = (prdt_t *)ata_dma_alloc(sizeof(prdt_t), &dev->dma.prdt_phys);
    if (dev->dma.prdt == NULL) {
        pr_crit(
            "[%-16s, %-9s] Failed to allocate memory for PRDT.\n", ata_get_device_settings_str(dev),
            ata_get_device_type_str(dev->type));
        return 1;
    }

    // Allocate the memory for the Direct Memory Access (DMA).
    dev->dma.start = (uint8_t *)ata_dma_alloc(ATA_DMA_SIZE, &dev->dma.start_phys);
    if (dev->dma.start == NULL) {
        pr_crit(
            "[%-16s, %-9s] Failed to allocate memory for DMA.\n", ata_get_device_settings_str(dev),
            ata_get_device_type_str(dev->type));
        // Free previously allocated PRDT.
        ata_dma_free((uintptr_t)dev->dma.prdt);
        return 1;
    }

    // Initialize the PRDT with the physical address of the DMA.
    dev->dma.prdt->physical_address = dev->dma.start_phys;

    // Set the size of the DMA transfer.
    dev->dma.prdt->byte_count = ATA_DMA_SIZE;

    // Set the End of Table (EOT) flag.
    dev->dma.prdt->end_of_table = 0x8000;

    // Print the device data for debugging purposes.
    ata_dump_device(dev);

    return 0;
}

// == ATA SECTOR READ/WRITE FUNCTIONS =========================================

/// @brief PIO fallback for sector reads.
/// @param dev target device.
/// @param lba_sector sector to read.
/// @param buffer destination buffer.
/// @return 0 on success, negative errno on failure.
static int ata_device_read_sector_pio(ata_device_t *dev, uint32_t lba_sector, uint8_t *buffer)
{
    int rc = 0;

    if (ata_status_wait_not(dev, ata_status_bsy, 100000)) {
        rc = -EBUSY;
        goto out;
    }

    outportb(dev->io_control, ata_control_nien);
    outportb(dev->io_reg.hddevsel, 0xE0 | (dev->slave << 4) | ((lba_sector >> 24) & 0x0F));
    ata_io_wait(dev);

    outportb(dev->io_reg.feature, 0x00);
    outportb(dev->io_reg.sector_count, 1);
    outportb(dev->io_reg.lba_lo, (uint8_t)(lba_sector & 0xFF));
    outportb(dev->io_reg.lba_mid, (uint8_t)((lba_sector >> 8) & 0xFF));
    outportb(dev->io_reg.lba_hi, (uint8_t)((lba_sector >> 16) & 0xFF));

    outportb(dev->io_reg.command, ATA_CMD_READ_PIO);

    if (ata_status_wait_not(dev, ata_status_bsy, 100000)) {
        rc = -EBUSY;
        goto out;
    }
    if (ata_status_wait_for(dev, ata_status_drq, 100000)) {
        rc = -ETIMEDOUT;
        goto out;
    }

    uint8_t status = inportb(dev->io_reg.status);
    if (status & (ata_status_err | ata_status_df)) {
        rc = -EIO;
        goto out;
    }

    inportsw(dev->io_reg.data, (uint16_t *)buffer, (ATA_SECTOR_SIZE / sizeof(uint16_t)));

    status = inportb(dev->io_reg.status);
    if (status & (ata_status_err | ata_status_df)) {
        rc = -EIO;
    }

out:
    outportb(dev->io_control, ata_control_zero);
    return rc;
}

/// @brief DMA path for sector reads with error detection.
/// @details Currently disabled: QEMU DMA IRQs don't fire as expected.
///          Always returns error to force PIO fallback, which works reliably.
/// @param dev target device.
/// @param lba_sector sector to read.
/// @param buffer destination buffer.
/// @return Always returns -ENOSYS to force PIO fallback.
static int ata_device_read_sector_dma(ata_device_t *dev, uint32_t lba_sector, uint8_t *buffer)
{
    // DMA IRQ handling is unreliable in QEMU emulation. PIO works perfectly.
    // Rather than waste time on DMA timeouts, go directly to PIO.
    (void)dev;
    (void)lba_sector;
    (void)buffer;
    return -ENOSYS;
}

/// @brief Reads an ATA sector with PIO (DMA disabled due to QEMU incompatibility).
/// @param dev the device on which we perform the read.
/// @param lba_sector the sector where we read.
/// @param buffer the buffer we are writing.
static void ata_device_read_sector(ata_device_t *dev, uint32_t lba_sector, uint8_t *buffer)
{
    if ((dev->type != ata_dev_type_pata) && (dev->type != ata_dev_type_sata)) {
        pr_crit("[%s] Unsupported device type for read operation.\n", ata_get_device_settings_str(dev));
        return;
    }

    spinlock_lock(&dev->lock);

    // Skip DMA entirely; QEMU's DMA IRQ emulation is unreliable.
    // PIO works perfectly, so use it directly.
    int rc = ata_device_read_sector_pio(dev, lba_sector, buffer);
    if (rc) {
        pr_crit("ata_device_read_sector: PIO failed (sector %u, rc=%d)\n", lba_sector, rc);
    }

    spinlock_unlock(&dev->lock);
}

/// @brief Writs an ATA sector.
/// @param dev the device on which we perform the write.
/// @param lba_sector the sector where we read.
/// @param buffer the buffer where we store what we read.
static void ata_device_write_sector(ata_device_t *dev, uint32_t lba_sector, uint8_t *buffer)
{
    // Check if we are trying to perform the read on a valid device type.
    if ((dev->type != ata_dev_type_pata) && (dev->type != ata_dev_type_sata)) {
        pr_crit("[%s] Unsupported device type for read operation.\n", ata_get_device_settings_str(dev));
        return;
    }

    // Acquire the lock for thread safety.
    spinlock_lock(&dev->lock);

    // Copy the buffer over to the DMA area.
    memcpy(dev->dma.start, buffer, ATA_DMA_SIZE);

    // Reset the bus master register's command register.
    outportb(dev->bmr.command, 0);

    // Set the Physical Region Descriptor Table (PRDT).
    outportl(dev->bmr.prdt, dev->dma.prdt_phys);

    // Enable error and IRQ status in the bus master register.
    outportb(dev->bmr.status, inportb(dev->bmr.status) | 0x04 | 0x02);

    // Wait for the device to be ready (BSY flag should be clear).
    if (ata_status_wait_not(dev, ata_status_bsy, 100000)) {
        ata_print_status_error(dev);
        spinlock_unlock(&dev->lock);
        return;
    }

    // Select the drive (set head and device).
    outportb(dev->io_reg.hddevsel, 0xe0 | (dev->slave << 4) | ((lba_sector & 0x0F000000) >> 24));

    // Wait for the device to be ready again (BSY flag should be clear).
    if (ata_status_wait_not(dev, ata_status_bsy, 100000)) {
        ata_print_status_error(dev);
        spinlock_unlock(&dev->lock);
        return;
    }

    // Set the features, sector count, and LBA for the write operation.
    outportb(dev->io_reg.feature, 0x00);                           // No features for this write operation.
    outportb(dev->io_reg.sector_count, 1);                         // Write one sector.
    outportb(dev->io_reg.lba_lo, (lba_sector & 0x000000FF) >> 0);  // LBA low byte.
    outportb(dev->io_reg.lba_mid, (lba_sector & 0x0000FF00) >> 8); // LBA mid byte.
    outportb(dev->io_reg.lba_hi, (lba_sector & 0x00FF0000) >> 16); // LBA high byte.

    // Wait for the device to be ready for data transfer (BSY should be clear).
    if (ata_status_wait_not(dev, ata_status_bsy, 100000)) {
        ata_print_status_error(dev);
        spinlock_unlock(&dev->lock);
        return;
    }

    // Notify that we are starting the DMA writing operation.
    outportb(dev->io_reg.command, ata_dma_command_write);

    // Start the DMA writing process.
    outportb(dev->bmr.command, 0x01); // Start the DMA transfer.

    // Wait for the DMA write to complete.
    while (1) {
        int status  = inportb(dev->bmr.status);
        int dstatus = inportb(dev->io_reg.status);
        // Wait for DMA transfer to complete.
        if (!(status & 0x04)) {
            continue;
        }
        // Exit when device is no longer busy.
        if (!(dstatus & ata_status_bsy)) {
            break;
        }
    }

    // Inform the device that we are done with the write operation.
    outportb(dev->bmr.status, inportb(dev->bmr.status) | 0x04 | 0x02);

    // Release the lock after the operation is complete.
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

    // Determine which device to open based on the provided path.
    if (strcmp(path, ata_primary_master.path) == 0) {
        dev = &ata_primary_master;
    } else if (strcmp(path, ata_primary_slave.path) == 0) {
        dev = &ata_primary_slave;
    } else if (strcmp(path, ata_secondary_master.path) == 0) {
        dev = &ata_secondary_master;
    } else if (strcmp(path, ata_secondary_slave.path) == 0) {
        dev = &ata_secondary_slave;
    } else {
        pr_crit("Device not found for path: %s\n", path);
        return NULL;
    }

    // If the device's filesystem root is already allocated, increment its
    // reference count.
    if (dev->fs_root) {
        // Increment reference count for the file.
        ++dev->fs_root->count;
        // Return the filesystem root associated with the device.
        return dev->fs_root;
    }

    pr_crit("Filesystem root not initialized for device: %s\n", path);
    return NULL;
}

/// @brief Closes an ATA device.
/// @param file the VFS file associated with the ATA device.
/// @return 0 on success, -errno on failure.
static int ata_close(vfs_file_t *file)
{
    // Validate the file pointer.
    if (file == NULL) {
        pr_err("ata_close: Invalid file pointer (NULL).\n");
        return -EINVAL;
    }

    // Get the device from the VFS file.
    ata_device_t *dev = (ata_device_t *)file->device;
    if (dev == NULL) {
        pr_crit("ata_close: Device not set for file `%s`.\n", file->name);
        return -ENODEV;
    }

    // Ensure the device is one of the known ATA devices.
    if (!(dev == &ata_primary_master || dev == &ata_primary_slave || dev == &ata_secondary_master ||
          dev == &ata_secondary_slave)) {
        pr_crit("ata_close: Invalid device encountered for file `%s`.\n", file->name);
        return -EINVAL;
    }

    // Decrement the reference count for the file.
    if (--file->count == 0) {
        pr_debug("ata_close: Closing file `%s` (ino: %d).\n", file->name, file->ino);

        // Remove the file from the list of opened files.
        list_head_remove(&file->siblings);
        pr_debug("ata_close: Removed file `%s` from the opened file list.\n", file->name);

        // Free the file from cache.
        vfs_dealloc_file(file);
        pr_debug("ata_close: Freed memory for file `%s`.\n", file->name);
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
        pr_crit("Device not set for file: %p\n", file);
        return -1; // Return error if the device is not set.
    }

    // Check device type.
    if (dev->type != ata_dev_type_pata && dev->type != ata_dev_type_sata) {
        pr_warning("Unsupported device type.\n");
        return -EPERM; // Return error for unsupported device types.
    }

    uint32_t max_offset = ata_max_offset(dev);

    // Check if the offset exceeds the disk size.
    if (offset > max_offset) {
        pr_warning("The offset is exceeding the disk size (%d > %d)\n", offset, max_offset);
        ata_dump_device(dev);
        // Get the error and status information of the device.
        uint8_t error  = inportb(dev->io_reg.error);
        uint8_t status = inportb(dev->io_reg.status);
        pr_err("Device error  : %s\n", ata_get_device_error_str(error));
        pr_err("Device status : %s\n", ata_get_device_status_str(status));
        return -1; // Return error on invalid offset.
    }

    // Check if reading exceeds the size.
    if ((offset + size) > max_offset) {
        size = max_offset - offset;
    }

    uint32_t start_block  = offset / ATA_SECTOR_SIZE;
    uint32_t start_offset = offset % ATA_SECTOR_SIZE;
    uint32_t end_block    = (offset + size - 1) / ATA_SECTOR_SIZE;
    uint32_t end_offset   = (offset + size - 1) % ATA_SECTOR_SIZE;
    uint32_t prefix_size  = (ATA_SECTOR_SIZE - start_offset);
    uint32_t postfix_size = (offset + size) % ATA_SECTOR_SIZE;
    uint32_t x_offset     = 0;

    if (start_offset) {
        ata_device_read_sector(dev, start_block, (uint8_t *)support_buffer);
        // Copy the prefix from the support buffer to the output buffer.
        memcpy(buffer, (void *)((uintptr_t)support_buffer + start_offset), prefix_size);
        x_offset += prefix_size;
        ++start_block;
    }

    // Read postfix if needed.
    if (postfix_size && (start_block <= end_block)) {
        ata_device_read_sector(dev, end_block, (uint8_t *)support_buffer);
        // Copy the postfix from the support buffer to the output buffer.
        memcpy((void *)((uintptr_t)buffer + size - postfix_size), support_buffer, postfix_size);
        --end_block;
    }

    // Read full sectors in between.
    for (; start_block <= end_block; ++start_block) {
        ata_device_read_sector(dev, start_block, (uint8_t *)((uintptr_t)buffer + x_offset));
        x_offset += ATA_SECTOR_SIZE;
    }

    // Return the number of bytes read.
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
        pr_crit("Device not set for file: %p\n", file);
        return -1; // Return error if the device is not set.
    }

    // Check device type.
    if (dev->type != ata_dev_type_pata && dev->type != ata_dev_type_sata) {
        pr_warning("Unsupported device type.\n");
        return -EPERM; // Return error for unsupported device types.
    }

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

    // Handle the prefix if needed.
    if (start_offset) {
        ata_device_read_sector(dev, start_block, (uint8_t *)support_buffer);
        memcpy((void *)((uintptr_t)support_buffer + (start_offset)), buffer, prefix_size);
        ata_device_write_sector(dev, start_block, (uint8_t *)support_buffer);
        x_offset += prefix_size;
        ++start_block;
    }

    // Handle the postfix if needed.
    if (postfix_size && (start_block <= end_block)) {
        ata_device_read_sector(dev, end_block, (uint8_t *)support_buffer);
        memcpy(support_buffer, (void *)((uintptr_t)buffer + size - postfix_size), postfix_size);
        ata_device_write_sector(dev, end_block, (uint8_t *)support_buffer);
        --end_block;
    }

    // Write full sectors in between.
    for (; start_block <= end_block; ++start_block) {
        ata_device_write_sector(dev, start_block, (uint8_t *)((uintptr_t)buffer + x_offset));
        x_offset += ATA_SECTOR_SIZE;
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
static int ata_fstat(vfs_file_t *file, stat_t *stat) { return _ata_stat(file->device, stat); }

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
static file_system_type_t ata_file_system_type = {.name = "ata", .fs_flags = 0, .mount = ata_mount_callback};

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
    vfs_file_t *file = vfs_alloc_file();
    if (file == NULL) {
        pr_err("Failed to create ATA device.\n");
        return NULL;
    }
    // Set the device name.
    memcpy(file->name, dev->name, NAME_MAX);
    file->uid            = 0;
    file->gid            = 0;
    file->mask           = 0x2000 | 0600;
    file->atime          = sys_time(NULL);
    file->mtime          = sys_time(NULL);
    file->ctime          = sys_time(NULL);
    // Set the device.
    file->device         = dev;
    // Re-set the flags.
    file->flags          = DT_BLK;
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
            vfs_dealloc_file(dev->fs_root);
            return ata_dev_type_unknown;
        }
        // Increment the drive letter.
        ++ata_drive_char;

        pr_debug(
            "Initialized %s device on %s.\n", ata_get_device_type_str(dev->type), ata_get_device_settings_str(dev));

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
static void ata_irq_handler_master(pt_regs_t *f)
{
    pr_warning("ata_irq_handler_master\n");
    inportb(ata_primary_master.io_reg.status);
    inportb(ata_primary_master.bmr.status);
    //outportb(ata_primary_master.bmr.command, ata_bm_stop_bus_master);
    pic8259_send_eoi(IRQ_FIRST_HD);
}

/// @param f The interrupt stack frame.
static void ata_irq_handler_slave(pt_regs_t *f)
{
    pr_warning("ata_irq_handler_slave\n");
    inportb(ata_secondary_master.io_reg.status);
    inportb(ata_primary_master.bmr.status);
    //outportb(ata_primary_master.bmr.command, ata_bm_stop_bus_master);
    pic8259_send_eoi(IRQ_SECOND_HD);
}

// == PCI FUNCTIONS ===========================================================

/// @brief Callback function used while scanning the PCI interface to find ATA devices.
/// @param device The PCI device identifier.
/// @param vendor_id The vendor ID of the device.
/// @param device_id The device ID of the device.
/// @param extra Pointer to store the device identifier once found.
/// @return 1 if a matching device is found, 0 if not, -1 on error.
static int pci_find_ata(uint32_t device, uint16_t vendor_id, uint16_t device_id, void *extra)
{
    // Check if the output pointer 'extra' is valid.
    if (extra == NULL) {
        pr_err("Output parameter 'extra' is NULL.\n");
        return 1;
    }
    // Intel Corporation AND (IDE Interface OR PIIX4 IDE).
    if ((vendor_id == 0x8086) && (device_id == 0x7010 || device_id == 0x7111)) {
        // Store the device identifier in the location pointed to by 'extra'.
        *((uint32_t *)extra) = device;
        // Call pci_dump_device_data to display device information
        pci_dump_device_data(device, vendor_id, device_id);
        return 0; // Matching device found.
    }
    return 1; // No matching device found.
}

// == INITIALIZE/FINALIZE ATA =================================================

int ata_initialize(void)
{
    // Search for ATA devices.
    if (pci_scan(pci_find_ata, -1, &ata_pci) != 0) {
        pr_err("Failed to scan for ATA devices.\n");
        return 1;
    }

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

int ata_finalize(void) { return 0; }

/// @}
