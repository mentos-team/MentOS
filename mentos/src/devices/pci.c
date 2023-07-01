/// @file pci.c
/// @brief Routines for PCI initialization.
/// @copyright (c) 2014-2023 This file is distributed under the MIT License.
/// See LICENSE.md for details.
///! @cond Doxygen_Suppress

// Setup the logging for this file (do this before any other include).
#include "sys/kernel_levels.h"          // Include kernel log levels.
#define __DEBUG_HEADER__ "[PCI   ]"     ///< Change header.
#define __DEBUG_LEVEL__  LOGLEVEL_DEBUG ///< Set log level.
#include "io/debug.h"                   // Include debugging functions.

#include "devices/pci.h"
#include "io/port_io.h"
#include "string.h"

/// The configuration bit in I/O location CF8h[31] must be set to 1b.
#define PCI_ADDR_ENABLE 0x80000000
/// The device's bus number must be written into bits [23:16] of I/O location CF8h.
#define PCI_ADDR_BUS(x) (((uint32_t)(x)&0xFF) << 16)
/// The device's "PCI device number" must be written into bits [15:11] of I/O location CF8h.
#define PCI_ADDR_DEV(x) (((uint32_t)(x)&0x1F) << 11)
/// The device's function must be written into bits [10:8] of I/O location CF8h.
#define PCI_ADDR_FUNC(x) (((uint32_t)(x)&0x03) << 8)
///
#define PCI_ADDR_FIELD(x) (((uint32_t)(x)&0xFC))

/// @brief Extracts the bus id from the device.
#define PCI_GET_BUS(x) ((uint8_t)((x) >> 16U))
/// @brief Extracts the slot id from the device.
#define PCI_GET_SLOT(x) ((uint8_t)((x) >> 8U))
/// @brief Extracts the function id from the device.
#define PCI_GET_FUNC(x) ((uint8_t)((x)))

/// @brief TODO: Comment.
/// @param device
/// @param field
/// @return
static inline uint32_t pci_get_addr(uint32_t device, uint32_t field)
{
    return PCI_ADDR_ENABLE |
           PCI_ADDR_BUS(PCI_GET_BUS(device)) |
           PCI_ADDR_DEV(PCI_GET_SLOT(device)) |
           PCI_ADDR_FUNC(PCI_GET_FUNC(device)) |
           PCI_ADDR_FIELD(field);
}

static inline uint32_t pci_box_device(uint8_t bus, uint8_t slot, uint8_t func)
{
    return (uint32_t)((bus << 16U) | (slot << 8U) | func);
}

void pci_write_8(uint32_t device, uint32_t field, uint8_t value)
{
    outportl(PCI_ADDRESS_PORT, pci_get_addr(device, field));
    outportb(PCI_VALUE_PORT + (field & 0x03), value);
}

void pci_write_16(uint32_t device, uint32_t field, uint16_t value)
{
    outportl(PCI_ADDRESS_PORT, pci_get_addr(device, field));
    outports(PCI_VALUE_PORT + (field & 0x02), value);
}

void pci_write_32(uint32_t device, uint32_t field, uint32_t value)
{
    outportl(PCI_ADDRESS_PORT, pci_get_addr(device, field));
    outportl(PCI_VALUE_PORT, value);
}

uint8_t pci_read_8(uint32_t device, int field)
{
    outportl(PCI_ADDRESS_PORT, pci_get_addr(device, field));
    return inportb(PCI_VALUE_PORT + (field & 0x03));
}

uint16_t pci_read_16(uint32_t device, int field)
{
    outportl(PCI_ADDRESS_PORT, pci_get_addr(device, field));
    return inports(PCI_VALUE_PORT + (field & 0x02));
}

uint32_t pci_read_32(uint32_t device, int field)
{
    outportl(PCI_ADDRESS_PORT, pci_get_addr(device, field));
    return inportl(PCI_VALUE_PORT);
}

/// @brief Finds the type of the given device.
/// @param device the device number.
/// @return the type of the device.
static inline uint32_t pci_find_type(uint32_t device)
{
    return pci_read_8(device, PCI_CLASS) << 16U |
           pci_read_8(device, PCI_SUBCLASS) << 8U |
           pci_read_8(device, PCI_PROG_IF);
}

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

/// @brief Searches for the vendor name from the ID.
/// @param vendor_id the vendor ID.
/// @return the vendor name.
static inline const char *pci_vendor_lookup(uint16_t vendor_id)
{
    for (size_t i = 0; i < count_of(_pci_vendors); ++i) {
        if (_pci_vendors[i].id == vendor_id) {
            return _pci_vendors[i].name;
        }
    }
    return "Unknown";
}

/// @brief Searches for the device name from its ID and the vendor id.
/// @param vendor_id the vendor ID.
/// @param device_id the device ID.
/// @return the device name.
static inline const char *pci_device_lookup(uint16_t vendor_id, uint16_t device_id)
{
    for (size_t i = 0; i < count_of(_pci_devices); ++i) {
        if ((_pci_devices[i].ven_id == vendor_id) && (_pci_devices[i].dev_id == device_id)) {
            return _pci_devices[i].name;
        }
    }
    return "Unknown";
}

static inline const char *pci_type_lookup(uint32_t type_id)
{
    for (size_t i = 0; i < count_of(_pci_types); ++i) {
        if (_pci_types[i].id == type_id) {
            return _pci_types[i].name;
        }
    }
    return "Unknown";
}

void pci_scan_bus(pci_scan_func_t f, int type, uint8_t bus, void *extra);

/// @brief Calls the function f on the device if found.
/// @param f the function to call.
/// @param device the device number.
/// @param extra the extra arguemnts.
void pci_scan_hit(pci_scan_func_t f, uint32_t device, void *extra)
{
    uint16_t vendor_id = pci_read_16(device, PCI_VENDOR_ID);
    uint16_t device_id = pci_read_16(device, PCI_DEVICE_ID);
    f(device, vendor_id, device_id, extra);
}

/// @brief Scans for the given type of device.
/// @param f the function to call once we have found the device.
/// @param type the type of device we are searching for.
/// @param bus bus number.
/// @param slot slot number.
/// @param func choose a specific function in a device.
/// @param extra the extra arguemnts.
void pci_scan_func(pci_scan_func_t f, int type, uint8_t bus, uint8_t slot, uint8_t func, void *extra)
{
    uint32_t device      = pci_box_device(bus, slot, func);
    uint32_t device_type = pci_find_type(device);
    if ((type == -1) || (type == device_type)) {
        pci_scan_hit(f, device, extra);
    }
    if (device_type == PCI_TYPE_BRIDGE) {
        pci_scan_bus(f, type, pci_read_8(device, PCI_SECONDARY_BUS), extra);
    }
}

/// @brief Scans for the given type of device.
/// @param f the function to call once we have found the device.
/// @param type the type of device we are searching for.
/// @param bus bus number.
/// @param slot slot number.
/// @param extra the extra arguemnts.
void pci_scan_slot(pci_scan_func_t f, int type, uint8_t bus, uint8_t slot, void *extra)
{
    uint32_t device = pci_box_device(bus, slot, 0);
    pci_scan_func(f, type, bus, slot, 0, extra);
    if (pci_read_8(device, PCI_HEADER_TYPE)) {
        for (uint32_t func = 1; func < 8; func++) {
            device = pci_box_device(bus, slot, func);
            if (pci_read_16(device, PCI_VENDOR_ID) != PCI_NONE) {
                pci_scan_func(f, type, bus, slot, func, extra);
            }
        }
    }
}

/// @brief Scans for the given type of device.
/// @param f the function to call once we have found the device.
/// @param type the type of device we are searching for.
/// @param bus bus number.
/// @param extra the extra arguemnts.
void pci_scan_bus(pci_scan_func_t f, int type, uint8_t bus, void *extra)
{
    for (uint8_t slot = 0; slot < 32; ++slot) {
        pci_scan_slot(f, type, bus, slot, extra);
    }
}

void pci_scan(pci_scan_func_t f, int type, void *extra)
{
    // Single PCI host controller.
    if ((pci_read_8(0, PCI_HEADER_TYPE) & 0x80) == 0) {
        pci_scan_bus(f, type, 0, extra);
    } else {
        for (uint8_t bus = 0; bus < 8; ++bus) {
            uint32_t device = pci_box_device(bus, 0, 0);
            if (pci_read_16(device, PCI_VENDOR_ID) != PCI_NONE) {
                pci_scan_bus(f, type, bus, extra);
            }
        }
    }
}

static void find_isa_bridge(uint32_t device, uint16_t vendorid, uint16_t deviceid, void *extra)
{
    if (vendorid == 0x8086 && (deviceid == 0x7000 || deviceid == 0x7110)) {
        *((uint32_t *)extra) = device;
    }
}

static uint32_t pci_isa       = 0;
static uint32_t pci_remaps[4] = { 0 };

void pci_remap(void)
{
    pci_scan(&find_isa_bridge, -1, &pci_isa);

    if (pci_isa) {
        pr_default("PCI-to-ISA interrupt mappings by line:\n");

        for (int i = 0; i < 4; ++i) {
            pci_remaps[i] = pci_read_8(pci_isa, 0x60 + i);
            pr_default("\tLine %d: 0x%2x\n", i + 1, pci_remaps[i]);
        }

        uint32_t out = 0;
        memcpy(&out, &pci_remaps, 4);
        pci_write_32(pci_isa, 0x60, out);
    }
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

static void __scan_count(uint32_t device, uint16_t vendorid, uint16_t deviceid, void *extra)
{
    (void)device;
    (void)vendorid;
    (void)deviceid;
    size_t *count = extra;
    ++(*count);
}

static void __scan_hit_list(uint32_t device, uint16_t vendorid, uint16_t deviceid, void *extra)
{
    (void)extra;
    pci_dump_device_data(device, vendorid, deviceid);
    pr_debug("\n");
}

void pci_debug_scan()
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
