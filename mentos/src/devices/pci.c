/// @file pci.c
/// @brief Routines for PCI initialization.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.
///! @cond Doxygen_Suppress

// Setup the logging for this file (do this before any other include).
#include "sys/kernel_levels.h"           // Include kernel log levels.
#define __DEBUG_HEADER__ "[PCI   ]"      ///< Change header.
#define __DEBUG_LEVEL__  LOGLEVEL_NOTICE ///< Set log level.
#include "io/debug.h"                    // Include debugging functions.

#include "devices/pci.h"
#include "io/port_io.h"
#include "string.h"

struct {
    uint16_t id;
    const char *name;
} _pci_vendors[] = {
    { 0x1022, "AMD" },
    { 0x106b, "Apple, Inc." },
    { 0x1234, "Bochs/QEMU" },
    { 0x1274, "Ensoniq" },
    { 0x15ad, "VMWare" },
    { 0x8086, "Intel Corporation" },
    { 0x80EE, "VirtualBox" },
};

struct {
    uint16_t ven_id;
    uint16_t dev_id;
    const char *name;
} _pci_devices[] = {
    { 0x1022, 0x2000, "PCNet Ethernet Controller (pcnet)" },
    { 0x106b, 0x003f, "OHCI Controller" },
    { 0x1234, 0x1111, "VGA BIOS Graphics Extensions" },
    { 0x1274, 0x1371, "Creative Labs CT2518 (ensoniq audio)" },
    { 0x15ad, 0x0740, "VM Communication Interface" },
    { 0x15ad, 0x0405, "SVGA II Adapter" },
    { 0x15ad, 0x0790, "PCI bridge" },
    { 0x15ad, 0x07a0, "PCI Express Root Port" },
    { 0x8086, 0x100e, "Gigabit Ethernet Controller (e1000)" },
    { 0x8086, 0x100f, "Gigabit Ethernet Controller (e1000)" },
    { 0x8086, 0x1237, "PCI & Memory" },
    { 0x8086, 0x2415, "AC'97 Audio Chipset" },
    { 0x8086, 0x7000, "PCI-to-ISA Bridge" },
    { 0x8086, 0x7010, "IDE Interface" },
    { 0x8086, 0x7110, "PIIX4 ISA" },
    { 0x8086, 0x7111, "PIIX4 IDE" },
    { 0x8086, 0x7113, "Power Management Controller" },
    { 0x8086, 0x7190, "Host Bridge" },
    { 0x8086, 0x7191, "AGP Bridge" },
    { 0x80EE, 0xBEEF, "Bochs/QEMU-compatible Graphics Adapter" },
    { 0x80EE, 0xCAFE, "Guest Additions Device" },
};

struct {
    uint32_t id;
    const char *name;
} _pci_types[] = {
    { 0x000000, "Legacy Device" },
    { 0x000100, "VGA-Compatible Device" },

    { 0x010000, "SCSI bus controller" },
    { 0x010100, "ISA Compatibility mode-only controller" },
    { 0x010105, "PCI native mode-only controller" },
    { 0x01010a, "ISA Compatibility mode controller, supports both channels "
                "switched to PCI native mode" },
    { 0x01010f, "PCI native mode controller, supports both channels switched "
                "to ISA compatibility mode" },
    { 0x010180,
      "ISA Compatibility mode-only controller, supports bus mastering" },
    { 0x010185, "PCI native mode-only controller, supports bus mastering" },
    { 0x01018a, "ISA Compatibility mode controller, supports both channels "
                "switched to PCI native mode, supports bus mastering" },
    { 0x01018f, "PCI native mode controller, supports both channels switched "
                "\to ISA compatibility mode, supports bus mastering" },

    { 0x010200, "Floppy disk controller" },
    { 0x010300, "IPI bus controller" },
    { 0x010400, "RAID controller" },
    { 0x010520, "ATA controller, single stepping" },
    { 0x010530, "ATA controller, continuous" },
    { 0x010600, "Serial ATA controller - vendor specific interface" },
    { 0x010601, "Serial ATA controller - AHCI 1.0 interface" },
    { 0x010700, "Serial Attached SCSI controller" },
    { 0x018000, "Mass Storage controller" },

    { 0x020000, "Ethernet controller" },
    { 0x020100, "Token Ring controller" },
    { 0x020200, "FDDI controller" },
    { 0x020300, "ATM controller" },
    { 0x020400, "ISDN controller" },
    { 0x020500, "WorldFip controller" },
    // { 0x0206xx , "PICMG 2.14 Multi Computing" },
    { 0x028000, "Network controller" },

    { 0x030000, "VGA Display controller" },
    { 0x030001, "8514-compatible Display controller" },
    { 0x030100, "XGA Display controller" },
    { 0x030200, "3D Display controller" },
    { 0x038000, "Display controller" },

    { 0x040000, "Video device" },
    { 0x040100, "Audio device" },
    { 0x040200, "Computer Telephony device" },
    { 0x048000, "Multimedia device" },

    { 0x050000, "RAM memory controller" },
    { 0x050100, "Flash memory controller" },
    { 0x058000, "Memory controller" },

    { 0x060000, "Host bridge" },
    { 0x060100, "ISA bridge" },
    { 0x060200, "EISA bridge" },
    { 0x060300, "MCA bridge" },
    { 0x060400, "PCI-to-PCI bridge" },
    { 0x060401, "PCI-to-PCI bridge (subtractive decoding)" },
    { 0x060500, "PCMCIA bridge" },
    { 0x060600, "NuBus bridge" },
    { 0x060700, "CardBus bridge" },
    // { 0x0608xx , "RACEway bridge" },
    { 0x060940, "PCI-to-PCI bridge, Semi-transparent, primary facing Host" },
    { 0x060980, "PCI-to-PCI bridge, Semi-transparent, secondary facing Host" },
    { 0x060A00, "InfiniBand-to-PCI host bridge" },
    { 0x068000, "Bridge device" },

    { 0x070000, "Generic XT-compatible serial controller" },
    { 0x070001, "16450-compatible serial controller" },
    { 0x070002, "16550-compatible serial controller" },
    { 0x070003, "16650-compatible serial controller" },
    { 0x070004, "16750-compatible serial controller" },
    { 0x070005, "16850-compatible serial controller" },
    { 0x070006, "16950-compatible serial controller" },

    { 0x070100, "Parallel port" },
    { 0x070101, "Bi-directional parallel port" },
    { 0x070102, "ECP 1.X compliant parallel port" },
    { 0x070103, "IEEE1284 controller" },
    { 0x0701FE, "IEEE1284 target device" },
    { 0x070200, "Multiport serial controller" },

    { 0x070300, "Generic modem" },
    { 0x070301, "Hayes 16450-compatible modem" },
    { 0x070302, "Hayes 16550-compatible modem" },
    { 0x070303, "Hayes 16650-compatible modem" },
    { 0x070304, "Hayes 16750-compatible modem" },
    { 0x070400, "GPIB (IEEE 488.1/2) controller" },
    { 0x070500, "Smart Card" },
    { 0x078000, "Communications device" },

    { 0x080000, "Generic 8259 PIC" },
    { 0x080001, "ISA PIC" },
    { 0x080002, "EISA PIC" },
    { 0x080010, "I/O APIC interrupt controller" },
    { 0x080020, "I/O(x) APIC interrupt controller" },

    { 0x080100, "Generic 8237 DMA controller" },
    { 0x080101, "ISA DMA controller" },
    { 0x080102, "EISA DMA controller" },

    { 0x080200, "Generic 8254 system timer" },
    { 0x080201, "ISA system timer" },
    { 0x080202, "EISA system timer-pair" },

    { 0x080300, "Generic RTC controller" },
    { 0x080301, "ISA RTC controller" },

    { 0x080400, "Generic PCI Hot-Plug controller" },
    { 0x080500, "SD Host controller" },
    { 0x088000, "System peripheral" },

    { 0x090000, "Keyboard controller" },
    { 0x090100, "Digitizer (pen)" },
    { 0x090200, "Mouse controller" },
    { 0x090300, "Scanner controller" },
    { 0x090400, "Generic Gameport controller" },
    { 0x090410, "Legacy Gameport controller" },
    { 0x098000, "Input controller" },

    { 0x0a0000, "Generic docking station" },
    { 0x0a8000, "Docking station" },

    { 0x0b0000, "386 Processor" },
    { 0x0b0100, "486 Processor" },
    { 0x0b0200, "Pentium Processor" },
    { 0x0b1000, "Alpha Processor" },
    { 0x0b2000, "PowerPC Processor" },
    { 0x0b3000, "MIPS Processor" },
    { 0x0b4000, "Co-processor" },

    { 0x0c0000, "IEEE 1394 (FireWire)" },
    { 0x0c0010, "IEEE 1394 -- OpenHCI spec" },
    { 0x0c0100, "ACCESS.bus" },
    { 0x0c0200, "SSA" },
    { 0x0c0300, "Universal Serial Bus (UHC spec)" },
    { 0x0c0310, "Universal Serial Bus (Open Host spec)" },
    { 0x0c0320, "USB2 Host controller (Intel Enhanced HCI spec)" },
    { 0x0c0380, "Universal Serial Bus (no PI spec)" },
    { 0x0c03FE, "USB Target Device" },
    { 0x0c0400, "Fibre Channel" },
    { 0x0c0500, "System Management Bus" },
    { 0x0c0600, "InfiniBand" },
    { 0x0c0700, "IPMI SMIC Interface" },
    { 0x0c0701, "IPMI Kybd Controller Style Interface" },
    { 0x0c0702, "IPMI Block Transfer Interface" },
    // { 0x0c08xx , "SERCOS Interface" },
    { 0x0c0900, "CANbus" },

    { 0x0d100, "iRDA compatible controller" },
    { 0x0d100, "Consumer IR controller" },
    { 0x0d100, "RF controller" },
    { 0x0d100, "Bluetooth controller" },
    { 0x0d100, "Broadband controller" },
    { 0x0d100, "Ethernet (802.11a 5 GHz) controller" },
    { 0x0d100, "Ethernet (802.11b 2.4 GHz) controller" },
    { 0x0d100, "Wireless controller" },

    // { 0x0e00xx , "I2O Intelligent I/O, spec 1.0" },
    { 0x0e0000, "Message FIFO at offset 040h" },

    { 0x0f0100, "TV satellite comm. controller" },
    { 0x0f0200, "Audio satellite comm. controller" },
    { 0x0f0300, "Voice satellite comm. controller" },
    { 0x0f0400, "Data satellite comm. controller" },

    { 0x100000, "Network and computing en/decryption" },
    { 0x101000, "Entertainment en/decryption" },
    { 0x108000, "En/Decryption" },

    { 0x110000, "DPIO modules" },
    { 0x110100, "Perf. counters" },
    { 0x111000, "Comm. synch., time and freq. test" },
    { 0x112000, "Management card" },
    { 0x118000, "Data acq./Signal proc." },
};

/// @brief Enables configuration space access for PCI devices.
/// @details The configuration bit in I/O location CF8h[31] must be set to 1b to
/// enable access to PCI configuration space.
#define PCI_ADDR_ENABLE 0x80000000U

/// @brief Writes the bus number into bits [23:16] of I/O location CF8h.
/// @details This macro takes the PCI bus number and shifts it into the correct
/// position (bits [23:16]) for the PCI configuration address.
#define PCI_ADDR_BUS(bus) ((uint32_t)(bus) << 16U)

/// @brief Writes the PCI device number into bits [15:11] of I/O location CF8h.
/// @details This macro takes the PCI device number and shifts it into the
/// correct position (bits [15:11]) for the PCI configuration address.
#define PCI_ADDR_DEV(slot) ((uint32_t)(slot) << 11U)

/// @brief Writes the function number into bits [10:8] of I/O location CF8h.
/// @details This macro takes the PCI function number and shifts it into the
/// correct position (bits [10:8]) for the PCI configuration address.
#define PCI_ADDR_FUNC(func) ((uint32_t)(func) << 8U)

/// @brief Writes the register field into bits [7:2] of I/O location CF8h.
/// @details This macro takes the PCI register field and masks it to fit into
/// bits [7:2] for the PCI configuration address. This field is used to specify
/// the configuration register within the PCI device.
#define PCI_ADDR_FIELD(field) ((uint32_t)(field) & 0xFCU)

/// @brief Extracts the bus ID from the PCI address.
/// @details This macro extracts the bus number from bits [23:16] of the PCI
/// address.
#define PCI_GET_BUS(dev) (((dev) >> 16U) & 0xFF)

/// @brief Extracts the slot ID (device number) from the PCI address.
/// @details This macro extracts the slot (device) number from bits [15:8] of
/// the PCI address.
#define PCI_GET_SLOT(dev) (((dev) >> 11U) & 0x1F)

/// @brief Extracts the function ID from the PCI address.
/// @details This macro extracts the function number from bits [7:0] of the PCI
/// address.
#define PCI_GET_FUNC(dev) (((dev) >> 8U) & 0x07)

/// @brief Constructs the PCI configuration address for a given device and register field.
///
/// @param device The 32-bit value representing the PCI device (includes bus, slot, and function).
/// @param field The 32-bit value representing the register field (which register to access).
/// @param[out] addr Pointer to store the constructed PCI configuration address.
/// @return 0 on success, 1 on error.
static inline int pci_get_addr(uint32_t device, uint32_t field, uint32_t *addr)
{
    // Check if the output pointer is valid.
    if (addr == NULL) {
        pr_err("Output parameter 'addr' is NULL.\n");
        return 1;
    }
    // Extract bus, slot, and function numbers from the device identifier.
    uint8_t bus  = PCI_GET_BUS(device);
    uint8_t slot = PCI_GET_SLOT(device);
    uint8_t func = PCI_GET_FUNC(device);
    // Validate bus number (0-255)
    if (bus > 255) {
        pr_err("Invalid bus number %u. Must be between 0 and 255.\n", bus);
        return 1;
    }
    // Validate slot number (0-31).
    if (slot > 31) {
        pr_err("Invalid slot number %u. Must be between 0 and 31.\n", slot);
        return 1;
    }
    // Validate function number (0-7).
    if (func > 7) {
        pr_err("Invalid function number %u. Must be between 0 and 7.\n", func);
        return 1;
    }
    // Validate field (must be less than or equal to 0xFC and aligned to the access size).
    if (field > 0xFC) {
        pr_err("Invalid field 0x%X. Must be less than or equal to 0xFC.\n", field);
        return 1;
    }
    // Construct the PCI configuration address.
    *addr = PCI_ADDR_ENABLE |      // Enable access to PCI configuration space.
            PCI_ADDR_BUS(bus) |    // Set the bus number.
            PCI_ADDR_DEV(slot) |   // Set the slot (device) number.
            PCI_ADDR_FUNC(func) |  // Set the function number.
            PCI_ADDR_FIELD(field); // Set the register field.
    return 0;
}

int pci_write_8(uint32_t device, uint32_t field, uint8_t value)
{
    // Get the PCI configuration address
    uint32_t addr;
    if (pci_get_addr(device, field, &addr)) {
        return 1;
    }
    // Write the address to the PCI address port
    outportl(PCI_ADDRESS_PORT, addr);
    // Write the 8-bit value to the PCI data port with the adjusted field offset
    outportb(PCI_VALUE_PORT + (field & 0x03), value);
    return 0;
}

int pci_write_16(uint32_t device, uint32_t field, uint16_t value)
{
    // Get the PCI configuration address
    uint32_t addr;
    if (pci_get_addr(device, field, &addr)) {
        return 1;
    }
    // Write the address to the PCI address port.
    outportl(PCI_ADDRESS_PORT, addr);
    // Write the 16-bit value to the PCI value port with the adjusted field offset.
    outports(PCI_VALUE_PORT + (field & 0x02), value);
    return 0;
}

int pci_write_32(uint32_t device, uint32_t field, uint32_t value)
{
    // Get the PCI configuration address
    uint32_t addr;
    if (pci_get_addr(device, field, &addr)) {
        return 1;
    }
    // Write the address to the PCI address port.
    outportl(PCI_ADDRESS_PORT, addr);
    // Write the 32-bit value to the PCI value port.
    outportl(PCI_VALUE_PORT, value);
    return 0;
}

uint8_t pci_read_8(uint32_t device, int field)
{
    // Get the PCI configuration address
    uint32_t addr;
    if (pci_get_addr(device, field, &addr)) {
        return 0;
    }
    outportl(PCI_ADDRESS_PORT, addr);
    return inportb(PCI_VALUE_PORT + (field & 0x03));
}

uint16_t pci_read_16(uint32_t device, int field)
{
    // Get the PCI configuration address
    uint32_t addr;
    if (pci_get_addr(device, field, &addr)) {
        return 0;
    }
    outportl(PCI_ADDRESS_PORT, addr);
    return inports(PCI_VALUE_PORT + (field & 0x02));
}

uint32_t pci_read_32(uint32_t device, int field)
{
    // Get the PCI configuration address
    uint32_t addr;
    if (pci_get_addr(device, field, &addr)) {
        return 0;
    }
    outportl(PCI_ADDRESS_PORT, addr);
    return inportl(PCI_VALUE_PORT);
}

/// @brief Searches for the vendor name from the vendor ID.
/// @param vendor_id The vendor ID to search for.
/// @return The vendor name if found; otherwise, "Unknown".
static inline const char *pci_vendor_lookup(uint16_t vendor_id)
{
    for (size_t i = 0; i < count_of(_pci_vendors); ++i) {
        if (_pci_vendors[i].id == vendor_id) {
            return _pci_vendors[i].name;
        }
    }
    pr_err("Vendor ID %u not found.\n", vendor_id);
    return "Unknown";
}

/// @brief Searches for the device name from its ID and the vendor ID.
/// @param vendor_id The vendor ID to search for.
/// @param device_id The device ID to search for.
/// @return The device name if found; otherwise, "Unknown".
static inline const char *pci_device_lookup(uint16_t vendor_id, uint16_t device_id)
{
    for (size_t i = 0; i < count_of(_pci_devices); ++i) {
        if ((_pci_devices[i].ven_id == vendor_id) && (_pci_devices[i].dev_id == device_id)) {
            return _pci_devices[i].name;
        }
    }
    pr_err("Device with Vendor ID %u and Device ID %u not found.\n", vendor_id, device_id);
    return "Unknown";
}

/// @brief Retrieves the type name from a given type ID.
/// @param type_id The type ID to search for.
/// @return The corresponding type name if found; otherwise, "Unknown".
static inline const char *pci_type_lookup(uint32_t type_id)
{
    for (size_t i = 0; i < count_of(_pci_types); ++i) {
        if (_pci_types[i].id == type_id) {
            return _pci_types[i].name;
        }
    }
    pr_err("Type ID %u not found.\n", type_id);
    return "Unknown";
}

static inline int pci_box_device(uint8_t bus, uint8_t slot, uint8_t func, uint32_t *device);
static inline int pci_find_type(uint32_t device, uint32_t *device_type);
static inline int pci_scan_hit(pci_scan_func_t f, uint32_t device, void *extra);
static inline int pci_scan_func(pci_scan_func_t f, int type, uint8_t bus, uint8_t slot, uint8_t func, void *extra);
static inline int pci_scan_slot(pci_scan_func_t f, int type, uint8_t bus, uint8_t slot, void *extra);
static inline int pci_scan_bus(pci_scan_func_t f, int type, uint8_t bus, void *extra);

/// @brief Combines bus, slot, and function numbers into a 32-bit PCI device identifier.
/// @param bus The PCI bus number (8-bit value).
/// @param slot The PCI slot (device) number (8-bit value).
/// @param func The PCI function number (8-bit value).
/// @param[out] device Pointer to store the combined 32-bit PCI device identifier.
/// @return 0 on success, 1 on failure.
static inline int pci_box_device(uint8_t bus, uint8_t slot, uint8_t func, uint32_t *device)
{
    // Check if the output pointer is valid.
    if (device == NULL) {
        pr_err("Output parameter 'device' is NULL.\n");
        return 1;
    }
    // Validate slot number (0-31).
    if (slot > 31) {
        pr_err("Invalid slot number %u. Must be between 0 and 31.\n", slot);
        return 1;
    }
    // Validate function number (0-7).
    if (func > 7) {
        pr_err("Invalid function number %u. Must be between 0 and 7.\n", func);
        return 1;
    }
    // Pack the bus, slot, and function numbers into a single 32-bit value.
    *device = (uint32_t)((bus << 16U) | (slot << 8U) | func);
    return 0;
}

/// @brief Finds the type of the given PCI device.
/// @param device The 32-bit PCI device identifier (bus, slot, and function).
/// @param[out] device_type Pointer to store the device type (class, subclass, and programming interface).
/// @return 0 on success, 1 on failure.
static inline int pci_find_type(uint32_t device, uint32_t *device_type)
{
    // Check if the output pointer is valid
    if (device_type == NULL) {
        pr_err("Output parameter 'device_type' is NULL.\n");
        return 1;
    }
    uint8_t class_code;
    uint8_t subclass_code;
    uint8_t prog_if;
    // Read the class code.
    class_code = pci_read_8(device, PCI_CLASS);
    // Read the subclass code.
    subclass_code = pci_read_8(device, PCI_SUBCLASS);
    // Read the programming interface.
    prog_if = pci_read_8(device, PCI_PROG_IF);
    // Combine the class code, subclass code, and programming interface
    *device_type = (class_code << 16U) | (subclass_code << 8U) | prog_if;
    return 0;
}

/// @brief Calls the function f on the device if found.
/// @param f The function to call.
/// @param device The device number.
/// @param extra Extra arguments to pass to the function.
/// @return 0 on success, 1 on failure.
static inline int pci_scan_hit(pci_scan_func_t f, uint32_t device, void *extra)
{
    // Check if the function pointer f is valid
    if (f == NULL) {
        pr_err("Function pointer is NULL.\n");
        return 1;
    }

    uint16_t vendor_id;
    uint16_t device_id;
    // Check if the function pointer f is valid.
    if (f == NULL) {
        pr_err("Function pointer is NULL.\n");
        return 1;
    }
    // Read the vendor ID.
    vendor_id = pci_read_16(device, PCI_VENDOR_ID);
    // Read the device ID.
    device_id = pci_read_16(device, PCI_DEVICE_ID);
    // Call the provided function with the device information.
    f(device, vendor_id, device_id, extra);
    return 0;
}

/// @brief Scans for the given type of device.
/// @param f The function to call once we have found the device.
/// @param type The type of device we are searching for.
/// @param bus Bus number.
/// @param slot Slot number.
/// @param func Function number.
/// @param extra Extra arguments.
/// @return 0 on success, 1 on failure.
int pci_scan_func(pci_scan_func_t f, int type, uint8_t bus, uint8_t slot, uint8_t func, void *extra)
{
    // Check if the function pointer f is valid
    if (f == NULL) {
        pr_err("Function pointer is NULL.\n");
        return 1;
    }

    uint32_t device, device_type;
    uint8_t class_code, subclass_code, secondary_bus;

    // Obtain the device identifier.
    if (pci_box_device(bus, slot, func, &device)) {
        pr_err("Failed to obtain the device identifier.\n");
        return 1;
    }
    // Find the device type.
    if (pci_find_type(device, &device_type)) {
        pr_err("Failed to obtain the device type.\n");
        return 1;
    }
    // If the device type matches or if type == -1 (any type), call the
    // function.
    if ((type == -1) || (type == (int)device_type)) {
        if (pci_scan_hit(f, device, extra)) {
            pr_err("The function call failed.\n");
            return 1;
        }
    }
    // Extract class and subclass codes from device_type.
    class_code    = (device_type >> 16U) & 0xFF;
    subclass_code = (device_type >> 8U) & 0xFF;
    // If the device is a PCI bridge, scan the secondary bus
    if ((class_code == PCI_TYPE_BRIDGE) && (subclass_code == PCI_TYPE_SUBCLASS_PCI_BRIDGE)) {
        // Get the secondary bus.
        secondary_bus = pci_read_8(device, PCI_SECONDARY_BUS);

        // Recursively scan the secondary bus.
        if (pci_scan_bus(f, type, secondary_bus, extra)) {
            pr_err("Call to pci_scan_bus failed.\n");
            return 1;
        }
    }
    return 0;
}

/// @brief Scans all functions in a PCI slot for a given device type.
/// @param f The function to call once a device is found.
/// @param type The type of device we are searching for.
/// @param bus The bus number.
/// @param slot The slot number.
/// @param extra Extra arguments to pass to the function.
/// @return 0 on success, non-negative value if any errors occurred.
int pci_scan_slot(pci_scan_func_t f, int type, uint8_t bus, uint8_t slot, void *extra)
{
    // Check if the function pointer f is valid
    if (f == NULL) {
        pr_err("Function pointer is NULL.\n");
        return 1;
    }

    uint32_t device;
    uint16_t vendor_id;
    uint8_t header_type;

    // Obtain the device identifier.
    if (pci_box_device(bus, slot, 0, &device)) {
        pr_err("Failed to obtain the device identifier.\n");
        return 1;
    }

    // Scan function 0.
    if (pci_scan_func(f, type, bus, slot, 0, extra)) {
        pr_err("Failed to call scan function.\n");
        return 1; // Cannot proceed further.
    }

    // Read the header type to determine if the device is multi-function
    header_type = pci_read_8(device, PCI_HEADER_TYPE);

    // Check if the device is multi-function (bit 7 of header_type is set)
    if ((header_type & 0x80) != 0) {
        for (uint32_t func = 1; func < 8; func++) {
            // Obtain the device identifier for this function.
            if (pci_box_device(bus, slot, func, &device)) {
                pr_err("Failed to obtain the device identifier.\n");
                continue; // Skip to next function.
            }

            // Read the vendor ID.
            vendor_id = pci_read_16(device, PCI_VENDOR_ID);

            // Check if the device exists.
            if (vendor_id != PCI_NONE) {
                // Scan this function.
                pci_scan_func(f, type, bus, slot, func, extra);
            }
        }
    }
    return 0;
}

/// @brief Scans a PCI bus for devices of a given type.
/// @param f The function to call once we have found the device.
/// @param type The type of device we are searching for.
/// @param bus The bus number.
/// @param extra Extra arguments.
/// @return 0 on success, non-negative value indicating the number of errors.
int pci_scan_bus(pci_scan_func_t f, int type, uint8_t bus, void *extra)
{
    // Check if the function pointer f is valid.
    if (f == NULL) {
        pr_err("Function pointer is NULL.\n");
        return 1;
    }

    for (uint8_t slot = 0; slot < 32; ++slot) {
        pci_scan_slot(f, type, bus, slot, extra);
    }
    return 0;
}

int pci_scan(pci_scan_func_t f, int type, void *extra)
{
    // Check if the function pointer f is valid.
    if (f == NULL) {
        pr_err("Function pointer is NULL.\n");
        return 1;
    }

    uint8_t header_type;
    uint32_t device;
    uint16_t vendor_id;

    // Read the header type of bus 0, device 0, function 0
    header_type = pci_read_8(0, PCI_HEADER_TYPE);

    // Check if it is a single PCI host controller.
    if ((header_type & 0x80) == 0) {
        // Single PCI host controller; scan bus 0
        if (pci_scan_bus(f, type, 0, extra)) {
            return 1;
        }
    } else {
        for (uint8_t bus = 0; bus < 8; ++bus) {
            // Obtain the device identifier for this function.
            if (pci_box_device(bus, 0, 0, &device)) {
                pr_err("Failed to obtain the device identifier (slot: %u, func: %u).\n", 0, 0);
                continue; // Skip to next function.
            }

            // Read the vendor ID.
            vendor_id = pci_read_16(device, PCI_VENDOR_ID);

            // Check if the device exists.
            if (vendor_id != PCI_NONE) {
                // Scan this bus.
                pci_scan_bus(f, type, bus, extra);
            }
        }
    }
    return 0;
}

/// @brief Callback function to find an ISA bridge device.
/// @param device The PCI device identifier.
/// @param vendor_id The vendor ID of the device.
/// @param device_id The device ID of the device.
/// @param extra Pointer to store the device identifier if a matching device is found.
/// @return 1 if a matching device is found, 0 if not, -1 on error.
static int find_isa_bridge(uint32_t device, uint16_t vendor_id, uint16_t device_id, void *extra)
{
    // Check if the output pointer 'extra' is valid
    if (extra == NULL) {
        pr_err("Output parameter 'extra' is NULL.\n");
        return 1;
    }
    // Check if the device matches the specified vendor and device IDs
    if (vendor_id == 0x8086 && (device_id == 0x7000 || device_id == 0x7110)) {
        // Store the device identifier in the location pointed to by 'extra'
        *((uint32_t *)extra) = device;
        return 1; // Matching device found
    }
    return 0; // No matching device found
}

static uint32_t pci_isa       = 0;
static uint32_t pci_remaps[4] = { 0 };

/// @brief Remaps PCI-to-ISA interrupts.
/// @return 0 on success, non-zero on failure.
static inline int pci_remap(void)
{
    // Scan for the ISA bridge device.
    if (pci_scan(&find_isa_bridge, -1, &pci_isa)) {
        pr_err("Failed to scan for ISA bridge.\n");
        return 1;
    }
    // Check if the ISA bridge device was found.
    if (!pci_isa) {
        return 1;
    }

    pr_default("PCI-to-ISA interrupt mappings by line:\n");

    for (int i = 0; i < 4; ++i) {
        pci_remaps[i] = pci_read_8(pci_isa, 0x60 + i);
        pr_default("\tLine %d: 0x%2x\n", i + 1, pci_remaps[i]);
    }

    uint32_t out = 0;
    memcpy(&out, &pci_remaps, 4);

    // Write the updated interrupt mappings back to the device.
    if (pci_write_32(pci_isa, 0x60, out)) {
        pr_err("Failed to write updated interrupt mappings.\n");
        return 1;
    }

    return 0;
}

int pci_get_interrupt(uint32_t device)
{
    if (pci_isa == 0) {
        return pci_read_8(device, PCI_INTERRUPT_LINE);
    }

    uint32_t irq_pin = pci_read_8(device, PCI_INTERRUPT_PIN);

    if (irq_pin == 0) {
        pr_default("PCI device does not specific interrupt line\n");
        return pci_read_8(device, PCI_INTERRUPT_LINE);
    }

    int pirq          = (irq_pin + PCI_GET_SLOT(device) - 2) % 4;
    uint32_t int_line = pci_read_8(device, PCI_INTERRUPT_LINE);
    pr_default("Slot is %d, irq pin is %d, so pirq is %d and that maps to %d?"
               "int_line=%d\n",
               PCI_GET_SLOT(device), irq_pin, pirq, pci_remaps[pirq],
               int_line);
    if (pci_remaps[pirq] == 0x80) {
        pr_default("Not mapped, remapping?\n");
        pci_remaps[pirq] = int_line;
        uint32_t out     = 0;
        memcpy(&out, &pci_remaps, 4);
        pci_write_32(pci_isa, 0x60, out);
        return pci_read_8(device, PCI_INTERRUPT_LINE);
    }
    return pci_remaps[pirq];
}

void pci_dump_device_data(uint32_t device, uint16_t vendorid, uint16_t deviceid)
{
    uint8_t bus = PCI_GET_BUS(device), slot = PCI_GET_SLOT(device), func = PCI_GET_FUNC(device);
    pr_debug("%2x:%x.%d (%s, %s)\n",
             bus, slot, func,
             pci_vendor_lookup(vendorid),
             pci_device_lookup(vendorid, deviceid));
    pr_debug("    %-12s: %s\n",
             "Type", pci_type_lookup(pci_find_type(device)),
             "Command", pci_read_16(device, PCI_COMMAND));
    pr_debug("    %-12s: %8x, %-12s: %8x\n",
             "Status", pci_read_16(device, PCI_STATUS),
             "Command", pci_read_16(device, PCI_COMMAND));
    pr_debug("    %-12s: %08x, %-12s: %08x, %-12s: %08x\n",
             "BAR0", pci_read_32(device, PCI_BASE_ADDRESS_0),
             "BAR1", pci_read_32(device, PCI_BASE_ADDRESS_1),
             "BAR2", pci_read_32(device, PCI_BASE_ADDRESS_2));
    pr_debug("    %-12s: %08x, %-12s: %08x, %-12s: %08x\n",
             "BAR3", pci_read_32(device, PCI_BASE_ADDRESS_3),
             "BAR4", pci_read_32(device, PCI_BASE_ADDRESS_4),
             "BAR5", pci_read_32(device, PCI_BASE_ADDRESS_5));
    pr_debug("    %-12s: %8d, %-12s: %8d, %-12s: %8d\n",
             "Int. Ping", pci_read_8(device, PCI_INTERRUPT_PIN),
             "Line", pci_read_8(device, PCI_INTERRUPT_LINE),
             "Number", pci_get_interrupt(device));
    pr_debug("    %-12s: %8d, %-12s: %8d, %-12s: %8d\n",
             "Revision", pci_read_8(device, PCI_REVISION_ID),
             "Cache L. Sz.", pci_read_8(device, PCI_CACHE_LINE_SIZE),
             "Latency Tmr.", pci_read_8(device, PCI_LATENCY_TIMER));
    pr_debug("    %-12s: %8d, %-12s: %8d, %-12s: %8d\n",
             "Header Type", pci_read_8(device, PCI_HEADER_TYPE),
             "BIST", pci_read_8(device, PCI_BIST),
             "Cardbus CIS", pci_read_8(device, PCI_CARDBUS_CIS));
#if 0
    // pr_default(" Subsystem V. ID : 0x%08x\n", pci_read_field(device, PCI_SUBSYSTEM_VENDOR_ID, 2));
    // pr_default(" Subsystem ID    : 0x%08x\n", pci_read_field(device, PCI_SUBSYSTEM_ID, 2));
    // pr_default(" ROM Base Address: 0x%08x\n", pci_read_field(device, PCI_ROM_ADDRESS, 4));
    // pr_default(" PCI Cp. LinkList: 0x%08x\n", pci_read_field(device, PCI_CAPABILITY_LIST, 1));
    // pr_default(" Max Latency     : 0x%08x\n", pci_read_field(device, PCI_MAX_LAT, 1));
    // pr_default(" Min Grant       : 0x%08x\n", pci_read_field(device, PCI_MIN_GNT, 1));
    // pr_default(" Interrupt Pin   : %2d\n", pci_read_field(device, PCI_INTERRUPT_PIN, 1));
    // pr_default(" Interrupt Line  : %2d\n", pci_read_field(device, PCI_INTERRUPT_LINE, 1));
    // pr_default(" Interrupt Number: %2d\n", pci_get_interrupt(device));
    // pr_default("\n");
#endif
}

/// @brief Callback function to count PCI devices during scanning.
/// @param device The PCI device identifier.
/// @param vendor_id The vendor ID of the device.
/// @param device_id The device ID of the device.
/// @param extra Pointer to a size_t variable to store the count.
/// @return 0 on success, non-zero on failure.
static int __scan_count(uint32_t device, uint16_t vendor_id, uint16_t device_id, void *extra)
{
    (void)device;
    (void)vendor_id;
    (void)device_id;
    // Check if the output pointer 'extra' is valid
    if (extra == NULL) {
        pr_err("Output parameter 'extra' is NULL.\n");
        return -1;
    }
    // Increment the count
    ++(*(size_t *)extra);
    return 0;
}

/// @brief Callback function to process and display PCI device data during scanning.
/// @param device The PCI device identifier.
/// @param vendor_id The vendor ID of the device.
/// @param device_id The device ID of the device.
/// @param extra Unused parameter.
/// @return 0 on success, non-zero on failure.
static int __scan_hit_list(uint32_t device, uint16_t vendor_id, uint16_t device_id, void *extra)
{
    (void)extra;

    // Process and display PCI device data
    pci_dump_device_data(device, vendor_id, device_id);

    pr_debug("\n");

    return 0;
}

void pci_debug_scan(void)
{
    pr_default("\n--------------------------------------------------\n");
    pr_default("Counting PCI entities...\n");
    size_t count = 0;
    pci_scan(&__scan_count, -1, &count);
    pr_default("Total PCI entities: %d\n", count);

    pr_default("Scanning PCI entities...\n");
    pci_scan(&__scan_hit_list, -1, NULL);

    pr_default("Mapping PCI entities...\n");
    pci_remap();

    pr_default("--------------------------------------------------\n");
}

///! @endcond
