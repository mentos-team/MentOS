/// @file virtio.c
/// @brief Minimal modern virtio-pci transport and split virtqueue.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.
///
/// See io/../devices/virtio.h for what this deliberately does not implement, and
/// for the volatile-access trap that governs every ring read here.

// Setup the logging for this file (do this before any other include).
#include "sys/kernel_levels.h"           // Include kernel log levels.
#define __DEBUG_HEADER__ "[VIRTIO]"      ///< Change header.
#define __DEBUG_LEVEL__  LOGLEVEL_NOTICE ///< Set log level.
#include "io/debug.h"                    // Include debugging functions.

#include "devices/virtio.h"

#include "devices/pci.h"
#include "mem/alloc/zone_allocator.h"
#include "mem/mm/vm_area.h"
#include "mem/paging.h"
#include "stddef.h"
#include "string.h"
#include "klib/perf.h"

/// @brief PCI vendor ID assigned to virtio devices.
#define VIRTIO_PCI_VENDOR      0x1AF4U
/// @brief Base of the modern virtio PCI device ID range.
#define VIRTIO_PCI_DEVICE_BASE 0x1040U

/// @brief Bound on how long a polled completion is waited for.
///
/// Deliberately an iteration count and not a wall-clock timeout: this runs at
/// points in the boot where the timer is not yet installed, so there is no clock
/// to consult. The number is large because a QEMU under TCG services a queue
/// from its own thread, and a guest spinning here competes with it -- measured
/// completion latency is on the order of milliseconds, not microseconds.
#define VIRTQ_POLL_ROUNDS      2000000U

/// @brief Compile-time check that the register window is in unused kernel space.
///
/// The kernel's linear map cannot reach 0xF8000000 and the I/O APIC sits at
/// 0xFEC00000; see the window map in virtio.h.
typedef char virtio_window_check
    [((VIRTIO_MMIO_VIRT_BASE >= 0xF8000000U) &&
      ((VIRTIO_MMIO_VIRT_BASE + (VIRTIO_MMIO_SLOTS * VIRTIO_MMIO_WINDOW)) <= 0xFEC00000U))
         ? 1
         : -1];

/// @brief Which register-window slots are in use.
static bool_t virtio_window_used[VIRTIO_MMIO_SLOTS];

/// @brief Reads a 16-bit register from a mapped structure.
static inline uint16_t __virtio_r16(volatile uint8_t *base, unsigned offset)
{
    return *(volatile uint16_t *)(base + offset);
}

/// @brief Writes a 16-bit register in a mapped structure.
static inline void __virtio_w16(volatile uint8_t *base, unsigned offset, uint16_t value)
{
    *(volatile uint16_t *)(base + offset) = value;
}

/// @brief Reads a 32-bit register from a mapped structure.
static inline uint32_t __virtio_r32(volatile uint8_t *base, unsigned offset)
{
    return *(volatile uint32_t *)(base + offset);
}

/// @brief Writes a 32-bit register in a mapped structure.
static inline void __virtio_w32(volatile uint8_t *base, unsigned offset, uint32_t value)
{
    *(volatile uint32_t *)(base + offset) = value;
}

/// @brief Reads an 8-bit register from a mapped structure.
static inline uint8_t __virtio_r8(volatile uint8_t *base, unsigned offset) { return *(base + offset); }

/// @brief Writes an 8-bit register in a mapped structure.
static inline void __virtio_w8(volatile uint8_t *base, unsigned offset, uint8_t value) { *(base + offset) = value; }

/// @brief Writes a 64-bit queue address as its two halves.
///
/// The registers are a pair of 32-bit halves rather than one 64-bit location, and
/// this kernel is 32-bit, so the high half is always zero.
static inline void __virtio_write_addr(volatile uint8_t *common, unsigned offset, uint32_t physical)
{
    __virtio_w32(common, offset, physical);
    __virtio_w32(common, offset + 4, 0);
}

/// @brief Rounds a value up to a multiple of a power of two.
static inline uint32_t __align_up_to(uint32_t value, uint32_t alignment)
{
    return (value + (alignment - 1U)) & ~(alignment - 1U);
}

/// @brief Byte offsets of the three rings inside one allocation.
///
/// The specification requires the descriptor table to be 16-byte aligned, the
/// available ring 2-byte and the used ring 4-byte. Everything is placed on a
/// 16-byte boundary here, which satisfies all three and makes the arithmetic
/// obvious.
static void __virtq_layout(uint16_t size, uint32_t *avail_offset, uint32_t *used_offset, uint32_t *total)
{
    uint32_t desc_bytes  = 16U * (uint32_t)size;
    uint32_t avail_bytes = 6U + (2U * (uint32_t)size);
    uint32_t used_bytes  = 6U + (8U * (uint32_t)size);

    *avail_offset = __align_up_to(desc_bytes, 16U);
    *used_offset  = __align_up_to(*avail_offset + avail_bytes, 16U);
    *total        = *used_offset + used_bytes;
}

/// @name Available and used ring accessors
/// @brief The rings are addressed as bytes because their layout depends on the
/// queue size, so they are not fixed-size structures.
///
/// Every field the device writes is read through a volatile pointer. See the
/// note in virtio.h: getting this wrong does not fail loudly, it makes the
/// device appear to answer one request late.
/// @{

/// @brief Sets the available ring's flags.
static inline void __avail_set_flags(uint8_t *avail, uint16_t flags) { *(volatile uint16_t *)(avail + 0) = flags; }

/// @brief Publishes the available ring's index.
static inline void __avail_set_idx(uint8_t *avail, uint16_t index) { *(volatile uint16_t *)(avail + 2) = index; }

/// @brief Places a descriptor head in the available ring.
static inline void __avail_set_entry(uint8_t *avail, uint16_t slot, uint16_t head)
{
    *(volatile uint16_t *)(avail + 4 + ((uint32_t)slot * 2U)) = head;
}

/// @brief Reads the used ring's index.
static inline uint16_t __used_idx(const uint8_t *used) { return *(volatile const uint16_t *)(used + 2); }

/// @brief Reads a used-ring entry.
static inline void __used_entry(const uint8_t *used, uint16_t slot, uint32_t *id, uint32_t *length)
{
    const volatile uint32_t *entry = (const volatile uint32_t *)(used + 4 + ((uint32_t)slot * 8U));
    *id                            = entry[0];
    *length                        = entry[1];
}
/// @}

/// @brief Reserves a register-window slot.
/// @param[out] slot Where the slot index is stored.
/// @return 0 on success, -1 when all slots are taken.
static int __virtio_claim_window(unsigned *slot)
{
    for (unsigned index = 0; index < VIRTIO_MMIO_SLOTS; ++index) {
        if (!virtio_window_used[index]) {
            virtio_window_used[index] = true;
            *slot                     = index;
            return 0;
        }
    }
    pr_err("All %u virtio register windows are in use.\n", (unsigned)VIRTIO_MMIO_SLOTS);
    return -1;
}

/// @brief Extracts a usable physical address from a device's BAR.
/// @param pci_device The PCI device.
/// @param index Which BAR to read.
/// @param[out] address Where the address is stored.
/// @return 0 on success, -1 when the BAR is not one this transport can use.
///
/// The encoding is decoded rather than masked blindly, because virtio actually
/// uses the case that blind masking gets away with by luck. QEMU's virtio-vga
/// declares the BAR holding the virtio structures as a **64-bit** prefetchable
/// memory BAR (observed value 0xFE80000C, so bits 2:1 are 0b10), whose upper
/// half lives in the following BAR register. Masking the low half alone happens
/// to work only while that upper half is zero, which is exactly the kind of
/// accident that survives testing and then breaks.
///
/// So: a 64-bit BAR is accepted when its upper half is zero -- this is a 32-bit
/// kernel and there is nothing it could do with an aperture above 4 GiB -- and
/// refused with a diagnostic when it is not.
static int __virtio_bar_address(uint32_t pci_device, uint8_t index, uint32_t *address)
{
    uint32_t low = 0;
    if (pci_read_32(pci_device, PCI_BASE_ADDRESS_0 + ((uint32_t)index * 4U), &low)) {
        pr_err("Failed to read BAR %u of virtio device 0x%08x.\n", index, pci_device);
        return -1;
    }

    if ((low & 0x1U) != 0U) {
        pr_err("Virtio BAR %u (0x%08x) is an I/O BAR, not memory.\n", index, low);
        return -1;
    }

    uint32_t type = (low >> 1U) & 0x3U;
    if (type == 0x2U) {
        // A 64-bit BAR consumes this register and the next one.
        if (index >= 5U) {
            pr_err("Virtio BAR %u claims to be 64-bit but has no companion register.\n", index);
            return -1;
        }
        uint32_t high = 0;
        if (pci_read_32(pci_device, PCI_BASE_ADDRESS_0 + (((uint32_t)index + 1U) * 4U), &high)) {
            pr_err("Failed to read the upper half of BAR %u.\n", index);
            return -1;
        }
        if (high != 0U) {
            pr_err("Virtio BAR %u is at 0x%08x%08x, above what a 32-bit kernel can map.\n", index, high,
                   low & 0xFFFFFFF0U);
            return -1;
        }
    } else if (type != 0x0U) {
        pr_err("Virtio BAR %u (0x%08x) has reserved memory type %u.\n", index, low, type);
        return -1;
    }

    uint32_t candidate = low & 0xFFFFFFF0U;
    if (candidate == 0U) {
        pr_err("Virtio BAR %u is unassigned; the firmware left it at zero.\n", index);
        return -1;
    }
    if ((candidate & (PAGE_SIZE - 1U)) != 0U) {
        pr_err("Virtio BAR %u (0x%08x) is not page aligned.\n", index, candidate);
        return -1;
    }

    *address = candidate;
    return 0;
}

/// @brief One virtio-pci capability, decoded.
typedef struct {
    uint8_t cfg_type; ///< Which structure it describes.
    uint8_t bar;      ///< Which BAR it lives in.
    uint32_t offset;  ///< Offset within that BAR.
    uint32_t length;  ///< Length of the structure.
} virtio_cap_t;

/// @brief Reads the four structure capabilities the transport needs.
/// @param pci_device The PCI device.
/// @param[out] caps Indexed by cfg_type; entries with length 0 were not found.
/// @return 0 on success, -1 on error.
static int __virtio_read_caps(uint32_t pci_device, virtio_cap_t caps[VIRTIO_PCI_CAP_PCI_CFG + 1])
{
    memset(caps, 0, sizeof(virtio_cap_t) * (VIRTIO_PCI_CAP_PCI_CFG + 1));

    uint8_t position = 0;
    int result;
    while ((result = pci_find_capability(pci_device, PCI_CAP_ID_VENDOR, position, &position)) == 0) {
        uint8_t cfg_type = 0;
        uint8_t bar      = 0;
        uint32_t offset  = 0;
        uint32_t length  = 0;

        if (pci_read_8(pci_device, (uint32_t)position + VIRTIO_PCI_CAP_CFG_TYPE, &cfg_type) ||
            pci_read_8(pci_device, (uint32_t)position + VIRTIO_PCI_CAP_BAR, &bar) ||
            pci_read_32(pci_device, (uint32_t)position + VIRTIO_PCI_CAP_OFFSET, &offset) ||
            pci_read_32(pci_device, (uint32_t)position + VIRTIO_PCI_CAP_LENGTH, &length)) {
            pr_err("Failed to read the virtio capability at 0x%02x.\n", position);
            return -1;
        }

        // Ignore anything past the types the transport knows, and anything with
        // a zero length: a capability that describes nothing is not usable.
        if ((cfg_type <= VIRTIO_PCI_CAP_PCI_CFG) && (length != 0U) && (caps[cfg_type].length == 0U)) {
            caps[cfg_type].cfg_type = cfg_type;
            caps[cfg_type].bar      = bar;
            caps[cfg_type].offset   = offset;
            caps[cfg_type].length   = length;
            pr_debug("virtio cap type %u: bar %u, offset 0x%08x, length 0x%08x\n", cfg_type, bar, offset, length);
        }
    }
    return (result < 0) ? -1 : 0;
}

int virtio_pci_find(uint16_t device_type, uint32_t *pci_device)
{
    if (pci_device == NULL) {
        pr_err("Output parameter 'pci_device' is NULL.\n");
        return -1;
    }

    uint16_t wanted = (uint16_t)(VIRTIO_PCI_DEVICE_BASE + device_type);

    // pci_scan() matches on class, not on vendor, so the search is done here.
    // Bus 0 slots only: this kernel has never had a device behind a bridge, and
    // walking bridges would need pci_scan()'s recursion, which cannot filter on
    // vendor.
    for (uint8_t slot = 0; slot < 32; ++slot) {
        for (uint8_t function = 0; function < 8; ++function) {
            uint32_t candidate = ((uint32_t)slot << 8) | function;
            uint16_t vendor    = 0;
            uint16_t device    = 0;

            if (pci_read_16(candidate, PCI_VENDOR_ID, &vendor) || (vendor != VIRTIO_PCI_VENDOR)) {
                // Function 0 absent means the whole slot is absent.
                if (function == 0) {
                    break;
                }
                continue;
            }
            if (pci_read_16(candidate, PCI_DEVICE_ID, &device)) {
                continue;
            }
            if (device == wanted) {
                *pci_device = candidate;
                pr_debug("Found modern virtio device type %u at 0x%08x (%04x:%04x).\n", device_type, candidate, vendor,
                         device);
                return 0;
            }
        }
    }
    return 1;
}

int virtio_pci_setup(uint32_t pci_device, virtio_device_t *dev)
{
    if (dev == NULL) {
        pr_err("Output parameter 'dev' is NULL.\n");
        return -1;
    }
    memset(dev, 0, sizeof(*dev));
    dev->pci_device = pci_device;

    virtio_cap_t caps[VIRTIO_PCI_CAP_PCI_CFG + 1];
    if (__virtio_read_caps(pci_device, caps) < 0) {
        return -1;
    }

    // The four structures the transport cannot work without.
    const uint8_t needed[] = {
        VIRTIO_PCI_CAP_COMMON_CFG,
        VIRTIO_PCI_CAP_NOTIFY_CFG,
        VIRTIO_PCI_CAP_ISR_CFG,
        VIRTIO_PCI_CAP_DEVICE_CFG,
    };
    for (unsigned index = 0; index < count_of(needed); ++index) {
        if (caps[needed[index]].length == 0U) {
            pr_err("Virtio device 0x%08x has no configuration structure of type %u.\n", pci_device, needed[index]);
            return -1;
        }
    }

    // They must share a BAR. Every device this kernel targets puts all four in
    // one, and mapping several BARs would need a slot per BAR; refuse clearly
    // rather than mapping the wrong thing.
    uint8_t bar = caps[VIRTIO_PCI_CAP_COMMON_CFG].bar;
    uint32_t extent = 0;
    for (unsigned index = 0; index < count_of(needed); ++index) {
        const virtio_cap_t *cap = &caps[needed[index]];
        if (cap->bar != bar) {
            pr_err(
                "Virtio structure type %u is in BAR %u but the common configuration is in BAR %u; this transport "
                "requires one BAR.\n",
                cap->cfg_type, cap->bar, bar);
            return -1;
        }
        if ((cap->offset + cap->length) > extent) {
            extent = cap->offset + cap->length;
        }
    }
    if (extent > VIRTIO_MMIO_WINDOW) {
        pr_err("Virtio structures span 0x%08x bytes, more than the 0x%08x window.\n", extent,
               (unsigned)VIRTIO_MMIO_WINDOW);
        return -1;
    }

    // Read and validate the BAR the structures live in.
    uint32_t physical = 0;
    if (__virtio_bar_address(pci_device, bar, &physical) < 0) {
        return -1;
    }

    // Enable memory space and bus mastering. The device is a bus master: it
    // walks the rings and the buffers itself.
    uint16_t command = 0;
    if (pci_read_16(pci_device, PCI_COMMAND, &command)) {
        pr_err("Failed to read the command register of virtio device 0x%08x.\n", pci_device);
        return -1;
    }
    command |= (uint16_t)((1U << pci_command_memory_space) | (1U << pci_command_bus_master));
    if (pci_write_16(pci_device, PCI_COMMAND, command)) {
        pr_err("Failed to enable memory space and bus mastering on 0x%08x.\n", pci_device);
        return -1;
    }

    unsigned slot = 0;
    if (__virtio_claim_window(&slot) < 0) {
        return -1;
    }
    uint32_t virtual_base = VIRTIO_MMIO_VIRT_BASE + (slot * VIRTIO_MMIO_WINDOW);

    page_directory_t *pgd = paging_get_main_pgd();
    if (pgd == NULL) {
        pr_err("No main page directory to map the virtio registers into.\n");
        virtio_window_used[slot] = false;
        return -1;
    }
    if (mem_upd_vm_area(pgd, virtual_base, physical, __align_up_to(extent, PAGE_SIZE),
                        MM_RW | MM_PRESENT | MM_GLOBAL | MM_UPDADDR) < 0) {
        pr_err("Failed to map virtio registers 0x%08x at 0x%08x.\n", physical, virtual_base);
        virtio_window_used[slot] = false;
        return -1;
    }

    dev->window_slot   = slot;
    dev->common        = (volatile uint8_t *)(virtual_base + caps[VIRTIO_PCI_CAP_COMMON_CFG].offset);
    dev->notify_base   = (volatile uint8_t *)(virtual_base + caps[VIRTIO_PCI_CAP_NOTIFY_CFG].offset);
    dev->isr           = (volatile uint8_t *)(virtual_base + caps[VIRTIO_PCI_CAP_ISR_CFG].offset);
    dev->device_config = (volatile uint8_t *)(virtual_base + caps[VIRTIO_PCI_CAP_DEVICE_CFG].offset);

    // The notify multiplier lives in the notify capability, which has to be
    // found again because __virtio_read_caps() does not keep the offset.
    uint8_t position = 0;
    dev->notify_multiplier = 0;
    while (pci_find_capability(pci_device, PCI_CAP_ID_VENDOR, position, &position) == 0) {
        uint8_t cfg_type = 0;
        if (pci_read_8(pci_device, (uint32_t)position + VIRTIO_PCI_CAP_CFG_TYPE, &cfg_type)) {
            continue;
        }
        if (cfg_type == VIRTIO_PCI_CAP_NOTIFY_CFG) {
            if (pci_read_32(pci_device, (uint32_t)position + VIRTIO_PCI_CAP_NOTIFY_MULT, &dev->notify_multiplier)) {
                pr_err("Failed to read the notify offset multiplier.\n");
                virtio_pci_release(dev);
                return -1;
            }
            break;
        }
    }

    pr_notice(
        "Virtio device 0x%08x mapped: registers 0x%08x at 0x%08x, notify multiplier %u.\n", pci_device, physical,
        virtual_base, dev->notify_multiplier);
    return 0;
}

void virtio_pci_release(virtio_device_t *dev)
{
    if (dev == NULL) {
        return;
    }
    if (dev->common != NULL) {
        // The mapping is left in place: mem_upd_vm_area() has no unmap
        // counterpart, and the window is a fixed reservation rather than
        // something another allocation could claim. Only the slot is returned.
        virtio_window_used[dev->window_slot] = false;
    }
    memset(dev, 0, sizeof(*dev));
}

void virtio_reset(virtio_device_t *dev)
{
    if ((dev == NULL) || (dev->common == NULL)) {
        return;
    }
    __virtio_w8(dev->common, VIRTIO_PCI_DEVICE_STATUS, 0);
    // The device is reset once the register reads back as zero. Bounded, so a
    // device that never clears it cannot hang the boot.
    for (unsigned round = 0; round < 1000000U; ++round) {
        if (__virtio_r8(dev->common, VIRTIO_PCI_DEVICE_STATUS) == 0) {
            return;
        }
        cpu_relax();
    }
    pr_err("Virtio device 0x%08x did not complete its reset.\n", dev->pci_device);
}

uint8_t virtio_read_isr(const virtio_device_t *dev)
{
    if ((dev == NULL) || (dev->isr == NULL)) {
        return 0;
    }
    // Reading clears it, which is why the value has to be returned rather than
    // tested here.
    return *(dev->isr);
}

uint8_t virtio_read_status(const virtio_device_t *dev)
{
    if ((dev == NULL) || (dev->common == NULL)) {
        return 0;
    }
    return __virtio_r8(dev->common, VIRTIO_PCI_DEVICE_STATUS);
}

int virtio_negotiate(virtio_device_t *dev, virtio_features_t wanted, virtio_features_t required)
{
    if ((dev == NULL) || (dev->common == NULL)) {
        pr_err("Cannot negotiate with an unmapped device.\n");
        return -1;
    }

    virtio_reset(dev);
    __virtio_w8(dev->common, VIRTIO_PCI_DEVICE_STATUS, VIRTIO_STATUS_ACKNOWLEDGE);
    __virtio_w8(dev->common, VIRTIO_PCI_DEVICE_STATUS, VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER);

    // Read both halves of what the device offers. The select register chooses
    // which half the feature register refers to.
    virtio_features_t offered;
    __virtio_w32(dev->common, VIRTIO_PCI_DEVICE_FEATURE_SELECT, 0);
    offered.low = __virtio_r32(dev->common, VIRTIO_PCI_DEVICE_FEATURE);
    __virtio_w32(dev->common, VIRTIO_PCI_DEVICE_FEATURE_SELECT, 1);
    offered.high = __virtio_r32(dev->common, VIRTIO_PCI_DEVICE_FEATURE);

    if (((offered.low & required.low) != required.low) || ((offered.high & required.high) != required.high)) {
        pr_err(
            "Virtio device 0x%08x offers 0x%08x:%08x but 0x%08x:%08x is required.\n", dev->pci_device, offered.high,
            offered.low, required.high, required.low);
        __virtio_w8(dev->common, VIRTIO_PCI_DEVICE_STATUS, VIRTIO_STATUS_FAILED);
        return -1;
    }

    virtio_features_t accepted;
    accepted.low  = offered.low & (wanted.low | required.low);
    accepted.high = offered.high & (wanted.high | required.high);

    __virtio_w32(dev->common, VIRTIO_PCI_DRIVER_FEATURE_SELECT, 0);
    __virtio_w32(dev->common, VIRTIO_PCI_DRIVER_FEATURE, accepted.low);
    __virtio_w32(dev->common, VIRTIO_PCI_DRIVER_FEATURE_SELECT, 1);
    __virtio_w32(dev->common, VIRTIO_PCI_DRIVER_FEATURE, accepted.high);

    __virtio_w8(dev->common, VIRTIO_PCI_DEVICE_STATUS,
                VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER | VIRTIO_STATUS_FEATURES_OK);
    if ((__virtio_r8(dev->common, VIRTIO_PCI_DEVICE_STATUS) & VIRTIO_STATUS_FEATURES_OK) == 0) {
        pr_err("Virtio device 0x%08x rejected the feature set 0x%08x:%08x.\n", dev->pci_device, accepted.high,
               accepted.low);
        __virtio_w8(dev->common, VIRTIO_PCI_DEVICE_STATUS, VIRTIO_STATUS_FAILED);
        return -1;
    }

    dev->features = accepted;
    pr_notice("Virtio device 0x%08x negotiated features 0x%08x:%08x.\n", dev->pci_device, accepted.high, accepted.low);
    return 0;
}

bool_t virtio_has_feature(const virtio_device_t *dev, unsigned bit)
{
    if (dev == NULL) {
        return false;
    }
    if (bit < 32U) {
        return (dev->features.low & (1U << bit)) != 0U;
    }
    if (bit < 64U) {
        return (dev->features.high & (1U << (bit - 32U))) != 0U;
    }
    return false;
}

int virtio_driver_ok(virtio_device_t *dev)
{
    if ((dev == NULL) || (dev->common == NULL)) {
        return -1;
    }
    __virtio_w8(dev->common, VIRTIO_PCI_DEVICE_STATUS,
                VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER | VIRTIO_STATUS_FEATURES_OK | VIRTIO_STATUS_DRIVER_OK);
    uint8_t status = __virtio_r8(dev->common, VIRTIO_PCI_DEVICE_STATUS);
    if ((status & VIRTIO_STATUS_DRIVER_OK) == 0) {
        pr_err("Virtio device 0x%08x did not accept DRIVER_OK (status 0x%02x).\n", dev->pci_device, status);
        return -1;
    }
    return 0;
}

int virtq_setup(virtio_device_t *dev, uint16_t index, uint16_t max_size, virtq_t *vq)
{
    if ((dev == NULL) || (dev->common == NULL) || (vq == NULL)) {
        pr_err("Invalid arguments to virtq_setup.\n");
        return -1;
    }
    memset(vq, 0, sizeof(*vq));

    if (index >= __virtio_r16(dev->common, VIRTIO_PCI_NUM_QUEUES)) {
        pr_err("Virtio device 0x%08x has no queue %u.\n", dev->pci_device, index);
        return -1;
    }

    __virtio_w16(dev->common, VIRTIO_PCI_QUEUE_SELECT, index);
    uint16_t size = __virtio_r16(dev->common, VIRTIO_PCI_QUEUE_SIZE);
    if (size == 0) {
        pr_err("Queue %u of virtio device 0x%08x is unavailable.\n", index, dev->pci_device);
        return -1;
    }
    if ((max_size != 0) && (size > max_size)) {
        // A smaller ring is always legal; the device is told what was chosen.
        size = max_size;
        __virtio_w16(dev->common, VIRTIO_PCI_QUEUE_SIZE, size);
    }

    uint32_t avail_offset = 0;
    uint32_t used_offset  = 0;
    uint32_t total        = 0;
    __virtq_layout(size, &avail_offset, &used_offset, &total);

    uint32_t order = find_nearest_order_greater(0, total);
    page_t *pages  = alloc_pages(GFP_KERNEL, order);
    if (pages == NULL) {
        pr_err("Failed to allocate %u bytes for the rings of queue %u.\n", total, index);
        return -1;
    }
    uint32_t physical = get_physical_address_from_page(pages);
    uint32_t virtual  = get_virtual_address_from_page(pages);
    if (virtual == 0) {
        pr_err("No low-memory address for the rings of queue %u.\n", index);
        free_pages(pages);
        return -1;
    }

    // Zero the whole allocation. The indices the device and the driver compare
    // live here, and starting them from whatever the allocator handed over is
    // the same bug as forgetting to zero .bss, with the same symptom.
    memset((void *)virtual, 0, total);

    vq->index        = index;
    vq->size         = size;
    vq->desc         = (virtq_desc_t *)virtual;
    vq->avail        = (uint8_t *)(virtual + avail_offset);
    vq->used         = (uint8_t *)(virtual + used_offset);
    vq->avail_shadow = 0;
    vq->used_seen    = 0;
    vq->pages        = pages;
    vq->ring_bytes   = total;

    // No notification suppression: the device should always tell us.
    __avail_set_flags(vq->avail, 0);

    __virtio_write_addr(dev->common, VIRTIO_PCI_QUEUE_DESC, physical);
    __virtio_write_addr(dev->common, VIRTIO_PCI_QUEUE_DRIVER, physical + avail_offset);
    __virtio_write_addr(dev->common, VIRTIO_PCI_QUEUE_DEVICE, physical + used_offset);

    uint16_t notify_offset = __virtio_r16(dev->common, VIRTIO_PCI_QUEUE_NOTIFY_OFF);
    vq->notify = (volatile uint16_t *)(dev->notify_base + ((uint32_t)notify_offset * dev->notify_multiplier));

    __virtio_w16(dev->common, VIRTIO_PCI_QUEUE_ENABLE, 1);

    pr_notice(
        "Queue %u ready: %u descriptors, rings at 0x%08x (phys 0x%08x), notify offset %u.\n", index, size, virtual,
        physical, notify_offset);
    return 0;
}

void virtq_free(virtio_device_t *dev, virtq_t *vq)
{
    if ((vq == NULL) || (vq->pages == NULL)) {
        return;
    }
    // Take the queue away from the device before the memory goes back to the
    // allocator, or it may walk rings that belong to something else.
    if ((dev != NULL) && (dev->common != NULL)) {
        __virtio_w16(dev->common, VIRTIO_PCI_QUEUE_SELECT, vq->index);
        __virtio_w16(dev->common, VIRTIO_PCI_QUEUE_ENABLE, 0);
    }
    free_pages(vq->pages);
    memset(vq, 0, sizeof(*vq));
}

/// @name Virtqueue metrics
/// @brief See klib/perf.h. Off unless ENABLE_PERF.
///
/// One submission is one round trip to the device, and the poll rounds are how
/// long the processor sat waiting for it -- which is the part a caller can only
/// reduce by asking for less.
/// @{
PERF_COUNTER(perf_submits, "virtqueue.submit.calls", "calls");
PERF_COUNTER(perf_rounds, "virtqueue.submit.poll_rounds", "rounds");
PERF_COUNTER(perf_submit_cycles, "virtqueue.submit.cycles", PERF_UNIT_CYCLES);
/// @}

int virtq_submit_sync(virtq_t *vq, const virtq_buffer_t *buffers, unsigned count, uint32_t *written)
{
    PERF_INC(perf_submits);
    perf_scope_t scope = PERF_SCOPE_BEGIN();
    if ((vq == NULL) || (vq->desc == NULL) || (buffers == NULL) || (count == 0)) {
        pr_err("Invalid arguments to virtq_submit_sync.\n");
        return -1;
    }
    if (count > vq->size) {
        pr_err("A chain of %u descriptors does not fit a ring of %u.\n", count, vq->size);
        return -1;
    }

    // Descriptors 0..count-1 are used every time. Only one chain is outstanding,
    // so there is no free list to keep.
    for (unsigned index = 0; index < count; ++index) {
        vq->desc[index].addr_low  = buffers[index].phys;
        vq->desc[index].addr_high = 0;
        vq->desc[index].len       = buffers[index].len;
        vq->desc[index].flags     = (uint16_t)((buffers[index].write ? VIRTQ_DESC_F_WRITE : 0U) |
                                          ((index + 1U < count) ? VIRTQ_DESC_F_NEXT : 0U));
        vq->desc[index].next      = (uint16_t)(index + 1U);
    }

    // Ignore anything the device left in the used ring from an earlier request
    // that timed out, so one timeout cannot desynchronise every later call.
    vq->used_seen = __used_idx(vq->used);

    __avail_set_entry(vq->avail, (uint16_t)(vq->avail_shadow % vq->size), 0);
    // The descriptors and the ring entry must be visible before the index that
    // publishes them.
    barrier();
    __avail_set_idx(vq->avail, ++vq->avail_shadow);
    barrier();

    *(vq->notify) = vq->index;

    for (uint32_t round = 0; round < VIRTQ_POLL_ROUNDS; ++round) {
        if ((uint16_t)(__used_idx(vq->used) - vq->used_seen) != 0) {
            uint32_t id     = 0;
            uint32_t length = 0;
            __used_entry(vq->used, (uint16_t)(vq->used_seen % vq->size), &id, &length);
            ++vq->used_seen;
            if (id != 0) {
                pr_err("Queue %u completed descriptor %u, expected 0.\n", vq->index, id);
                PERF_SCOPE_END(perf_submit_cycles, scope);
                return -1;
            }
            if (written != NULL) {
                *written = length;
            }
            PERF_SCOPE_END(perf_submit_cycles, scope);
            return 0;
        }
        cpu_relax();
        PERF_INC(perf_rounds);
    }

    pr_err("Queue %u did not complete a request within %u rounds.\n", vq->index, (unsigned)VIRTQ_POLL_ROUNDS);
    PERF_SCOPE_END(perf_submit_cycles, scope);
    return -2;
}
