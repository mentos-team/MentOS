///                MentOS, The Mentoring Operating system project
/// @file   pci.h
/// @brief  Routines for PCI initialization.
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "stdint.h"

/// @defgroup pci_configuration_space The PCI configuration space.
/// The PCI Specification defines the organization of the 256-byte.
/// Configuration Space registers and imposes a specific template for the
/// space. Figures 2 & 3 show the layout of the 256-byte Configuration space.
/// All PCI compliant devices must support the Vendor ID, Device ID, Command
/// and Status, Revision ID, Class Code and Header Type fields. Implementation
/// of the other registers is optional, depending upon the devices
/// functionality.
/// @{

/// 16 bits - Identifies the manufacturer of the device. Where valid IDs are
/// allocated by PCI-SIG (the list is here) to ensure uniqueness and 0xFFFF is
/// an invalid value that will be returned on read accesses to Configuration
/// Space registers of non-existent devices.
#define PCI_VENDOR_ID 0x00

/// 16 bits - Identifies the particular device. Where valid IDs are allocated
/// by the vendor.
#define PCI_DEVICE_ID 0x02

/// 16 bits - Provides control over a device's ability to generate and
/// respond to PCI cycles. Where the only functionality guaranteed to be
/// supported by all devices is, when a 0 is written to this register, the
/// device is disconnected from the PCI bus for all accesses except
/// Configuration Space access.
#define PCI_COMMAND 0x04

/// 16 bits - A register used to record status information for PCI bus
/// related events.
#define PCI_STATUS 0x06

/// 8 bits - Specifies a revision identifier for a particular device. Where
/// valid IDs are allocated by the vendor.
#define PCI_REVISION_ID 0x08

/// 8 bits - A read-only register that specifies a register-level
/// programming interface the device has, if it has any at all.
#define PCI_PROG_IF 0x09

/// 8 bits - A read-only register that specifies the specific function the
/// device performs.
#define PCI_SUBCLASS 0x0a

/// 8 bits - A read-only register that specifies the type of function the
/// device performs.
#define PCI_CLASS 0x0b

/// 8 bits - Specifies the system cache line size in 32-bit units. A device
/// can limit the number of cacheline sizes it can support, if a unsupported
/// value is written to this field, the device will behave as if a value of 0
/// was written.
#define PCI_CACHE_LINE_SIZE 0x0c

/// 8 bits - Specifies the latency timer in units of PCI bus clocks.
#define PCI_LATENCY_TIMER 0x0d

/// 8 bits - Identifies the layout of the rest of the header beginning at
/// byte 0x10 of the header and also specifies whether or not the device has
/// multiple functions. Where a value of 0x00 specifies a general device, a
/// value of 0x01 specifies a PCI-to-PCI bridge, and a value of 0x02 specifies
/// a CardBus bridge. If bit 7 of this register is set, the device has
/// multiple functions; otherwise, it is a single function device.
#define PCI_HEADER_TYPE 0x0e

/// 8 bits - Represents that status and allows control of a devices BIST
/// (built-in self test).
#define PCI_BIST 0x0f

/// @defgroup pci_base_addresses The PCI base addresses.
/// Base addresses specify locations in memory or I/O space.
/// Decoded size can be determined by writing a value of 0xffffffff to the
/// register, and reading it back. Only 1 bits are decoded.
/// @{

#define PCI_BASE_ADDRESS_0 0x10

#define PCI_BASE_ADDRESS_1 0x14

#define PCI_BASE_ADDRESS_2 0x18

#define PCI_BASE_ADDRESS_3 0x1c

#define PCI_BASE_ADDRESS_4 0x20

#define PCI_BASE_ADDRESS_5 0x24

/// @} pci_base_addresses

/// Points to the Card Information Structure and is used by devices that
/// share silicon between CardBus and PCI.
#define PCI_CARDBUS_CIS 0x28

#define PCI_SUBSYSTEM_VENDOR_ID 0x2c

#define PCI_SUBSYSTEM_ID 0x2e

#define PCI_ROM_ADDRESS 0x30

/// Points to a linked list of new capabilities implemented by the device.
/// Used if bit 4 of the status register (Capabilities List bit) is set to 1.
/// The bottom two bits are reserved and should be masked before the Pointer
/// is used to access the Configuration Space.
#define PCI_CAPABILITY_LIST 0x34

/// Specifies which input of the system interrupt controllers the device's
/// interrupt pin is connected to and is implemented by any device that makes
/// use of an interrupt pin. For the x86 architecture this register
/// corresponds to the PIC IRQ numbers 0-15 (and not I/O APIC IRQ numbers) and
/// a value of 0xFF defines no connection.
#define PCI_INTERRUPT_LINE 0x3c

/// Specifies which interrupt pin the device uses. Where a value of 0x01 is
/// INTA#, 0x02 is INTB#, 0x03 is INTC#, 0x04 is INTD#, and 0x00 means the
/// device does not use an interrupt pin.
#define PCI_INTERRUPT_PIN 0x3d

/// A read-only register that specifies the burst period length, in 1/4
/// microsecond units, that the device needs (assuming a 33 MHz clock rate).
#define PCI_MIN_GNT 0x3e

/// A read-only register that specifies how often the device needs access to
/// the PCI bus (in 1/4 microsecond units).
#define PCI_MAX_LAT 0x3f

/// @} pci_configuration_space

#define PCI_SECONDARY_BUS 0x19

#define PCI_HEADER_TYPE_DEVICE 0

#define PCI_HEADER_TYPE_BRIDGE 1

#define PCI_HEADER_TYPE_CARDBUS 2

#define PCI_TYPE_BRIDGE 0x060400

#define PCI_TYPE_SATA 0x010600

#define PCI_ADDRESS_PORT 0xCF8

#define PCI_VALUE_PORT 0xCFC

#define PCI_NONE 0xFFFF

// TODO: doxygen comment.
/// @brief
typedef void (*pci_func_t)(uint32_t device, uint16_t vendor_id,
			   uint16_t device_id, void *extra);

// TODO: doxygen comment.
/// @brief
static inline int pci_extract_bus(uint32_t device)
{
	return (uint8_t)((device >> 16));
}

// TODO: doxygen comment.
/// @brief
static inline int pci_extract_slot(uint32_t device)
{
	return (uint8_t)((device >> 8));
}

// TODO: doxygen comment.
/// @brief
static inline int pci_extract_func(uint32_t device)
{
	return (uint8_t)(device);
}

// TODO: doxygen comment.
/// @brief
static inline uint32_t pci_get_addr(uint32_t device, int field)
{
	return 0x80000000 | (pci_extract_bus(device) << 16) |
	       (pci_extract_slot(device) << 11) |
	       (pci_extract_func(device) << 8) | ((field)&0xFC);
}

// TODO: doxygen comment.
/// @brief
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
void pci_scan_hit(pci_func_t f, uint32_t dev, void *extra);

// TODO: doxygen comment.
/// @brief
void pci_scan_func(pci_func_t f, int type, int bus, int slot, int func,
		   void *extra);

// TODO: doxygen comment.
/// @brief
void pci_scan_slot(pci_func_t f, int type, int bus, int slot, void *extra);

// TODO: doxygen comment.
/// @brief
void pci_scan_bus(pci_func_t f, int type, int bus, void *extra);

// TODO: doxygen comment.
/// @brief
void pci_scan(pci_func_t f, int type, void *extra);

// TODO: doxygen comment.
/// @brief
void pci_remap(void);

// TODO: doxygen comment.
/// @brief
int pci_get_interrupt(uint32_t device);

// TODO: doxygen comment.
/// @brief
void pci_debug_scan();
