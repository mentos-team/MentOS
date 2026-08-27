/// @file virtio.h
/// @brief Minimal modern (virtio 1.0) virtio-pci transport and split virtqueue.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.
///
/// Enough of the transport to drive a device that needs one or two queues and
/// synchronous request/response traffic. There is deliberately no support for
/// MSI-X, packed rings, indirect descriptors, event indices or notification
/// suppression: none of them is offered-and-needed by the devices this kernel
/// talks to, and each would be dead code.
///
/// Completion is **polled**. The transport takes no interrupt of its own; a
/// driver that wants the configuration-change interrupt installs its own handler
/// and uses virtio_read_isr() to find out why it fired.
///
/// ## The one trap
///
/// Every access to a ring field the device also writes -- `used->idx` above all
/// -- must go through a volatile pointer. The device updates it behind the
/// compiler's back, and at -O2 gcc will otherwise hoist the read out of a poll
/// loop and spin on a value that can never change. That presents as a device
/// which answers every request exactly one request late, which is a remarkably
/// confusing thing to debug. The accessors below are volatile for this reason
/// and not incidentally.
/// @addtogroup devices Hardware Interfaces
/// @{
/// @addtogroup virtio Virtio
/// @brief Modern virtio-pci transport.
/// @{

#pragma once

#include "mem/mm/page.h"
#include "stdbool.h"
#include "stdint.h"

/// @name Kernel virtual windows for device registers
/// @brief Where device apertures are mapped, and why here.
///
/// The kernel's linear map starts at 0xC0000000 and covers at most the 896 MB of
/// low memory, so it cannot reach 0xF8000000 -- which is where the linker
/// script's KERNEL_HIGHMEM region begins and where no section is placed. The
/// windows carved out of it so far:
///
///     0xF9000000 - 0xF93FFFFF   VBE linear framebuffer (see vbe_lfb.c)
///     0xFA000000 - 0xFA03FFFF   virtio device registers (here)
///     0xFEC00000                I/O APIC; nothing may be mapped at or past it
/// @{

/// Base of the virtio register window.
#define VIRTIO_MMIO_VIRT_BASE 0xFA000000U
/// Bytes of virtual address space reserved per device.
///
/// The largest structure offset a virtio-pci capability has been observed to
/// use is 0x3000 with a length of 0x1000, so 64 KiB per device is generous.
#define VIRTIO_MMIO_WINDOW    0x00010000U
/// How many devices the transport can map at once.
#define VIRTIO_MMIO_SLOTS     4U
/// @}

/// @name Virtio PCI capability layout
/// @brief `struct virtio_pci_cap`, as offsets from the capability.
/// @{
#define VIRTIO_PCI_CAP_CFG_TYPE     3  ///< Which structure this capability describes.
#define VIRTIO_PCI_CAP_BAR          4  ///< Which BAR it lives in.
#define VIRTIO_PCI_CAP_OFFSET       8  ///< Byte offset within that BAR.
#define VIRTIO_PCI_CAP_LENGTH       12 ///< Length of the structure.
#define VIRTIO_PCI_CAP_NOTIFY_MULT  16 ///< Only for a notify capability.
/// @}

/// @name Virtio PCI configuration structure types
/// @{
#define VIRTIO_PCI_CAP_COMMON_CFG 1 ///< The common configuration structure.
#define VIRTIO_PCI_CAP_NOTIFY_CFG 2 ///< The notification area.
#define VIRTIO_PCI_CAP_ISR_CFG    3 ///< The interrupt status register.
#define VIRTIO_PCI_CAP_DEVICE_CFG 4 ///< The device-specific configuration.
#define VIRTIO_PCI_CAP_PCI_CFG    5 ///< The configuration-space access window.
/// @}

/// @name Common configuration structure offsets
/// @{
#define VIRTIO_PCI_DEVICE_FEATURE_SELECT 0x00 ///< Which 32-bit half of the device features to read.
#define VIRTIO_PCI_DEVICE_FEATURE        0x04 ///< The selected half of the device features.
#define VIRTIO_PCI_DRIVER_FEATURE_SELECT 0x08 ///< Which half of the driver features to write.
#define VIRTIO_PCI_DRIVER_FEATURE        0x0C ///< The selected half of the driver features.
#define VIRTIO_PCI_MSIX_CONFIG           0x10 ///< Configuration-change MSI-X vector.
#define VIRTIO_PCI_NUM_QUEUES            0x12 ///< How many queues the device has.
#define VIRTIO_PCI_DEVICE_STATUS         0x14 ///< The status handshake register.
#define VIRTIO_PCI_CONFIG_GENERATION     0x15 ///< Changes while device config is being updated.
#define VIRTIO_PCI_QUEUE_SELECT          0x16 ///< Which queue the registers below refer to.
#define VIRTIO_PCI_QUEUE_SIZE            0x18 ///< Descriptor count of the selected queue.
#define VIRTIO_PCI_QUEUE_MSIX_VECTOR     0x1A ///< Queue MSI-X vector.
#define VIRTIO_PCI_QUEUE_ENABLE          0x1C ///< Whether the selected queue is live.
#define VIRTIO_PCI_QUEUE_NOTIFY_OFF      0x1E ///< Index into the notification area.
#define VIRTIO_PCI_QUEUE_DESC            0x20 ///< Physical address of the descriptor table.
#define VIRTIO_PCI_QUEUE_DRIVER          0x28 ///< Physical address of the available ring.
#define VIRTIO_PCI_QUEUE_DEVICE          0x30 ///< Physical address of the used ring.
/// @}

/// @name Device status bits
/// @{
#define VIRTIO_STATUS_ACKNOWLEDGE 0x01U ///< The driver has noticed the device.
#define VIRTIO_STATUS_DRIVER      0x02U ///< The driver knows how to drive it.
#define VIRTIO_STATUS_DRIVER_OK   0x04U ///< The driver is ready.
#define VIRTIO_STATUS_FEATURES_OK 0x08U ///< The driver has accepted the feature set.
#define VIRTIO_STATUS_NEEDS_RESET 0x40U ///< The device has given up and wants a reset.
#define VIRTIO_STATUS_FAILED      0x80U ///< The driver has given up.
/// @}

/// @name Transport feature bits
/// @{
#define VIRTIO_F_VERSION_1       32U ///< The device speaks the modern specification.
#define VIRTIO_F_ACCESS_PLATFORM 33U ///< Addresses need platform (IOMMU) translation.
#define VIRTIO_F_RING_PACKED     34U ///< The packed ring layout is available.
/// @}

/// @brief A feature set, as the two 32-bit halves the registers expose.
///
/// Not a `uint64_t`, for the reason given on virtq_desc_t: the type is 32 bits
/// wide here, so a feature above bit 31 -- VIRTIO_F_VERSION_1 among them, the
/// one feature that is mandatory -- would be shifted out of existence.
typedef struct {
    uint32_t low;  ///< Feature bits 0 to 31.
    uint32_t high; ///< Feature bits 32 to 63.
} virtio_features_t;

/// @brief Mask of a feature bit within the low half; 0 if it is in the high half.
#define VIRTIO_FEATURE_LOW(bit)  (((bit) < 32U) ? (1U << ((bit) & 31U)) : 0U)
/// @brief Mask of a feature bit within the high half; 0 if it is in the low half.
#define VIRTIO_FEATURE_HIGH(bit) (((bit) >= 32U) ? (1U << ((bit) & 31U)) : 0U)

/// @name Interrupt status register bits
/// @{
#define VIRTIO_ISR_QUEUE  0x01U ///< A queue has been used.
#define VIRTIO_ISR_CONFIG 0x02U ///< The device configuration has changed.
/// @}

/// @name Descriptor flags
/// @{
#define VIRTQ_DESC_F_NEXT  1U ///< This descriptor chains to `next`.
#define VIRTQ_DESC_F_WRITE 2U ///< The device writes this buffer, rather than reading it.
/// @}

/// @brief One descriptor-table entry.
///
/// The address is split into two 32-bit halves rather than declared `uint64_t`
/// **deliberately**. This project's `lib/inc/stdint.h` defines `uint64_t` as
/// `unsigned int`, so it is 32 bits wide; a `uint64_t addr` field would make
/// this structure 12 bytes instead of the 16 the specification requires, and
/// every descriptor after the first would be read from the wrong offset. Split
/// halves are also what the 32-bit machine actually stores, so nothing is lost
/// by being explicit. The size check below is what keeps this honest.
typedef struct {
    uint32_t addr_low;  ///< Low half of the buffer's guest physical address.
    uint32_t addr_high; ///< High half; always zero on this 32-bit kernel.
    uint32_t len;       ///< Length of the buffer.
    uint16_t flags;     ///< VIRTQ_DESC_F_* flags.
    uint16_t next;      ///< Next descriptor, when VIRTQ_DESC_F_NEXT is set.
} virtq_desc_t;

/// @brief Compile-time check that a descriptor is the size the device expects.
///
/// Fails the build with a negative array size if it ever stops being 16 bytes.
typedef char virtq_desc_size_check[(sizeof(virtq_desc_t) == 16) ? 1 : -1];

/// @brief One used-ring entry.
typedef struct {
    uint32_t id;  ///< Head of the descriptor chain that was used.
    uint32_t len; ///< Bytes written into the chain's device-writable buffers.
} virtq_used_elem_t;

/// @brief A buffer to hand to the device, as one descriptor.
typedef struct {
    uint32_t phys; ///< Guest physical address of the buffer.
    uint32_t len;  ///< Length of the buffer.
    bool_t write;  ///< true when the device writes it, false when it reads it.
} virtq_buffer_t;

/// @brief A set-up split virtqueue.
///
/// The three rings live in one physically contiguous allocation, because the
/// device is given their physical addresses and there is no scatter/gather for
/// the rings themselves.
typedef struct {
    uint16_t index;               ///< Queue index on the device.
    uint16_t size;                ///< Number of descriptors.
    virtq_desc_t *desc;           ///< Descriptor table.
    uint8_t *avail;               ///< Available ring, as bytes: the layout is not a fixed-size struct.
    uint8_t *used;                ///< Used ring, likewise.
    uint16_t avail_shadow;        ///< Our own copy of the available ring's index.
    uint16_t used_seen;           ///< Last used-ring entry consumed.
    volatile uint16_t *notify;    ///< Where to write this queue's index to kick it.
    page_t *pages;                ///< Backing pages of the ring allocation, for freeing.
    uint32_t ring_bytes;          ///< Size of that allocation.
} virtq_t;

/// @brief A virtio-pci device the transport has taken over.
typedef struct {
    uint32_t pci_device;            ///< Boxed PCI bus/slot/function.
    volatile uint8_t *common;       ///< The common configuration structure.
    volatile uint8_t *notify_base;  ///< Base of the notification area.
    volatile uint8_t *isr;          ///< The interrupt status register.
    volatile uint8_t *device_config;///< The device-specific configuration structure.
    uint32_t notify_multiplier;     ///< Scale applied to a queue's notify offset.
    virtio_features_t features;     ///< The feature set actually negotiated.
    unsigned window_slot;           ///< Which mapping slot this device occupies.
} virtio_device_t;

/// @brief Finds and maps a modern virtio-pci device's register structures.
/// @param pci_device The boxed PCI device identifier.
/// @param dev Where the device handle is built.
/// @return 0 on success, -1 on failure.
///
/// Enables memory space and bus mastering, walks the capability list for the
/// four structures the transport needs, and maps the BARs they live in. Does not
/// touch the device beyond that, so a failure here leaves it as it was found.
int virtio_pci_setup(uint32_t pci_device, virtio_device_t *dev);

/// @brief Releases a device's register mappings.
/// @param dev The device handle.
///
/// Does not reset the device; call virtio_reset() first if that is wanted.
void virtio_pci_release(virtio_device_t *dev);

/// @brief Resets the device and negotiates features.
/// @param dev The device handle.
/// @param wanted Feature bits to accept if the device offers them.
/// @param required Feature bits the driver cannot work without.
/// @return 0 on success, -1 when a required feature is missing or the device
///         rejects the negotiated set.
///
/// On success the device is in DRIVER|FEATURES_OK and `dev->features` holds what
/// was agreed. Queues may then be set up; virtio_driver_ok() finishes the
/// handshake.
int virtio_negotiate(virtio_device_t *dev, virtio_features_t wanted, virtio_features_t required);

/// @brief Whether a feature was negotiated.
/// @param dev The device handle.
/// @param bit The feature bit number.
/// @return true when the driver and the device both agreed to it.
bool_t virtio_has_feature(const virtio_device_t *dev, unsigned bit);

/// @brief Completes the handshake, after the queues are configured.
/// @param dev The device handle.
/// @return 0 on success, -1 if the device did not accept it.
int virtio_driver_ok(virtio_device_t *dev);

/// @brief Puts the device back into its reset state.
/// @param dev The device handle.
///
/// Writing 0 to the status register is the reset; the device is reset once it
/// reads back as 0. This is also the only way back from a device that has
/// signalled NEEDS_RESET.
void virtio_reset(virtio_device_t *dev);

/// @brief Reads and acknowledges the interrupt status register.
/// @param dev The device handle.
/// @return The VIRTIO_ISR_* bits that were set.
///
/// Reading the register clears it, so the result must be acted on. Cheap enough
/// to call from an interrupt handler, which is its only intended caller.
uint8_t virtio_read_isr(const virtio_device_t *dev);

/// @brief Reads the device status register.
/// @param dev The device handle.
/// @return The VIRTIO_STATUS_* bits currently set.
uint8_t virtio_read_status(const virtio_device_t *dev);

/// @brief Allocates and publishes a virtqueue.
/// @param dev The device handle.
/// @param index Queue index on the device.
/// @param max_size Largest ring the caller is willing to use; 0 accepts the
///                 device's own size.
/// @param vq Where the queue handle is built.
/// @return 0 on success, -1 on failure.
///
/// Must be called after virtio_negotiate() and before virtio_driver_ok().
int virtq_setup(virtio_device_t *dev, uint16_t index, uint16_t max_size, virtq_t *vq);

/// @brief Frees a virtqueue's rings.
/// @param dev The device handle.
/// @param vq The queue handle.
///
/// Disables the queue on the device first, so the device cannot walk rings that
/// have been handed back to the allocator.
void virtq_free(virtio_device_t *dev, virtq_t *vq);

/// @brief Submits a descriptor chain and waits for it to complete.
/// @param vq The queue.
/// @param buffers The buffers making up the chain, in order.
/// @param count How many buffers.
/// @param[out] written Bytes the device reported writing; may be NULL.
/// @return 0 on success, -1 on a malformed request, -2 on timeout.
///
/// Synchronous and polled: it must therefore be called with interrupts in their
/// normal state and never from an interrupt handler, and never while holding
/// anything the rest of the kernel needs.
///
/// One chain is outstanding at a time, which is all a console needs and which is
/// what keeps the descriptor bookkeeping to a shadow index.
int virtq_submit_sync(virtq_t *vq, const virtq_buffer_t *buffers, unsigned count, uint32_t *written);

/// @brief Finds the first PCI device matching a virtio device type.
/// @param device_type The virtio device type, e.g. 16 for a GPU.
/// @param[out] pci_device Where the boxed PCI identifier is stored.
/// @return 0 when found, 1 when absent, -1 on error.
///
/// Modern virtio devices use PCI device ID 0x1040 + device_type with vendor
/// 0x1AF4. Transitional devices use the legacy IDs, which this deliberately
/// does not accept: the transport here only speaks the modern specification.
int virtio_pci_find(uint16_t device_type, uint32_t *pci_device);

/// @}
/// @}
