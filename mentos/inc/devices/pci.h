///                MentOS, The Mentoring Operating system project
/// @file pci.h
/// @brief Routines for interfacing with the Peripheral Component Interconnect (PCI).
/// @copyright (c) 2014-2021 This file is distributed under the MIT License.
/// See LICENSE.md for details.
/// @addtogroup devices Hardware Interfaces
/// @{
/// @addtogroup pci Peripheral Component Interconnect (PCI)
/// @brief Routines for interfacing with the peripherals.
/// @{

#pragma once

#include "stdint.h"

/// @name PCI Configuration Space
/// @brief
/// The PCI Specification defines the organization of the 256-byte.
/// Configuration Space registers and imposes a specific template for the
/// space. Figures 2 & 3 show the layout of the 256-byte Configuration space.
/// All PCI compliant devices must support the Vendor ID, Device ID, Command
/// and Status, Revision ID, Class Code and Header Type fields. Implementation
/// of the other registers is optional, depending upon the devices
/// functionality.
/// @{

/// @brief Identifies the manufacturer of the device (16 bits). Where valid IDs are allocated by PCI-SIG (the list is here) to
/// ensure uniqueness and 0xFFFF is an invalid value that will be returned 
/// on read accesses to Configuration Space registers of non-existent devices.
#define PCI_VENDOR_ID 0x00

/// @brief Identifies the particular device (16 bits).
#define PCI_DEVICE_ID 0x02

/// @brief Provides control over a device's ability to generate and
/// respond to PCI cycles (16 bits). Where the only functionality guaranteed to be supported by all 
/// devices is, when a 0 is written to this register, the device is disconnected
/// from the PCI bus for all accesses except Configuration Space access.
#define PCI_COMMAND 0x04

/// @brief A register used to record status information for PCI bus related events (16 bits).
#define PCI_STATUS 0x06

/// @brief Specifies a revision identifier for a particular device (8 bits).
#define PCI_REVISION_ID 0x08

/// @brief A read-only register that specifies a register-level
/// programming interface the device has, if it has any at all (8 bits).
#define PCI_PROG_IF 0x09

/// @brief A read-only register that specifies the specific function the
/// device performs (8 bits).
#define PCI_SUBCLASS 0x0a

/// @brief A read-only register that specifies the type of function the
/// device performs (8 bits).
#define PCI_CLASS 0x0b

/// @brief Specifies the system cache line size in 32-bit units (8 bits). A device
/// can limit the number of cacheline sizes it can support, if a unsupported
/// value is written to this field, the device will behave as if a value of 0
/// was written.
#define PCI_CACHE_LINE_SIZE 0x0c

/// Specifies the latency timer in units of PCI bus clocks (8 bits).
#define PCI_LATENCY_TIMER 0x0d

/// @brief Identifies the layout of the header based on the type of device it 
/// begins at byte 0x10 of the header (8 bits). A value of 0x00 specifies a general device, a value of 0x01 specifies
/// a PCI-to-PCI bridge, and a value of 0x02 specifies a CardBus bridge. 
/// If bit 7 of this register is set, the device has multiple functions; 
/// otherwise, it is a single function device.
#define PCI_HEADER_TYPE 0x0e

/// @brief Represents that status and allows control of devices built-in self tests (8 bits).
#define PCI_BIST 0x0f

/// @brief Points to the Card Information Structure and is used by devices that share silicon
/// between CardBus and PCI.
#define PCI_CARDBUS_CIS 0x28

/// @brief Points to the Subsystem Vendor ID
#define PCI_SUBSYSTEM_VENDOR_ID 0x2c

/// @brief Points to the Subsystem Device ID
#define PCI_SUBSYSTEM_ID 0x2e

/// @brief Bits 31..11 are address, 10..1 reserved
#define PCI_ROM_ADDRESS 0x30

/// @brief Points to a linked list of new capabilities implemented by the device. Used if bit 4 of the status register (Capabilities List bit) is set to 1.
/// The bottom two bits are reserved and should be masked before the Pointer
/// is used to access the Configuration Space.
#define PCI_CAPABILITY_LIST 0x34

/// @brief Specifies which input of the system interrupt controllers the device's
/// interrupt pin is connected to and is implemented by any device that makes
/// use of an interrupt pin. For the x86 architecture this register
/// corresponds to the PIC IRQ numbers 0-15 (and not I/O APIC IRQ numbers) and
/// a value of 0xFF defines no connection.
#define PCI_INTERRUPT_LINE 0x3c

/// @brief Specifies which interrupt pin the device uses. Where a value of 0x01 is INTA#, 0x02 is INTB#, 0x03 is INTC#, 
/// 0x04 is INTD#, and 0x00 means the device does not use an interrupt pin.
#define PCI_INTERRUPT_PIN 0x3d

/// @brief A read-only register that specifies the burst period length, in 1/4
/// microsecond units, that the device needs (assuming a 33 MHz clock rate).
#define PCI_MIN_GNT 0x3e

/// @brief A read-only register that specifies how often the device needs access to
/// the PCI bus (in 1/4 microsecond units).
#define PCI_MAX_LAT 0x3f

/// @}

/// @name PCI Base Addresses
/// @brief
/// Base addresses specify locations in memory or I/O space.
/// Decoded size can be determined by writing a value of 0xffffffff to the
/// register, and reading it back. Only 1 bits are decoded.
/// @{

#define PCI_BASE_ADDRESS_0 0x10 ///< Location of base address 0.
#define PCI_BASE_ADDRESS_1 0x14 ///< Location of base address 1.
#define PCI_BASE_ADDRESS_2 0x18 ///< Location of base address 2.
#define PCI_BASE_ADDRESS_3 0x1c ///< Location of base address 3.
#define PCI_BASE_ADDRESS_4 0x20 ///< Location of base address 4.
#define PCI_BASE_ADDRESS_5 0x24 ///< Location of base address 5.

/// @}

#define PCI_PRIMARY_BUS   0x18 ///< Primary bus number.
#define PCI_SECONDARY_BUS 0x19 ///< Secondary bus number.

#define PCI_HEADER_TYPE_NORMAL  0 ///< TODO: Document.
#define PCI_HEADER_TYPE_BRIDGE  1 ///< TODO: Document.
#define PCI_HEADER_TYPE_CARDBUS 2 ///< TODO: Document.

#define PCI_TYPE_BRIDGE 0x060400 ///< TODO: Document.
#define PCI_TYPE_SATA   0x010600 ///< TODO: Document.

#define PCI_ADDRESS_PORT 0xCF8  ///< TODO: Document.
#define PCI_VALUE_PORT   0xCFC  ///< TODO: Document.
#define PCI_NONE         0xFFFF ///< TODO: Document.

/// @brief PIC scan function.
typedef void (*pci_scan_func_t)(uint32_t device, uint16_t vendor_id, uint16_t device_id, void *extra);

/// @brief Extract the `bus` from the device.
static inline int pci_extract_bus(uint32_t device)
{
    return (uint8_t)((device >> 16));
}

/// @brief Extract the `slot` from the device.
static inline int pci_extract_slot(uint32_t device)
{
    return (uint8_t)((device >> 8));
}

/// @brief Extract the `func` from the device.
static inline int pci_extract_func(uint32_t device)
{
    return (uint8_t)(device);
}

/// @brief TODO: doxygen comment.
static inline uint32_t pci_get_addr(uint32_t device, int field)
{
    return 0x80000000 | (pci_extract_bus(device) << 16) |
           (pci_extract_slot(device) << 11) | (pci_extract_func(device) << 8) |
           ((field)&0xFC);
}

/// @brief TODO: doxygen comment.
static inline uint32_t pci_box_device(int bus, int slot, int func)
{
    return (uint32_t)((bus << 16) | (slot << 8) | func);
}

/// @brief Reads a field from the given PCI device.
uint32_t pci_read_field(uint32_t device, int field, int size);

// TODO: doxygen comment.
/// @brief
void pci_write_field(uint32_t device, int field, int size, uint32_t value);

// TODO: doxygen comment.
/// @brief
uint32_t pci_find_type(uint32_t device);

// TODO: doxygen comment.
/// @brief
const char *pci_vendor_lookup(unsigned short vendor_id);

// TODO: doxygen comment.
/// @brief
const char *pci_device_lookup(unsigned short vendor_id,
                              unsigned short device_id);
// TODO: doxygen comment.
/// @brief
void pci_scan_hit(pci_scan_func_t f, uint32_t dev, void *extra);

// TODO: doxygen comment.
/// @brief
void pci_scan_func(pci_scan_func_t f, int type, int bus, int slot, int func, void *extra);

// TODO: doxygen comment.
/// @brief
void pci_scan_slot(pci_scan_func_t f, int type, int bus, int slot, void *extra);

// TODO: doxygen comment.
/// @brief
void pci_scan_bus(pci_scan_func_t f, int type, int bus, void *extra);

// TODO: doxygen comment.
/// @brief
void pci_scan(pci_scan_func_t f, int type, void *extra);

// TODO: doxygen comment.
/// @brief
void pci_remap(void);

// TODO: doxygen comment.
/// @brief
int pci_get_interrupt(uint32_t device);

// TODO: doxygen comment.
/// @brief
void pci_debug_scan();

/// @}
/// @}