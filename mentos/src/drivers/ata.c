///                MentOS, The Mentoring Operating system project
/// @file ata.c
/// @brief
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "ata.h"
#include "isr.h"
#include "vfs.h"
#include "pci.h"
#include "list.h"
#include "debug.h"
#include "stdio.h"
#include "kheap.h"
#include "assert.h"
#include "string.h"
#include "kernel.h"
#include "pic8259.h"
#include "spinlock.h"

// #define COMPLETE_VFS
// #define COMPLETE_SCHEDULER

uint32_t ata_pci = 0x00000000;

list_t *atapi_waiter;

bool_t atapi_in_progress = false;

typedef union {
	uint8_t command_bytes[12];
	uint16_t command_words[6];
} atapi_command_t;

typedef struct {
	uintptr_t offset;
	uint16_t bytes;
	uint16_t last;
} prdt_t;

typedef struct {
	char name[256];
	int io_base;
	int control;
	int slave;
	bool_t is_atapi;
	ata_identify_t identity;
	prdt_t *dma_prdt;
	uintptr_t dma_prdt_phys;
	uint8_t *dma_start;
	uintptr_t dma_start_phys;
	uint32_t bar4;
	uint32_t atapi_lba;
	uint32_t atapi_sector_size;
} ata_device_t;

ata_device_t ata_primary_master = { .io_base = 0x1F0,
									.control = 0x3F6,
									.slave = 0 };
ata_device_t ata_primary_slave = { .io_base = 0x1F0,
								   .control = 0x3F6,
								   .slave = 1 };
ata_device_t ata_secondary_master = { .io_base = 0x170,
									  .control = 0x376,
									  .slave = 0 };
ata_device_t ata_secondary_slave = { .io_base = 0x170,
									 .control = 0x376,
									 .slave = 1 };

// volatile uint8_t ata_lock = 0;
spinlock_t ata_lock = { 0 };

// TODO: support other sector sizes.
#define ATA_SECTOR_SIZE 512

/// @brief Waits for the
/// @param dev
void ata_io_wait(ata_device_t *dev)
{
	inportb(dev->io_base + ATA_REG_ALTSTATUS);
	inportb(dev->io_base + ATA_REG_ALTSTATUS);
	inportb(dev->io_base + ATA_REG_ALTSTATUS);
	inportb(dev->io_base + ATA_REG_ALTSTATUS);
	inportb(dev->io_base + ATA_REG_ALTSTATUS);
}

uint8_t ata_status_wait(ata_device_t *dev, int timeout)
{
	uint8_t status;
	if (timeout > 0) {
		for (int i = 0; (status = inportb(dev->io_base + ATA_REG_STATUS)) &
							ATA_STAT_BUSY &&
						(i < timeout);
			 ++i)
			;
	} else {
		while ((status = inportb(dev->io_base + ATA_REG_STATUS)) &
			   ATA_STAT_BUSY)
			;
	}

	return status;
}

int ata_wait(ata_device_t *dev, bool_t advanced)
{
	ata_io_wait(dev);
	uint8_t status = ata_status_wait(dev, 0);
	if (advanced) {
		status = inportb(dev->io_base + ATA_REG_STATUS);
		if (status & ATA_STAT_ERR)
			return 1;
		if (status & ATA_STAT_FAULT)
			return 1;
		if (!(status & ATA_STAT_DRQ))
			return 1;
	}

	return 0;
}

void ata_device_select(ata_device_t *dev)
{
	outportb(dev->io_base + 1, 1);
	outportb(dev->control, 0);
	outportb(dev->io_base + ATA_REG_HDDEVSEL, 0xA0 | dev->slave << 4);
	ata_io_wait(dev);
}

void ata_device_read_sector(ata_device_t *dev, uint32_t lba, uint8_t *buf);

void ata_device_read_sector_atapi(ata_device_t *dev, uint32_t lba,
								  uint8_t *buf);

void ata_device_write_sector_retry(ata_device_t *dev, uint32_t lba,
								   uint8_t *buf);

#ifdef COMPLETE_VFS
uint32_t read_ata(fs_node_t *node, uint32_t offset, uint32_t size,
				  uint8_t *buffer);
uint32_t write_ata(fs_node_t *node, uint32_t offset, uint32_t size,
				   uint8_t *buffer);
void open_ata(fs_node_t *node, unsigned int flags);
void close_ata(fs_node_t *node);
#endif

uint64_t ata_max_offset(ata_device_t *dev)
{
	uint64_t sectors = dev->identity.sectors_48;
	if (!sectors) {
		// Fall back to sectors_28.
		sectors = dev->identity.sectors_28;
	}

	return sectors * ATA_SECTOR_SIZE;
}

uint64_t atapi_max_offset(ata_device_t *dev)
{
	uint64_t max_sector = dev->atapi_lba;

	if (!max_sector) {
		return 0;
	}

	return (max_sector + 1) * dev->atapi_sector_size;
}

#ifdef COMPLETE_VFS
uint32_t read_ata(fs_node_t *node, uint32_t offset, uint32_t size,
				  uint8_t *buffer)
{
	ata_device_t *dev = (ata_device_t *)node->device;

	unsigned int start_block = offset / ATA_SECTOR_SIZE;
	unsigned int end_block = (offset + size - 1) / ATA_SECTOR_SIZE;

	unsigned int x_offset = 0;

	if (offset > ata_max_offset(dev)) {
		return 0;
	}

	if (offset + size > ata_max_offset(dev)) {
		unsigned int i = ata_max_offset(dev) - offset;
		size = i;
	}

	if (offset % ATA_SECTOR_SIZE) {
		unsigned int prefix_size =
			(ATA_SECTOR_SIZE - (offset % ATA_SECTOR_SIZE));
		char *tmp = malloc(ATA_SECTOR_SIZE);
		ata_device_read_sector(dev, start_block, (uint8_t *)tmp);

		memcpy(buffer, (void *)((uintptr_t)tmp + (offset % ATA_SECTOR_SIZE)),
			   prefix_size);

		free(tmp);

		x_offset += prefix_size;
		start_block++;
	}

	if ((offset + size) % ATA_SECTOR_SIZE && start_block <= end_block) {
		unsigned int postfix_size = (offset + size) % ATA_SECTOR_SIZE;
		char *tmp = malloc(ATA_SECTOR_SIZE);
		ata_device_read_sector(dev, end_block, (uint8_t *)tmp);

		memcpy((void *)((uintptr_t)buffer + size - postfix_size), tmp,
			   postfix_size);

		free(tmp);

		end_block--;
	}

	while (start_block <= end_block) {
		ata_device_read_sector(dev, start_block,
							   (uint8_t *)((uintptr_t)buffer + x_offset));
		x_offset += ATA_SECTOR_SIZE;
		start_block++;
	}

	return size;
}
#endif

#ifdef COMPLETE_VFS
uint32_t read_atapi(fs_node_t *node, uint32_t offset, uint32_t size,
					uint8_t *buffer)
{
	ata_device_t *dev = (ata_device_t *)node->device;

	unsigned int start_block = offset / dev->atapi_sector_size;
	unsigned int end_block = (offset + size - 1) / dev->atapi_sector_size;

	unsigned int x_offset = 0;

	if (offset > atapi_max_offset(dev)) {
		return 0;
	}

	if (offset + size > atapi_max_offset(dev)) {
		unsigned int i = atapi_max_offset(dev) - offset;
		size = i;
	}

	if (offset % dev->atapi_sector_size) {
		unsigned int prefix_size =
			(dev->atapi_sector_size - (offset % dev->atapi_sector_size));
		char *tmp = malloc(dev->atapi_sector_size);
		ata_device_read_sector_atapi(dev, start_block, (uint8_t *)tmp);

		memcpy(buffer,
			   (void *)((uintptr_t)tmp + (offset % dev->atapi_sector_size)),
			   prefix_size);

		free(tmp);

		x_offset += prefix_size;
		start_block++;
	}

	if ((offset + size) % dev->atapi_sector_size && start_block <= end_block) {
		unsigned int postfix_size = (offset + size) % dev->atapi_sector_size;
		char *tmp = malloc(dev->atapi_sector_size);
		ata_device_read_sector_atapi(dev, end_block, (uint8_t *)tmp);

		memcpy((void *)((uintptr_t)buffer + size - postfix_size), tmp,
			   postfix_size);

		free(tmp);

		end_block--;
	}

	while (start_block <= end_block) {
		ata_device_read_sector_atapi(dev, start_block,
									 (uint8_t *)((uintptr_t)buffer + x_offset));
		x_offset += dev->atapi_sector_size;
		start_block++;
	}

	return size;
}
#endif

#ifdef COMPLETE_VFS
uint32_t write_ata(fs_node_t *node, uint32_t offset, uint32_t size,
				   uint8_t *buffer)
{
	ata_device_t *dev = (ata_device_t *)node->device;

	unsigned int start_block = offset / ATA_SECTOR_SIZE;
	unsigned int end_block = (offset + size - 1) / ATA_SECTOR_SIZE;

	unsigned int x_offset = 0;

	if (offset > ata_max_offset(dev)) {
		return 0;
	}

	if (offset + size > ata_max_offset(dev)) {
		unsigned int i = ata_max_offset(dev) - offset;
		size = i;
	}

	if (offset % ATA_SECTOR_SIZE) {
		unsigned int prefix_size =
			(ATA_SECTOR_SIZE - (offset % ATA_SECTOR_SIZE));

		char *tmp = malloc(ATA_SECTOR_SIZE);
		ata_device_read_sector(dev, start_block, (uint8_t *)tmp);

		dbg_print("Writing first block");

		memcpy((void *)((uintptr_t)tmp + (offset % ATA_SECTOR_SIZE)), buffer,
			   prefix_size);
		ata_device_write_sector_retry(dev, start_block, (uint8_t *)tmp);

		free(tmp);
		x_offset += prefix_size;
		start_block++;
	}

	if ((offset + size) % ATA_SECTOR_SIZE && start_block <= end_block) {
		unsigned int postfix_size = (offset + size) % ATA_SECTOR_SIZE;

		char *tmp = malloc(ATA_SECTOR_SIZE);
		ata_device_read_sector(dev, end_block, (uint8_t *)tmp);

		dbg_print("Writing last block");

		memcpy(tmp, (void *)((uintptr_t)buffer + size - postfix_size),
			   postfix_size);

		ata_device_write_sector_retry(dev, end_block, (uint8_t *)tmp);

		free(tmp);
		end_block--;
	}

	while (start_block <= end_block) {
		ata_device_write_sector_retry(
			dev, start_block, (uint8_t *)((uintptr_t)buffer + x_offset));
		x_offset += ATA_SECTOR_SIZE;
		start_block++;
	}

	return size;
}
#endif

#ifdef COMPLETE_VFS
void open_ata(fs_node_t *node, unsigned int flags)
{
	return;
}
#endif

#ifdef COMPLETE_VFS
void close_ata(fs_node_t *node)
{
	return;
}
#endif

#ifdef COMPLETE_VFS
fs_node_t *atapi_device_create(ata_device_t *device)
{
	fs_node_t *fnode = malloc(sizeof(fs_node_t));
	memset(fnode, 0x00, sizeof(fs_node_t));
	fnode->inode = 0;
	sprintf(fnode->name, "cdrom%d", cdrom_number);
	fnode->device = device;
	fnode->uid = 0;
	fnode->gid = 0;
	fnode->mask = 0660;
	fnode->length = atapi_max_offset(device);
	fnode->flags = FS_BLOCKDEVICE;
	fnode->read = read_atapi;
	// No write support.
	fnode->write = NULL;
	fnode->open = open_ata;
	fnode->close = close_ata;
	fnode->readdir = NULL;
	fnode->finddir = NULL;
	// TODO, identify: etc?
	fnode->ioctl = NULL;
	return fnode;
}
#endif

#ifdef COMPLETE_VFS
fs_node_t *ata_device_create(ata_device_t *device)
{
	fs_node_t *fnode = malloc(sizeof(fs_node_t));
	memset(fnode, 0x00, sizeof(fs_node_t));
	fnode->inode = 0;
	sprintf(fnode->name, "atadev%d", ata_drive_char - 'a');
	fnode->device = device;
	fnode->uid = 0;
	fnode->gid = 0;
	fnode->mask = 0660;
	// TODO
	fnode->length = ata_max_offset(device);
	fnode->flags = FS_BLOCKDEVICE;
	fnode->read = read_ata;
	fnode->write = write_ata;
	fnode->open = open_ata;
	fnode->close = close_ata;
	fnode->readdir = NULL;
	fnode->finddir = NULL;
	// TODO: identify, etc?
	fnode->ioctl = NULL;
	return fnode;
}
#endif

void ata_soft_reset(ata_device_t *dev)
{
	outportb(dev->control, 0x04);
	ata_io_wait(dev);
	outportb(dev->control, 0x00);
}

void ata_irq_handler(pt_regs *r)
{
	inportb(ata_primary_master.io_base + ATA_REG_STATUS);

	if (atapi_in_progress) {
		// wakeup_queue(atapi_waiter);
	}

	// irq_ack(14);
	pic8259_send_eoi(14);
}

void ata_irq_handler_s(pt_regs *r)
{
	inportb(ata_secondary_master.io_base + ATA_REG_STATUS);

	if (atapi_in_progress) {
		// wakeup_queue(atapi_waiter);
	}

	// irq_ack(15);
	pic8259_send_eoi(15);
}

bool_t ata_device_init(ata_device_t *dev)
{
	dbg_print("Detected IDE device on bus 0x%3x\n", dev->io_base);
	dbg_print("Device name: %s\n", dev->name);
	ata_device_select(dev);
	outportb(dev->io_base + ATA_REG_COMMAND, ATA_CMD_IDENT);
	ata_io_wait(dev);
	uint8_t status = inportb(dev->io_base + ATA_REG_COMMAND);
	dbg_print("Device status: %d\n", status);

	ata_wait(dev, false);

	uint16_t *buf = (uint16_t *)&dev->identity;
	for (int i = 0; i < 256; ++i) {
		buf[i] = inports(dev->io_base);
	}

	uint8_t *ptr = (uint8_t *)&dev->identity.model;
	for (int i = 0; i < 39; i += 2) {
		char tmp = ptr[i + 1];
		ptr[i + 1] = ptr[i];
		ptr[i] = tmp;
	}
	ptr[39] = 0;

	dbg_print("Device Model: %s\n", dev->identity.model);
	dbg_print("Sectors (48): %d\n", (uint32_t)dev->identity.sectors_48);
	dbg_print("Sectors (24): %d\n", dev->identity.sectors_28);

	dbg_print("Setting up DMA...\n");
	// TODO: Ale.
	// dev->dma_prdt = kmalloc_p(sizeof(prdt_t) * 1, &dev->dma_prdt_phys);
	// dev->dma_start = kmalloc_p(4096, &dev->dma_start_phys);

	dbg_print("Putting prdt    at 0x%x (0x%x phys)\n", dev->dma_prdt,
			  dev->dma_prdt_phys);
	dbg_print("Putting prdt[0] at 0x%x (0x%x phys)\n", dev->dma_start,
			  dev->dma_start_phys);

	dev->dma_prdt[0].offset = dev->dma_start_phys;
	dev->dma_prdt[0].bytes = 512;
	dev->dma_prdt[0].last = 0x8000;

	dbg_print("ATA PCI device ID: 0x%x\n", ata_pci);

	uint16_t command_reg = pci_read_field(ata_pci, PCI_COMMAND, 4);
	dbg_print("COMMAND register before: 0x%4x\n", command_reg);
	if (command_reg & (1U << 2U)) {
		dbg_print("Bus mastering already enabled.\n");
	} else {
		// bit 2.
		command_reg |= (1U << 2U);
		dbg_print("Enabling bus mastering...\n");
		pci_write_field(ata_pci, PCI_COMMAND, 4, command_reg);
		command_reg = pci_read_field(ata_pci, PCI_COMMAND, 4);
		dbg_print("COMMAND register after: 0x%4x\n", command_reg);
	}

	dev->bar4 = pci_read_field(ata_pci, PCI_BASE_ADDRESS_4, 4);
	dbg_print("BAR4: 0x%x\n", dev->bar4);

	if (dev->bar4 & 0x00000001U) {
		dev->bar4 = dev->bar4 & 0xFFFFFFFC;
	} else {
		dbg_print("? ATA bus master registers are 'usually' I/O ports.\n");

		// No DMA because we're not sure what to do here-
		return false;
	}
#if 0
    pci_write_field(ata_pci, PCI_INTERRUPT_LINE, 1, 0xFE);
    if (pci_read_field(ata_pci, PCI_INTERRUPT_LINE, 1) == 0xFE)
    {
        // Needs assignment.
        pci_write_field(ata_pci, PCI_INTERRUPT_LINE, 1, 14);
    }
#endif

	return true;
}

int atapi_device_init(ata_device_t *dev)
{
	dbg_print("Detected ATAPI device at io-base 0x%3x, ctrl 0x%3x, slave %d\n",
			  dev->io_base, dev->control, dev->slave);
	dbg_print("Device name: %s\n", dev->name);
	ata_device_select(dev);
	outportb(dev->io_base + ATA_REG_COMMAND, ATAPI_CMD_ID_PCKT);
	ata_io_wait(dev);
	uint8_t status = inportb(dev->io_base + ATA_REG_COMMAND);
	dbg_print("Device status: %d\n", status);

	ata_wait(dev, false);

	uint16_t *buf = (uint16_t *)&dev->identity;

	for (int i = 0; i < 256; ++i) {
		buf[i] = inports(dev->io_base);
	}

	uint8_t *ptr = (uint8_t *)&dev->identity.model;
	for (int i = 0; i < 39; i += 2) {
		char tmp = ptr[i + 1];
		ptr[i + 1] = ptr[i];
		ptr[i] = tmp;
	}
	ptr[39] = 0;

	dbg_print("Device Model: %s\n", dev->identity.model);

	// Detect medium.
	atapi_command_t command;
	command.command_bytes[0] = 0x25;
	command.command_bytes[1] = 0;
	command.command_bytes[2] = 0;
	command.command_bytes[3] = 0;
	command.command_bytes[4] = 0;
	command.command_bytes[5] = 0;
	command.command_bytes[6] = 0;
	command.command_bytes[7] = 0;
	// Bit 0 = PMI (0, last sector).
	command.command_bytes[8] = 0;
	// Control.
	command.command_bytes[9] = 0;
	command.command_bytes[10] = 0;
	command.command_bytes[11] = 0;

	uint16_t bus = dev->io_base;

	outportb(bus + ATA_REG_FEATURES, 0x00);
	outportb(bus + ATA_REG_LBA1, 0x08);
	outportb(bus + ATA_REG_LBA2, 0x08);
	outportb(bus + ATA_REG_COMMAND, ATAPI_CMD_PACKET);

	// Poll.
	while (1) {
		status = inportb(dev->io_base + ATA_REG_STATUS);
		if ((status & ATA_STAT_ERR)) {
			goto atapi_error;
		}
		if (!(status & ATA_STAT_BUSY) && (status & ATA_STAT_READY)) {
			break;
		}
	}

	for (int i = 0; i < 6; ++i)
		outports(bus, command.command_words[i]);

	// Poll.
	while (1) {
		status = inportb(dev->io_base + ATA_REG_STATUS);
		if ((status & ATA_STAT_ERR)) {
			goto atapi_error_read;
		}
		if (!(status & ATA_STAT_BUSY) && (status & ATA_STAT_READY)) {
			break;
		}
		if ((status & ATA_STAT_DRQ)) {
			break;
		}
	}

	uint16_t data[4];

	for (int i = 0; i < 4; ++i) {
		data[i] = inports(bus);
	}

#define htonl(l)                                                               \
	((((l)&0xFF) << 24) | (((l)&0xFF00) << 8) | (((l)&0xFF0000) >> 8) |        \
	 (((l)&0xFF000000) >> 24))
	uint32_t lba, blocks;
	;

	memcpy(&lba, &data[0], sizeof(uint32_t));

	lba = htonl(lba);

	memcpy(&blocks, &data[2], sizeof(uint32_t));

	blocks = htonl(blocks);

	dev->atapi_lba = lba;
	dev->atapi_sector_size = blocks;

	if (!lba) {
		return false;
	}

	dbg_print("Finished! LBA = %x; block length = %x\n", lba, blocks);
	return true;

atapi_error_read:
	dbg_print("ATAPI error; no medium?\n");
	return false;

atapi_error:
	dbg_print("ATAPI early error; unsure\n");
	return false;
}

int ata_device_detect(ata_device_t *dev)
{
	static char ata_drive_char = 'a';
	static int cdrom_number = 0;

	ata_soft_reset(dev);
	ata_io_wait(dev);
	outportb(dev->io_base + ATA_REG_HDDEVSEL, 0xA0 | dev->slave << 4);
	ata_io_wait(dev);
	ata_status_wait(dev, 10000);

	dbg_print("Probing cylinder registers...\n");
	uint8_t cl = inportb(dev->io_base + ATA_REG_LBA1);
	uint8_t ch = inportb(dev->io_base + ATA_REG_LBA2);
	if ((cl == 0xFF) && (ch == 0xFF)) {
		dbg_print("No drive(s) present\n");
		return false;
	}

	dbg_print("Waiting while busy...\n");
	uint8_t status = ata_status_wait(dev, 5000);
	if (status & ATA_STAT_BUSY) {
		dbg_print("No drive(s) present\n");

		return false;
	}

	dbg_print("Device detected: 0x%2x 0x%2x\n", cl, ch);
	if ((cl == 0x00 && ch == 0x00) || (cl == 0x3C && ch == 0xC3)) {
		// The device is not an ATAPI.
		dev->is_atapi = false;
		// Parallel ATA device, or emulated SATA
		sprintf(dev->name, "/dev/hd%c", ata_drive_char);

#ifdef COMPLETE_VFS
		fs_node_t *node = ata_device_create(dev);
		vfs_mount(devname, node);
#endif

		++ata_drive_char;
		if (!ata_device_init(dev)) {
			return 0;
		}

		return 1;
	} else if ((cl == 0x14 && ch == 0xEB) || (cl == 0x69 && ch == 0x96)) {
		// The device is an ATAPI.
		dev->is_atapi = true;
		sprintf(dev->name, "/dev/cdrom%d", cdrom_number);

#ifdef COMPLETE_VFS
		fs_node_t *node = atapi_device_create(dev);
		vfs_mount(devname, node);
#endif

		++cdrom_number;
		if (!atapi_device_init(dev)) {
			return 0;
		}
		return 2;
	}
	dbg_print("\n");

	// TODO: ATAPI, SATA, SATAPI.

	return 0;
}

void ata_device_read_sector(ata_device_t *dev, uint32_t lba, uint8_t *buf)
{
	uint16_t bus = dev->io_base;

	uint8_t slave = dev->slave;

	if (dev->is_atapi) {
		return;
	}

	spinlock_lock(&ata_lock);

#if 0
    int errors = 0;
    try_again:
#endif

	ata_wait(dev, false);

	// Stop.
	outportb(dev->bar4, 0x00);

	// Set the PRDT.
	outportl(dev->bar4 + 0x04, dev->dma_prdt_phys);

	// Enable error, irq status.
	outportb(dev->bar4 + 0x2, inportb(dev->bar4 + 0x02) | 0x04 | 0x02);

	// Set read.
	outportb(dev->bar4, 0x08);

	irq_enable();

	while (1) {
		uint8_t status = inportb(dev->io_base + ATA_REG_STATUS);
		if (!(status & ATA_STAT_BUSY)) {
			break;
		}
	}

	outportb(bus + ATA_REG_CONTROL, 0x00);
	outportb(bus + ATA_REG_HDDEVSEL,
			 0xe0 | slave << 4 | (lba & 0x0f000000) >> 24);
	ata_io_wait(dev);
	outportb(bus + ATA_REG_FEATURES, 0x00);
	outportb(bus + ATA_REG_SECCOUNT0, 1);
	outportb(bus + ATA_REG_LBA0, (lba & 0x000000ff) >> 0);
	outportb(bus + ATA_REG_LBA1, (lba & 0x0000ff00) >> 8);
	outportb(bus + ATA_REG_LBA2, (lba & 0x00ff0000) >> 16);
	// outportb(bus + ATA_REG_COMMAND, ATA_CMD_READ);
#if 1
	while (1) {
		uint8_t status = inportb(dev->io_base + ATA_REG_STATUS);
		if (!(status & ATA_STAT_BUSY) && (status & ATA_STAT_READY)) {
			break;
		}
	}
#endif
	outportb(bus + ATA_REG_COMMAND, ATA_CMD_RD_DMA);

	ata_io_wait(dev);

	outportb(dev->bar4, 0x08 | 0x01);

	while (1) {
		int status = inportb(dev->bar4 + 0x02);
		int dstatus = inportb(dev->io_base + ATA_REG_STATUS);
		if (!(status & 0x04)) {
			continue;
		}
		if (!(dstatus & ATA_STAT_BUSY)) {
			break;
		}
	}
	irq_disable();

#if 0
    if (ata_wait(dev, true)) {
        dbg_print("Error during ATA read of lba block %d\n", lba);
        errors++;
        if (errors > 4)
        {
            dbg_print(
                "-- Too many errors trying to read this block. Bailing.\n");
            spinlock_unlock(&ata_lock);
            return;
        }
        goto try_again;
    }
#endif

	// Copy from DMA buffer to output buffer.
	memcpy(buf, dev->dma_start, 512);

	// Inform device we are done.
	outportb(dev->bar4 + 0x2, inportb(dev->bar4 + 0x02) | 0x04 | 0x02);

#if 0
    int size = 256;
    inportsm(bus,buf,size);
    ata_wait(dev, false);
    outportb(bus + ATA_REG_CONTROL, 0x02);
#endif
	spinlock_unlock(&ata_lock);
}

void ata_device_read_sector_atapi(ata_device_t *dev, uint32_t lba, uint8_t *buf)
{
	if (!dev->is_atapi) {
		return;
	}

	uint16_t bus = dev->io_base;
	spinlock_lock(&ata_lock);

	outportb(dev->io_base + ATA_REG_HDDEVSEL, 0xA0 | dev->slave << 4);
	ata_io_wait(dev);

	outportb(bus + ATA_REG_FEATURES, 0x00);
	outportb(bus + ATA_REG_LBA1, dev->atapi_sector_size & 0xFF);
	outportb(bus + ATA_REG_LBA2, dev->atapi_sector_size >> 8);
	outportb(bus + ATA_REG_COMMAND, ATAPI_CMD_PACKET);

	// Poll.
	while (1) {
		uint8_t status = inportb(dev->io_base + ATA_REG_STATUS);
		if ((status & ATA_STAT_ERR)) {
			goto atapi_error_on_read_setup;
		}
		if (!(status & ATA_STAT_BUSY) && (status & ATA_STAT_DRQ)) {
			break;
		}
	}

	atapi_in_progress = true;

	atapi_command_t command;
	command.command_bytes[0] = 0xA8;
	command.command_bytes[1] = 0;
	command.command_bytes[2] = (lba >> 0x18) & 0xFF;
	command.command_bytes[3] = (lba >> 0x10) & 0xFF;
	command.command_bytes[4] = (lba >> 0x08) & 0xFF;
	command.command_bytes[5] = (lba >> 0x00) & 0xFF;
	command.command_bytes[6] = 0;
	command.command_bytes[7] = 0;
	// Bit 0 = PMI (0, last sector).
	command.command_bytes[8] = 0;
	// Control.
	command.command_bytes[9] = 1;
	command.command_bytes[10] = 0;
	command.command_bytes[11] = 0;

	for (int i = 0; i < 6; ++i) {
		outports(bus, command.command_words[i]);
	}

	// Wait.
#ifdef COMPLETE_SCHEDULER
	sleep_on(atapi_waiter);
#endif

	atapi_in_progress = false;

	while (1) {
		uint8_t status = inportb(dev->io_base + ATA_REG_STATUS);
		if ((status & ATA_STAT_ERR)) {
			goto atapi_error_on_read_setup;
		}
		if (!(status & ATA_STAT_BUSY) && (status & ATA_STAT_DRQ)) {
			break;
		}
	}

	uint16_t size_to_read = inportb(bus + ATA_REG_LBA2) << 8;
	size_to_read = size_to_read | inportb(bus + ATA_REG_LBA1);

	inportsm(bus, buf, size_to_read / 2);

	while (1) {
		uint8_t status = inportb(dev->io_base + ATA_REG_STATUS);
		if ((status & ATA_STAT_ERR)) {
			goto atapi_error_on_read_setup;
		}
		if (!(status & ATA_STAT_BUSY) && (status & ATA_STAT_READY)) {
			break;
		}
	}

atapi_error_on_read_setup:
	spinlock_unlock(&ata_lock);
}

void ata_device_write_sector(ata_device_t *dev, uint32_t lba, uint8_t *buf)
{
	uint16_t bus = dev->io_base;
	uint8_t slave = dev->slave;

	spinlock_lock(&ata_lock);

	outportb(bus + ATA_REG_CONTROL, 0x02);

	ata_wait(dev, false);
	outportb(bus + ATA_REG_HDDEVSEL,
			 0xe0 | slave << 4 | (lba & 0x0f000000) >> 24);
	ata_wait(dev, false);

	outportb(bus + ATA_REG_FEATURES, 0x00);
	outportb(bus + ATA_REG_SECCOUNT0, 0x01);
	outportb(bus + ATA_REG_LBA0, (lba & 0x000000ff) >> 0);
	outportb(bus + ATA_REG_LBA1, (lba & 0x0000ff00) >> 8);
	outportb(bus + ATA_REG_LBA2, (lba & 0x00ff0000) >> 16);
	outportb(bus + ATA_REG_COMMAND, ATA_CMD_WRITE);
	ata_wait(dev, false);
	int size = ATA_SECTOR_SIZE / 2;
	outportsm(bus, buf, size);
	outportb(bus + 0x07, ATA_CMD_CH_FLSH);
	ata_wait(dev, false);
	spinlock_unlock(&ata_lock);
}

int buffer_compare(uint32_t *ptr1, uint32_t *ptr2, size_t size)
{
	assert(!(size % 4));

	size_t i = 0;

	while (i < size) {
		if (*ptr1 != *ptr2) {
			return 1;
		}

		ptr1++;
		ptr2++;
		i += sizeof(uint32_t);
	}
	return 0;
}

void ata_device_write_sector_retry(ata_device_t *dev, uint32_t lba,
								   uint8_t *buf)
{
	uint8_t *read_buf = kmalloc(ATA_SECTOR_SIZE);
	do {
		ata_device_write_sector(dev, lba, buf);
		ata_device_read_sector(dev, lba, read_buf);
	} while (
		buffer_compare((uint32_t *)buf, (uint32_t *)read_buf, ATA_SECTOR_SIZE));
	kfree(read_buf);
}

void find_ata_pci(uint32_t dev, uint16_t vid, uint16_t did, void *extra)
{
	if ((vid == 0x8086) && (did == 0x7010 || did == 0x7111)) {
		*((uint32_t *)extra) = dev;
	}
}

int ata_initialize()
{
	// Detect drives and mount them.
	// Locate ATA device via PCI.
	pci_scan(&find_ata_pci, -1, &ata_pci);

	irq_install_handler(14, ata_irq_handler, "ide master");
	irq_install_handler(15, ata_irq_handler_s, "ide slave");

	atapi_waiter = list_create();

	dbg_print("Detecteing devices...\n");
	dbg_print("Detecteing Primary Master...\n");
	ata_device_detect(&ata_primary_master);
	dbg_print("\n");
	dbg_print("Detecteing Primary Slave...\n");
	ata_device_detect(&ata_primary_slave);
	dbg_print("\n");
	dbg_print("Detecteing Secondary Master...\n");
	ata_device_detect(&ata_secondary_master);
	dbg_print("\n");
	dbg_print("Detecteing Secondary Slave...\n");
	ata_device_detect(&ata_secondary_slave);
	dbg_print("\n");
	dbg_print("Done\n");

	return 0;
}

int ata_finalize()
{
	return 0;
}
