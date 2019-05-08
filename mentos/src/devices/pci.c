///                MentOS, The Mentoring Operating system project
/// @file pci.c
/// @brief Routines for PCI initialization.
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "pci.h"
#include "debug.h"
#include "stdio.h"
#include "kheap.h"
#include "string.h"
#include "port_io.h"

void pci_write_field(uint32_t device, int field, int size, uint32_t value)
{
	(void)size;
	outportl(PCI_ADDRESS_PORT, pci_get_addr(device, field));
#if 0
    if (size == 4)
        outportl(PCI_VALUE_PORT, value);
    else if (size == 2)
        outports(PCI_VALUE_PORT, value);
    else if (size == 1)
        outportb(PCI_VALUE_PORT, value);
#else
	outportl(PCI_VALUE_PORT, value);
#endif
}

uint32_t pci_read_field(uint32_t device, int field, int size)
{
	outportl(PCI_ADDRESS_PORT, pci_get_addr(device, field));

	if (size == 4) {
		return inportl(PCI_VALUE_PORT);
	} else if (size == 2) {
		return inports(PCI_VALUE_PORT + (field & 2));
	} else if (size == 1) {
		return inportb(PCI_VALUE_PORT + (field & 3));
	}
	return 0xFFFF;
}

uint32_t pci_find_type(uint32_t device)
{
	return pci_read_field(device, PCI_CLASS, 1) << 16 |
	       pci_read_field(device, PCI_SUBCLASS, 1) << 8 |
	       pci_read_field(device, PCI_PROG_IF, 1);
}

struct {
	uint16_t id;
	const char *name;
} _pci_vendors[] = {
	{ 0x1022, "AMD" },	{ 0x106b, "Apple, Inc." },
	{ 0x1234, "Bochs/QEMU" }, { 0x1274, "Ensoniq" },
	{ 0x15ad, "VMWare" },     { 0x8086, "Intel Corporation" },
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
} _pci_classes[] = {
	{ 0x000000, "Legacy Device" },
	{ 0x000100, "VGA-Compatible Device" },

	{ 0x010000, "SCSI bus controller" },
	{ 0x010100, "ISA Compatibility mode-only controller" },
	{ 0x010105, "PCI native mode-only controller" },
	{ 0x01010a, "ISA Compatibility mode controller, supports both channels "
		    "switched to PCI native mode" },
	{ 0x01010f,
	  "PCI native mode controller, supports both channels switched "
	  "to ISA compatibility mode" },
	{ 0x010180,
	  "ISA Compatibility mode-only controller, supports bus mastering" },
	{ 0x010185, "PCI native mode-only controller, supports bus mastering" },
	{ 0x01018a, "ISA Compatibility mode controller, supports both channels "
		    "switched to PCI native mode, supports bus mastering" },
	{ 0x01018f,
	  "PCI native mode controller, supports both channels switched "
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
	{ 0x060940,
	  "PCI-to-PCI bridge, Semi-transparent, primary facing Host" },
	{ 0x060980,
	  "PCI-to-PCI bridge, Semi-transparent, secondary facing Host" },
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

const char *pci_vendor_lookup(unsigned short vendor_id)
{
	for (int i = 0; i < sizeof(_pci_vendors) / sizeof(_pci_vendors[0]);
	     ++i) {
		if (_pci_vendors[i].id == vendor_id) {
			return _pci_vendors[i].name;
		}
	}

	return "Unknown";
}

const char *pci_device_lookup(unsigned short vendor_id,
			      unsigned short device_id)
{
	for (int i = 0; i < sizeof(_pci_devices) / sizeof(_pci_devices[0]);
	     ++i) {
		if (_pci_devices[i].ven_id == vendor_id &&
		    _pci_devices[i].dev_id == device_id) {
			return _pci_devices[i].name;
		}
	}

	return "Unknown";
}

const char *pci_class_lookup(uint32_t class_code)
{
	for (int i = 0; i < sizeof(_pci_classes) / sizeof(_pci_classes[0]);
	     ++i) {
		if (_pci_classes[i].id == class_code) {
			return _pci_classes[i].name;
		}
	}

	return "Unknown";
}

void pci_scan_hit(pci_func_t f, uint32_t dev, void *extra)
{
	uint16_t dev_vend = (uint16_t)pci_read_field(dev, PCI_VENDOR_ID, 2);
	uint16_t dev_dvid = (uint16_t)pci_read_field(dev, PCI_DEVICE_ID, 2);
	f(dev, dev_vend, dev_dvid, extra);
}

void pci_scan_func(pci_func_t f, int type, int bus, int slot, int func,
		   void *extra)
{
	uint32_t dev = pci_box_device(bus, slot, func);

	if ((type == -1) || (type == pci_find_type(dev))) {
		pci_scan_hit(f, dev, extra);
	}
	if (pci_find_type(dev) == PCI_TYPE_BRIDGE) {
		pci_scan_bus(f, type, pci_read_field(dev, PCI_SECONDARY_BUS, 1),
			     extra);
	}
}

void pci_scan_slot(pci_func_t f, int type, int bus, int slot, void *extra)
{
	uint32_t dev = pci_box_device(bus, slot, 0);

	if (pci_read_field(dev, PCI_VENDOR_ID, 2) == PCI_NONE) {
		return;
	}

	pci_scan_func(f, type, bus, slot, 0, extra);

	if (!pci_read_field(dev, PCI_HEADER_TYPE, 1)) {
		return;
	}

	for (int func = 1; func < 8; func++) {
		dev = pci_box_device(bus, slot, func);

		if (pci_read_field(dev, PCI_VENDOR_ID, 2) != PCI_NONE) {
			pci_scan_func(f, type, bus, slot, func, extra);
		}
	}
}

void pci_scan_bus(pci_func_t f, int type, int bus, void *extra)
{
	for (int slot = 0; slot < 32; ++slot) {
		pci_scan_slot(f, type, bus, slot, extra);
	}
}

void pci_scan(pci_func_t f, int type, void *extra)
{
	if ((pci_read_field(0, PCI_HEADER_TYPE, 1) & 0x80) == 0) {
		pci_scan_bus(f, type, 0, extra);
		return;
	}

	for (int func = 0; func < 8; ++func) {
		uint32_t dev = pci_box_device(0, 0, func);

		if (pci_read_field(dev, PCI_VENDOR_ID, 2) == PCI_NONE) {
			break;
		}
		pci_scan_bus(f, type, func, extra);
	}
}

static void find_isa_bridge(uint32_t device, uint16_t vendorid,
			    uint16_t deviceid, void *extra)
{
	if (vendorid == 0x8086 && (deviceid == 0x7000 || deviceid == 0x7110)) {
		*((uint32_t *)extra) = device;
	}
}

static uint32_t pci_isa = 0;
static uint32_t pci_remaps[4] = { 0 };

void pci_remap()
{
	pci_scan(&find_isa_bridge, -1, &pci_isa);

	if (pci_isa) {
		dbg_print("PCI-to-ISA interrupt mappings by line:\n");

		for (int i = 0; i < 4; ++i) {
			pci_remaps[i] = pci_read_field(pci_isa, 0x60 + i, 1);
			dbg_print("\tLine %d: 0x%2x\n", i + 1, pci_remaps[i]);
		}

		uint32_t out = 0;
		memcpy(&out, &pci_remaps, 4);
		pci_write_field(pci_isa, 0x60, 4, out);
	}
}

int pci_get_interrupt(uint32_t device)
{
	if (pci_isa == 0) {
		return pci_read_field(device, PCI_INTERRUPT_LINE, 1);
	}

	uint32_t irq_pin = pci_read_field(device, PCI_INTERRUPT_PIN, 1);

	if (irq_pin == 0) {
		dbg_print("PCI device does not specific interrupt line\n");
		return pci_read_field(device, PCI_INTERRUPT_LINE, 1);
	}

	int pirq = (irq_pin + pci_extract_slot(device) - 2) % 4;

	uint32_t int_line = pci_read_field(device, PCI_INTERRUPT_LINE, 1);

	dbg_print(
		"Slot is %d, irq pin is %d, so pirq is %d and that maps to %d?"
		"int_line=%d\n",
		pci_extract_slot(device), irq_pin, pirq, pci_remaps[pirq],
		int_line);

	if (pci_remaps[pirq] == 0x80) {
		dbg_print("Not mapped, remapping?\n");
		pci_remaps[pirq] = int_line;
		uint32_t out = 0;
		memcpy(&out, &pci_remaps, 4);
		pci_write_field(pci_isa, 0x60, 4, out);
		return pci_read_field(device, PCI_INTERRUPT_LINE, 1);
	}

	return pci_remaps[pirq];
}

static void scan_count(uint32_t device, uint16_t vendorid, uint16_t deviceid,
		       void *extra)
{
	(void)device;
	(void)vendorid;
	(void)deviceid;
	size_t *count = extra;
	++(*count);
}

static void scan_hit_list(uint32_t device, uint16_t vendorid, uint16_t deviceid,
			  void *extra)
{
	(void)extra;
	dbg_print("%2x:%2x.%d\n", pci_extract_bus(device),
		  pci_extract_slot(device), pci_extract_func(device), deviceid);
	dbg_print(" Vendor          : [0x%06x] %s\n", vendorid,
		  pci_vendor_lookup(vendorid));
	dbg_print(" Device          : [0x%06x] %s\n", deviceid,
		  pci_device_lookup(vendorid, deviceid));
	dbg_print(" Type            : [0x%06x] %s\n", pci_find_type(device),
		  pci_class_lookup(pci_find_type(device)));
	dbg_print(" Status          : 0x%4x\n",
		  pci_read_field(device, PCI_STATUS, 2));
	dbg_print(" Command         : 0x%4x\n",
		  pci_read_field(device, PCI_COMMAND, 2));
	dbg_print(" Revision        : %2d\n",
		  pci_read_field(device, PCI_REVISION_ID, 1));
	dbg_print(" Cache Line Size : %2d\n",
		  pci_read_field(device, PCI_CACHE_LINE_SIZE, 1));
	dbg_print(" Latency Timer   : %2d\n",
		  pci_read_field(device, PCI_LATENCY_TIMER, 1));
	dbg_print(" Header Type     : %2d\n",
		  pci_read_field(device, PCI_HEADER_TYPE, 1));
	dbg_print(" BIST            : %2d\n",
		  pci_read_field(device, PCI_BIST, 1));
	dbg_print(" BAR0 : 0x%08x",
		  pci_read_field(device, PCI_BASE_ADDRESS_0, 4));
	dbg_print(" BAR1 : 0x%08x",
		  pci_read_field(device, PCI_BASE_ADDRESS_1, 4));
	dbg_print(" BAR2 : 0x%08x\n",
		  pci_read_field(device, PCI_BASE_ADDRESS_2, 4));
	dbg_print(" BAR3 : 0x%08x",
		  pci_read_field(device, PCI_BASE_ADDRESS_3, 4));
	dbg_print(" BAR4 : 0x%08x",
		  pci_read_field(device, PCI_BASE_ADDRESS_4, 4));
	dbg_print(" BAR6 : 0x%08x\n",
		  pci_read_field(device, PCI_BASE_ADDRESS_5, 4));
	dbg_print(" Cardbus CIS     : 0x%08x\n",
		  pci_read_field(device, PCI_CARDBUS_CIS, 4));
	dbg_print(" Subsystem V. ID : 0x%08x\n",
		  pci_read_field(device, PCI_SUBSYSTEM_VENDOR_ID, 2));
	dbg_print(" Subsystem ID    : 0x%08x\n",
		  pci_read_field(device, PCI_SUBSYSTEM_ID, 2));
	dbg_print(" ROM Base Address: 0x%08x\n",
		  pci_read_field(device, PCI_ROM_ADDRESS, 4));
	dbg_print(" PCI Cp. LinkList: 0x%08x\n",
		  pci_read_field(device, PCI_CAPABILITY_LIST, 1));
	dbg_print(" Max Latency     : 0x%08x\n",
		  pci_read_field(device, PCI_MAX_LAT, 1));
	dbg_print(" Min Grant       : 0x%08x\n",
		  pci_read_field(device, PCI_MIN_GNT, 1));
	dbg_print(" Interrupt Pin   : %2d\n",
		  pci_read_field(device, PCI_INTERRUPT_PIN, 1));
	dbg_print(" Interrupt Line  : %2d\n",
		  pci_read_field(device, PCI_INTERRUPT_LINE, 1));
	dbg_print(" Interrupt Number: %2d\n", pci_get_interrupt(device));
	dbg_print("\n");
}

void pci_debug_scan()
{
	dbg_print("\n--------------------------------------------------\n");
	dbg_print("Counting PCI entities...\n");
	size_t count = 0;
	pci_scan(&scan_count, -1, &count);
	dbg_print("Total PCI entities: %d\n", count);

	dbg_print("Scanning PCI entities...\n");
	pci_scan(&scan_hit_list, -1, NULL);

	dbg_print("Mapping PCI entities...\n");
	pci_remap();

	dbg_print("--------------------------------------------------\n");
}
