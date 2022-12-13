/// @file ata.c
/// @brief Advanced Technology Attachment (ATA) and Advanced Technology Attachment Packet Interface (ATAPI) drivers.
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.
/// @addtogroup ata
/// @{

// Include the kernel log levels.
#include "sys/kernel_levels.h"
/// Change the header.
#define __DEBUG_HEADER__ "[ATA   ]"
/// Set the log level.
#define __DEBUG_LEVEL__ LOGLEVEL_NOTICE

#include "drivers/ata.h"

#include "descriptor_tables/isr.h"
#include "hardware/pic8259.h"
#include "klib/spinlock.h"
#include "process/wait.h"
#include "system/panic.h"
#include "devices/pci.h"
#include "io/port_io.h"
#include "sys/errno.h"
#include "mem/kheap.h"
#include "io/debug.h"
#include "string.h"
#include "fs/vfs.h"
#include "fcntl.h"
#include "stdio.h"

/// @brief ATA Error Bits
typedef enum {
    ata_err_amnf  = 0, ///< Address mark not found.
    ata_err_tkznf = 1, ///< Track zero not found.
    ata_err_abrt  = 2, ///< Aborted command.
    ata_err_mcr   = 3, ///< Media change request.
    ata_err_idnf  = 4, ///< ID not found.
    ata_err_mc    = 5, ///< Media changed.
    ata_err_unc   = 6, ///< Uncorrectable data error.
    ata_err_bbk   = 7, ///< Bad Block detected.
} ata_error_t;

/// @brief ATA Status Bits
typedef enum {
    ata_status_err  = 0, ///< Indicates an error occurred.
    ata_status_idx  = 1, ///< Index. Always set to zero.
    ata_status_corr = 2, ///< Corrected data. Always set to zero.
    ata_status_drq  = 3, ///< Set when the drive has PIO data to transfer, or is ready to accept PIO data.
    ata_status_srv  = 4, ///< Overlapped Mode Service Request.
    ata_status_df   = 5, ///< Drive Fault Error (does not set ERR).
    ata_status_rdy  = 6, ///< Bit is clear when drive is spun down, or after an error. Set otherwise.
    ata_status_bsy  = 7, ///< The drive is preparing to send/receive data (wait for it to clear).
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

/// @brief IDENTIFY device data (response to 0xEC).
typedef struct ata_identify_t {
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
    /// Word   1-9  : Unused.
    uint16_t unused1[9];
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
} ata_identify_t;

/// @brief Physical Region Descriptor Table (PRDT) entry.
/// @details
/// The physical memory region to be transferred is described by a Physical
/// Region Descriptor (PRD). The data transfer will proceed until all regions
/// described by the PRDs in the table have been transferred.
/// Each Physical Region Descriptor entry is 8 bytes in length.
///         |    byte 3  |  byte 2  |  byte 1  |  byte 0    |
/// Dword 0 |  Memory Region Physical Base Address [31:1] |0|
/// Dword 1 |  EOT | reserved       | Byte Count [15:1]   |0|
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
    unsigned io_base;
    /// I/O registers.
    struct {
        /// [R/W] Data Register. Read/Write PIO data bytes (16-bit).
        unsigned data;
        /// [R  ] Error Register. Read error generated by the last ATA command executed (8-bit).
        unsigned error;
        /// [  W] Features Register. Used to control command specific interface features (8-bit).
        unsigned feature;
        /// [R/W] Sector Count Register. Number of sectors to read/write (0 is a special value) (8-bit).
        unsigned sector_count;
        /// [R/W] Sector Number Register. This is CHS/LBA28/LBA48 specific (8-bit).
        unsigned lba_lo;
        /// [R/W] Cylinder Low Register. Partial Disk Sector address (8-bit).
        unsigned lba_mid;
        /// [R/W] Cylinder High Register. Partial Disk Sector address (8-bit).
        unsigned lba_hi;
        /// [R/W] Drive / Head Register. Used to select a drive and/or head. Supports extra address/flag bits (8-bit).
        unsigned hddevsel;
        /// [R  ] Status Register. Used to read the current status (8-bit).
        unsigned status;
        /// [  W] Command Register. Used to send ATA commands to the device (8-bit).
        unsigned command;
    } io_reg;
    /// The "Control" port base.
    unsigned control_base;
    /// If the device master or slave.
    unsigned slave;
    /// If the device is connected to the primary bus.
    bool_t primary;
    /// The device identity data.
    ata_identify_t identity;
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
    /// Pointer to the first entry of the PRDT.
    prdt_t *dma_prdt;
    /// Physical address of the first entry of the PRDT.
    uintptr_t dma_prdt_phys;
    /// Pointer to the DMA memory area.
    uint8_t *dma_start;
    /// Physical address of the DMA memory area.
    uintptr_t dma_start_phys;
    /// Device root file.
    vfs_file_t *fs_root;
} ata_device_t;

#define ATA_SECTOR_SIZE 512 ///< The sector size.
#define ATA_DMA_SIZE    512 ///< The size of the DMA area.

static spinlock_t ata_lock;

static char ata_drive_char = 'a';
static int cdrom_number    = 0;
static uint32_t ata_pci    = 0x00000000;

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
    .control_base = 0x3F6,
    .slave        = 0,
    .primary      = true
};

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
    .control_base = 0x3F6,
    .slave        = 1,
    .primary      = true
};

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
    .control_base = 0x376,
    .slave        = 0,
    .primary      = false
};

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
    .control_base = 0x376,
    .slave        = 1,
    .primary      = false
};

static void ata_device_read_sector(ata_device_t *, uint64_t, uint8_t *);
static void ata_device_write_sector(ata_device_t *, uint64_t, uint8_t *);

static vfs_file_t *ata_open(const char *, int, mode_t);
static int ata_close(vfs_file_t *);
static ssize_t ata_read(vfs_file_t *, char *, off_t, size_t);
static ssize_t ata_write(vfs_file_t *, const void *, off_t, size_t);
static int ata_fstat(vfs_file_t *file, stat_t *stat);
static int ata_stat(const char *path, stat_t *stat);

// == SUPPORT FUNCTIONS =======================================================

static inline const char *ata_get_device_status_str(ata_device_t *dev)
{
    static char status_str[9]   = { 0 };
    uint8_t status              = inportb(dev->io_reg.status);
    status_str[ata_status_err]  = bit_check(status, ata_status_err) ? 'e' : ' ';
    status_str[ata_status_idx]  = bit_check(status, ata_status_idx) ? 'i' : ' ';
    status_str[ata_status_corr] = bit_check(status, ata_status_corr) ? 'c' : ' ';
    status_str[ata_status_drq]  = bit_check(status, ata_status_drq) ? 'd' : ' ';
    status_str[ata_status_srv]  = bit_check(status, ata_status_srv) ? 's' : ' ';
    status_str[ata_status_df]   = bit_check(status, ata_status_df) ? 'd' : ' ';
    status_str[ata_status_rdy]  = bit_check(status, ata_status_rdy) ? 'r' : ' ';
    status_str[ata_status_bsy]  = bit_check(status, ata_status_bsy) ? 'b' : ' ';
    return status_str;
}

static inline const char *ata_get_device_settings_str(ata_device_t *dev)
{
    if (dev->io_base == 0x1F0) {
        if (dev->slave)
            return "Primary Slave";
        return "Primary Master";
    }
    if (dev->slave)
        return "Secondary Slave";
    return "Secondary Master";
}

static inline const char *ata_get_device_type_str(ata_device_type_t type)
{
    if (type == ata_dev_type_pata)
        return "pata";
    if (type == ata_dev_type_sata)
        return "sata";
    if (type == ata_dev_type_patapi)
        return "patapi";
    if (type == ata_dev_type_satapi)
        return "satapi";
    if (type == ata_dev_type_unknown)
        return "unknown";
    return "no_device";
}

/// @brief Waits for 400 nanoseconds.
/// @param dev the device on which we wait.
static inline void ata_io_wait(ata_device_t *dev)
{
    inportb(dev->control_base);
    inportb(dev->control_base);
    inportb(dev->control_base);
    inportb(dev->control_base);
}

static inline uint8_t ata_status_wait(ata_device_t *dev, int timeout)
{
    uint8_t status;
    if (timeout > 0) {
        while (timeout--) {
            if (!bit_check((status = inportb(dev->io_reg.status)), ata_status_bsy))
                break;
        }
    } else {
        while (bit_check((status = inportb(dev->io_reg.status)), ata_status_bsy)) {
            cpu_relax();
        }
    }
    return status;
}

static inline int ata_wait(ata_device_t *dev, bool_t advanced)
{
    ata_io_wait(dev);
    ata_status_wait(dev, 0);
    if (advanced) {
        uint8_t status = inportb(dev->io_reg.status);
        if (bit_check(status, ata_status_err))
            return 1;
        if (bit_check(status, ata_status_df))
            return 1;
        if (!bit_check(status, ata_status_drq))
            return 1;
    }
    return 0;
}

static inline void ata_device_select(ata_device_t *dev)
{
    outportb(dev->io_base + 1, 1);
    outportb(dev->control_base, 0);
    outportb(dev->io_reg.hddevsel, 0xA0 | (dev->slave << 4));
    ata_io_wait(dev);
}

static inline uint64_t ata_max_offset(ata_device_t *dev)
{
    if (dev->identity.sectors_48) {
        return dev->identity.sectors_48 * ATA_SECTOR_SIZE;
    }
    return dev->identity.sectors_28 * ATA_SECTOR_SIZE;
}

static inline void ata_fix_string(char *str, unsigned len)
{
    char tmp;
    for (unsigned i = 0; i < len; i += 2) {
        tmp        = str[i + 1];
        str[i + 1] = str[i];
        str[i]     = tmp;
    }
    str[len] = 0;
}

static inline bool_t ata_read_device_identity(ata_device_t *dev, ata_identity_command_t command)
{
    // Request the device identity.
    outportb(dev->io_reg.command, command);
    // Wait 400ns for the command to work.
    ata_io_wait(dev);
    inportb(dev->io_reg.command);
    ata_wait(dev, 0);
    // Read the identity.
    uint16_t *buffer = (uint16_t *)&dev->identity;
    for (unsigned i = 0; i < 256; ++i) {
        buffer[i] = inports(dev->io_reg.data);
    }
    // Fix the serial.
    ata_fix_string((char *)&dev->identity.serial_number, 20 - 1);
    // Fix the firmware.
    ata_fix_string((char *)&dev->identity.firmware_revision, 8 - 1);
    // Fix the model.
    ata_fix_string((char *)&dev->identity.model_number, 40 - 1);
    return true;
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
    // Do a "software reset" on the bus.
    outportb(dev->control_base, ata_control_srst);
    // Wait for the soft reset to complete.
    ata_io_wait(dev);
    // Reset the bus to normal operation.
    outportb(dev->control_base, ata_control_zero);
}

static inline void ata_enable_bus_mastering(ata_device_t *dev)
{
    pr_debug("[%s] Enabling bust mastering...\n", ata_get_device_settings_str(dev));
    uint16_t pci_cmd = pci_read_field(ata_pci, PCI_COMMAND, 4);
    if (bit_check(pci_cmd, pci_command_bus_master)) {
        pr_warning("[%s] Bus mastering already enabled.\n", ata_get_device_settings_str(dev));
    } else {
        // Set the bit for bus mastering.
        bit_set_assign(pci_cmd, pci_command_bus_master);
        // Write the PCI command field.
        pci_write_field(ata_pci, PCI_COMMAND, 4, pci_cmd);
        // Check that the bus mastering is enabled.
        pci_cmd = pci_read_field(ata_pci, PCI_COMMAND, 4);
        if (!bit_check(pci_cmd, pci_command_bus_master)) {
            pr_warning("[%s] Bus mastering is not correctly set.\n", ata_get_device_settings_str(dev));
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
static inline void ata_initialize_bus_mastering_address(ata_device_t *dev)
{
    pr_debug("[%s] Initializing bust mastering...\n", ata_get_device_settings_str(dev));
    unsigned address = pci_read_field(ata_pci, PCI_BASE_ADDRESS_4, 4);
    // To distinguish between memory space BARs and I/O space BARs, you can
    // check the value of the lowest bit. memory space BARs has always a 0,
    // while I/O space BARs has always a 1.
    if (!bit_check(address, 0)) {
        kernel_panic("Failed to initialize BUS Mastering.");
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

/* on Primary bus: ctrl->base =0x1F0, ctrl->dev_ctl =0x3F6. REG_CYL_LO=4, REG_CYL_HI=5, REG_DEVSEL=6 */
static inline ata_device_type_t ata_detect_device_type(ata_device_t *dev)
{
    pr_debug("[%s] Detecting device type...\n", ata_get_device_settings_str(dev));
    // Perform a soft reset.
    ata_soft_reset(dev);
    // Wait until master drive is ready again.
    ata_io_wait(dev);
    // Select the drive.
    outportb(dev->io_reg.hddevsel, 0xA0 | (dev->slave << 4));
    // Wait for drive select to work.
    ata_io_wait(dev);
    // Get the "signature bytes" by reading low and high cylinder register.
    uint8_t cyl_lo = inportb(dev->io_reg.lba_mid);
    uint8_t cyl_hi = inportb(dev->io_reg.lba_hi);
    // Differentiate ATA, ATAPI, SATA and SATAPI.
    if ((cyl_lo == 0x00) && (cyl_hi == 0x00))
        return ata_dev_type_pata;
    if ((cyl_lo == 0x3C) && (cyl_hi == 0xC3))
        return ata_dev_type_sata;
    if ((cyl_lo == 0x14) && (cyl_hi == 0xEB))
        return ata_dev_type_patapi;
    if ((cyl_lo == 0x69) && (cyl_hi == 0x96))
        return ata_dev_type_satapi;
    if ((cyl_lo == 0xFF) && (cyl_hi == 0xFF))
        return ata_dev_type_no_device;
    return ata_dev_type_unknown;
}

/// @brief Creates the DMA memory area used to write and read on the device.
/// @param size the size of the DMA memory area.
/// @param physical the physical address of the DMA memory area.
/// @return the logical address of the DMA memory area.
static inline uintptr_t malloc_dma(size_t size, uintptr_t *physical)
{
    // Get the page order to accomodate the size.
    uint32_t order           = find_nearest_order_greater(0, size);
    page_t *page             = _alloc_pages(GFP_KERNEL, order);
    *physical                = get_physical_address_from_page(page);
    uintptr_t lowmem_address = get_lowmem_address_from_page(page);
    pr_debug("[DMA Malloc] Size requirement is %d, which results in an order %d\n", size, order);
    pr_debug("[DMA Malloc] Allocated page is at       : 0x%p\n", page);
    pr_debug("[DMA Malloc] The physical address is at : 0x%p\n", physical);
    pr_debug("[DMA Malloc] The lowmem address is at   : 0x%p\n", lowmem_address);
    return lowmem_address;
}

// == ATA DEVICE MANAGEMENT ===================================================
static bool_t ata_device_init(ata_device_t *dev)
{
    pr_debug("[%s] Detected ATA device.\n", ata_get_device_settings_str(dev));

    // Select the ATA device.
    ata_device_select(dev);

    // Read the ATA device identity.
    ata_read_device_identity(dev, ata_command_pata_ident);

    // Allocate the memory for the Physical Region Descriptor Table (PRDT).
    dev->dma_prdt = (prdt_t *)malloc_dma(sizeof(prdt_t), &dev->dma_prdt_phys);
    // Allocate the memory for the Direct Memory Access (DMA).
    dev->dma_start = (uint8_t *)malloc_dma(ATA_DMA_SIZE, &dev->dma_start_phys);
    // Initialize the table, specifying the physical address of the DMA.
    dev->dma_prdt->physical_address = dev->dma_start_phys;
    // The size of the DMA.
    dev->dma_prdt->byte_count = ATA_DMA_SIZE;
    // Set the EOT to 1.
    dev->dma_prdt->end_of_table = 0x8000;

    // Update the filesystem entry with the length of the device.
    dev->fs_root->length = ata_max_offset(dev);

    // Enable bus mastering.
    ata_enable_bus_mastering(dev);

    // Initialize the bus mastering addresses.
    ata_initialize_bus_mastering_address(dev);

    // Print the device data.
    pr_debug("Device name     : %s\n", dev->name);
    pr_debug("Device status   : [%s]\n", ata_get_device_status_str(dev));
    pr_debug("Device Serial   : %s\n", dev->identity.serial_number);
    pr_debug("Device Firmware : %s\n", dev->identity.firmware_revision);
    pr_debug("Device Model    : %s\n", dev->identity.model_number);
    pr_debug("Sectors (48)    : %d\n", dev->identity.sectors_48);
    pr_debug("Sectors (24)    : %d\n", dev->identity.sectors_28);
    pr_debug("PCI device ID   : 0x%x\n", ata_pci);
    pr_debug("Device PRDT     : 0x%x (Ph: 0x%x)\n", dev->dma_prdt, dev->dma_prdt_phys);
    pr_debug("Device START    : 0x%x (Ph: 0x%x)\n", dev->dma_start, dev->dma_start_phys);
    pr_debug("BMR Command     : 0x%x\n", dev->bmr.command);
    pr_debug("BMR Status      : 0x%x\n", dev->bmr.status);
    pr_debug("BMR Prdt        : 0x%x\n", dev->bmr.prdt);
    return 0;
}

// == ATA SECTOR READ/WRITE FUNCTIONS =========================================
static void ata_device_read_sector(ata_device_t *dev, uint32_t lba, uint8_t *buffer)
{
    // Check if we are trying to perform the read on the correct drive type.
    if ((dev->type != ata_dev_type_pata) && (dev->type != ata_dev_type_sata)) {
        return;
    }
    //pr_debug("ata_device_read_sector(dev: %p, lba: %d, buff: %p)\n", dev, lba, buffer);
    spinlock_lock(&ata_lock);

    ata_wait(dev, 0);

    // Reset bus master register's command register.
    outportb(dev->bmr.command, 0x00);

    // Set the PRDT.
    outportl(dev->bmr.prdt, dev->dma_prdt_phys);

    // Enable error, irq status.
    outportb(dev->bmr.status, inportb(dev->bmr.status) | 0x04 | 0x02);

    // Set read.
    outportb(dev->bmr.command, 0x08);

    while (1) {
        uint8_t status = inportb(dev->io_reg.status);
        if (!bit_check(status, ata_status_bsy))
            break;
    }

    outportb(dev->control_base, 0x00);
    outportb(dev->io_reg.hddevsel, 0xe0 | (dev->slave << 4));
    ata_io_wait(dev);
    outportb(dev->io_reg.feature, 0x00);

    outportb(dev->io_reg.sector_count, 0);
    outportb(dev->io_reg.lba_lo, (lba & 0xff000000) >> 24);
    outportb(dev->io_reg.lba_mid, (lba & 0xff00000000) >> 32);
    outportb(dev->io_reg.lba_hi, (lba & 0xff0000000000) >> 40);

    outportb(dev->io_reg.sector_count, 1);
    outportb(dev->io_reg.lba_lo, (lba & 0x000000ff) >> 0);
    outportb(dev->io_reg.lba_mid, (lba & 0x0000ff00) >> 8);
    outportb(dev->io_reg.lba_hi, (lba & 0x00ff0000) >> 16);

    while (1) {
        uint8_t status = inportb(dev->io_reg.status);
        if (!bit_check(status, ata_status_bsy) && bit_check(status, ata_status_rdy))
            break;
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
        if (!bit_check(dstatus, ata_status_bsy)) {
            break;
        }
    }

    // Copy from DMA buffer to output buffer.
    memcpy(buffer, dev->dma_start, ATA_DMA_SIZE);

    // Inform device we are done.
    outportb(dev->bmr.status, inportb(dev->bmr.status) | 0x04 | 0x02);

    spinlock_unlock(&ata_lock);
}

static void ata_device_write_sector(ata_device_t *dev, uint32_t lba, uint8_t *buffer)
{
    spinlock_lock(&ata_lock);

    // Copy the buffer over to the DMA area
    memcpy(dev->dma_start, buffer, ATA_DMA_SIZE);

    // Reset bus master register's command register
    outportb(dev->bmr.command, 0);

    // Set prdt
    outportl(dev->bmr.prdt, dev->dma_prdt_phys);

    // Enable error, irq status.
    outportb(dev->bmr.status, inportb(dev->bmr.status) | 0x04 | 0x02);

    // Select drive
    ata_wait(dev, 0);
    outportb(dev->io_reg.hddevsel, 0xe0 | dev->slave << 4 | (lba & 0x0f000000) >> 24);
    ata_wait(dev, 0);

    // Set sector counts and LBAs
    outportb(dev->io_reg.feature, 0x00);
    outportb(dev->io_reg.sector_count, 1);
    outportb(dev->io_reg.lba_lo, (lba & 0x000000ff) >> 0);
    outportb(dev->io_reg.lba_mid, (lba & 0x0000ff00) >> 8);
    outportb(dev->io_reg.lba_hi, (lba & 0x00ff0000) >> 16);
    ata_wait(dev, 0);

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

    spinlock_unlock(&ata_lock);
}

// == VFS ENTRY GENERATION ====================================================
/// Filesystem general operations.
static vfs_sys_operations_t ata_sys_operations = {
    .mkdir_f = NULL,
    .rmdir_f = NULL,
    .stat_f  = ata_stat
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
    .getdents_f = NULL
};

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
    // Set the device.
    file->device = dev;
    // Re-set the flags.
    file->flags = DT_BLK;
    // Change the operations.
    file->sys_operations = &ata_sys_operations;
    file->fs_operations  = &ata_fs_operations;
    return file;
}

// == VFS CALLBACKS ===========================================================
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

static ssize_t ata_read(vfs_file_t *file, char *buffer, off_t offset, size_t size)
{
    pr_debug("ata_read(file: 0x%p, buffer: 0x%p, offest: %8d, size: %8d)\n", file, buffer, offset, size);
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

static int _ata_stat(const ata_device_t *dev, stat_t *stat)
{
    if (dev && dev->fs_root) {
        pr_debug("_ata_stat(%p, %p)\n", dev, stat);
        stat->st_dev   = 0;
        stat->st_ino   = 0;
        stat->st_mode  = 0;
        stat->st_uid   = 0;
        stat->st_gid   = 0;
        stat->st_atime = sys_time(NULL);
        stat->st_mtime = sys_time(NULL);
        stat->st_ctime = sys_time(NULL);
        stat->st_size  = dev->fs_root->length;
    }
    return 0;
}

/// @brief      Retrieves information concerning the file at the given position.
/// @param fid  The file struct.
/// @param stat The structure where the information are stored.
/// @return     0 if success.
static int ata_fstat(vfs_file_t *file, stat_t *stat)
{
    return _ata_stat(file->device, stat);
}

/// @brief      Retrieves information concerning the file at the given position.
/// @param path The path where the file resides.
/// @param stat The structure where the information are stored.
/// @return     0 if success.
static int ata_stat(const char *path, stat_t *stat)
{
    super_block_t *sb = vfs_get_superblock(path);
    if (sb && sb->root) {
        return _ata_stat(sb->root->device, stat);
    }
    return -1;
}

static ata_device_type_t ata_device_detect(ata_device_t *dev)
{
    // Detect the device type.
    ata_device_type_t type = ata_detect_device_type(dev);
    // Exit if there is no drive.
    if (type == ata_dev_type_no_device) {
        pr_debug("No drive(s) present\n");
        return type;
    }
    // Parallel ATA drive, or emulated SATA.
    if ((type == ata_dev_type_pata) || (type == ata_dev_type_sata)) {
        // Device type supported, set it.
        dev->type = type;
        // Set the device name.
        sprintf(dev->name, "hd%c", ata_drive_char);
        // Set the device path.
        sprintf(dev->path, "/dev/hd%c", ata_drive_char);
        // Create the filesystem entry for the drive.
        dev->fs_root = ata_device_create(dev);
        // Check if we failed to create the filesystem entry.
        if (!dev->fs_root) {
            pr_crit("Failed to create ata device!\n");
            return 1;
        }
        // Try to mount the drive.
        if (!vfs_mount(dev->path, dev->fs_root)) {
            pr_crit("Failed to mount ata device!\n");
            return 1;
        }
        // Initialize the drive.
        if (ata_device_init(dev)) {
            pr_crit("Failed to initialize ata device!\n");
            return 1;
        }
        // Increment the drive letter.
        ++ata_drive_char;
    } else if ((type == ata_dev_type_patapi) || (type == ata_dev_type_satapi)) {
        pr_warning("ATAPI and SATAPI drives are not currently supported.\n");
        return ata_dev_type_no_device;
    } else {
        pr_alert("Unsupported drive type.\n");
        return ata_dev_type_unknown;
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
static void pci_find_ata(uint32_t dev, uint16_t vid, uint16_t did, void *extra)
{
    if ((vid == 0x8086) && (did == 0x7010 || did == 0x7111)) {
        *((uint32_t *)extra) = dev;
    }
}

// == INITIALIZE/FINALIZE ATA =================================================
int ata_initialize()
{
    // Initialize the spinlock.
    spinlock_init(&ata_lock);

    // Search for ATA devices.
    pci_scan(&pci_find_ata, -1, &ata_pci);

    // Install the IRQ handlers.
    irq_install_handler(IRQ_FIRST_HD, ata_irq_handler_master, "IDE Master");
    irq_install_handler(IRQ_SECOND_HD, ata_irq_handler_slave, "IDE Slave");

    ata_device_type_t type;
    type = ata_device_detect(&ata_primary_master);
    if ((type != ata_dev_type_no_device) && (type != ata_dev_type_unknown)) {
        pr_info("    Found %s device connected to primary master.\n", ata_get_device_type_str(type));
    }
    type = ata_device_detect(&ata_primary_slave);
    if ((type != ata_dev_type_no_device) && (type != ata_dev_type_unknown)) {
        pr_info("    Found %s device connected to primary slave.\n", ata_get_device_type_str(type));
    }
    type = ata_device_detect(&ata_secondary_master);
    if ((type != ata_dev_type_no_device) && (type != ata_dev_type_unknown)) {
        pr_info("    Found %s device connected to secondary master.\n", ata_get_device_type_str(type));
    }
    type = ata_device_detect(&ata_secondary_slave);
    if ((type != ata_dev_type_no_device) && (type != ata_dev_type_unknown)) {
        pr_info("    Found %s device connected to secondary slave.\n", ata_get_device_type_str(type));
    }
    return 0;
}

int ata_finalize()
{
    return 0;
}

/// @}
