/// @file virtio_gpu.c
/// @brief Virtio-gpu 2D video backend: one scanout, 32 bpp, promoted at runtime.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.
///
/// A second materialization of the console, brought up after the machine has
/// already booted on another one. It is never the boot backend: virtio needs
/// PCI capability walking, page allocation and kernel mappings, none of which
/// exist at video_init() time.
///
/// The console is rendered into an ordinary memory framebuffer which the device
/// is given as a 2D resource. Unlike the VBE backend there is no aperture and no
/// palette: the resource is 32 bpp and the attribute nibbles index a small table
/// of pixel values, so a cell scan line is eight 32-bit stores into RAM.
///
/// ## Why the framebuffer is scattered but looks linear
///
/// The device accepts a resource backed by a **list** of physical ranges, which
/// is what makes this affordable: a 1024x768x32 framebuffer is 3 MiB, and
/// demanding one contiguous buddy block that size would be fragile. So it is
/// allocated as several blocks and handed over as several memory entries.
///
/// The CPU still needs it linear, though, or every pixel write would have to
/// work out which block it lands in. So the blocks are also mapped
/// consecutively into a fixed kernel virtual window: the device reads a
/// scattered resource while the drawing code writes one flat array.
///
/// ## Two things this backend deliberately does not do
///
/// **It does not blink the cursor.** A drawn cursor only becomes visible after a
/// transfer and a flush, which are device round trips, and those may not happen
/// in the timer interrupt -- see the re-entrancy note below. This kernel has no
/// deferred-work facility to hand them to either, so `cursor_blink` is NULL and
/// the cursor is steady. It is still drawn, moved and removed exactly as the
/// other graphical backend's is.
///
/// **It does not use the cursor queue.** The console draws its own cursor into
/// the framebuffer, as both other graphical backends do, so the hardware cursor
/// plane would buy nothing and would need a second queue.
///
/// ## Re-entrancy
///
/// `printf` reaches the console from any context, including an interrupt. A
/// virtqueue submission is polled and holds one outstanding chain, so being
/// re-entered half way through one would corrupt the ring. Every submission is
/// therefore guarded: a re-entrant caller draws into the framebuffer as usual,
/// records the damage, and returns without touching the device. The next
/// submission that is not re-entered picks the damage up. Nothing is lost; the
/// display is at worst one console operation behind.

// Setup the logging for this file (do this before any other include).
#include "sys/kernel_levels.h"           // Include kernel log levels.
#define __DEBUG_HEADER__ "[VIRTGPU]"     ///< Change header.
#define __DEBUG_LEVEL__  LOGLEVEL_NOTICE ///< Set log level.
#include "io/debug.h"                    // Include debugging functions.

#include "io/video/virtio_gpu.h"

#include "descriptor_tables/isr.h"
#include "devices/pci.h"
#include "devices/virtio.h"
#include "drivers/keyboard/keyboard.h"
#include "hardware/pic8259.h"
#include "klib/irqflags.h"
#include "mem/alloc/zone_allocator.h"
#include "mem/mm/page.h"
#include "mem/mm/vm_area.h"
#include "mem/paging.h"
#include "io/video.h"
#include "io/video_backend.h"
#include "stdbool.h"
#include "stddef.h"
#include "string.h"

#include "io/video/video_font.h"
#include "video_palette_16.h"


/// @brief Virtio device type of a GPU.
#define VIRTIO_ID_GPU                     16U

/// @name Control commands
/// @{
#define VIRTIO_GPU_CMD_GET_DISPLAY_INFO   0x0100U ///< Ask for the scanout geometry.
#define VIRTIO_GPU_CMD_RESOURCE_CREATE_2D 0x0101U ///< Create a 2D resource.
#define VIRTIO_GPU_CMD_RESOURCE_UNREF     0x0102U ///< Destroy a resource.
#define VIRTIO_GPU_CMD_SET_SCANOUT        0x0103U ///< Point a scanout at a resource.
#define VIRTIO_GPU_CMD_RESOURCE_FLUSH     0x0104U ///< Show what was transferred.
#define VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D 0x0105U ///< Copy guest memory into the resource.
#define VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING 0x0106U ///< Give a resource its backing pages.
/// @}

/// @name Responses
/// @{
#define VIRTIO_GPU_RESP_OK_NODATA       0x1100U ///< Success, nothing returned.
#define VIRTIO_GPU_RESP_OK_DISPLAY_INFO 0x1101U ///< Success, display info follows.
/// @}

/// @brief Pixel format: bytes B, G, R, unused.
///
/// On a little-endian machine that is a uint32_t of 0x00RRGGBB, which is the
/// most convenient value to compose, and it is the format QEMU wants natively.
#define VIRTIO_GPU_FORMAT_B8G8R8X8_UNORM 2U

/// @brief Largest number of scanouts this backend will look at.
#define VIRTIO_GPU_MAX_SCANOUTS          16U

/// @name Device configuration offsets
/// @{
#define VIRTIO_GPU_CFG_EVENTS_READ  0x00 ///< Pending event bits.
#define VIRTIO_GPU_CFG_EVENTS_CLEAR 0x04 ///< Write to acknowledge them.
#define VIRTIO_GPU_CFG_NUM_SCANOUTS 0x08 ///< How many scanouts the device has.
/// @}

/// @brief The scanout this backend drives. There is only ever one.
#define VIRTIO_GPU_SCANOUT          0U
/// @brief The resource id this backend uses. Ids are the driver's to choose.
#define VIRTIO_GPU_RESOURCE         1U


/// @brief Largest width or height this backend will build, in pixels.
///
/// One bound for both axes on purpose. A per-axis pair would refuse a portrait
/// 2160x3840 while accepting 3840x2160, which is an orientation restriction
/// wearing a resource limit's clothes. What actually limits a framebuffer is how
/// many bytes it needs against the window mapped below, and that is checked
/// separately and on its own terms.
///
/// This bound's job is to keep the byte arithmetic from overflowing. It is
/// applied before width and height are ever multiplied together, and
/// 3840 * 3840 * 4 is 56 MiB -- comfortably inside 32 bits -- so the
/// multiplication that follows it cannot wrap whatever the device reports.
#define GPU_MAX_DIMENSION 3840U

/// @brief The largest mode this backend is expected to be able to build.
///
/// 4K at 32 bpp. Not a limit -- GPU_MAX_DIMENSION and the window are the limits
/// -- but a statement of intent that the check below holds the window to.
#define GPU_LARGEST_MODE_BYTES (3840U * 2160U * 4U)

/// @brief Kernel virtual base the framebuffer blocks are mapped consecutively at.
///
/// A third window in the unused KERNEL_HIGHMEM region; see the map in
/// devices/virtio.h.
#define GPU_FB_VIRT_BASE  0xFB000000U
/// @brief Size of that window.
///
/// 32 MiB, which holds 3840x2160 at 32 bpp (31.6 MiB) and therefore every
/// common widescreen and high-DPI mode below it: 2560x1440 needs 14.1 MiB,
/// 2560x1600 15.6 MiB and 3440x1440 18.9 MiB.
///
/// The region would take considerably more. This base is 0xFB000000 and nothing
/// may be mapped at or past the I/O APIC at 0xFEC00000, which leaves room for 60
/// MiB; the check below pins both ends. 32 MiB is what the modes above need, not
/// what fits.
#define GPU_FB_VIRT_SIZE  0x02000000U

/// @brief Most blocks the framebuffer may be split into.
///
/// Each becomes one memory entry in the backing list. A 4 MiB block is the
/// largest this takes, so a 32 MiB framebuffer needs eight of them when memory
/// is unfragmented. It is not: at 1024x768, where three would do, fragmentation
/// was measured pushing it to seven. Scaling that ratio to the largest supported
/// mode is where thirty-two comes from. The compile-time check below keeps the
/// resulting backing list inside the command page.
#define GPU_FB_MAX_BLOCKS 32U

/// @brief Largest allocation order used for a framebuffer block.
///
/// The buddy allocator tops out at MAX_BUDDYSYSTEM_GFP_ORDER; staying a little
/// below it avoids leaning on the single largest block the system has.
#define GPU_FB_MAX_ORDER  10U

/// @brief Number of scan lines the underline cursor covers.
#define GPU_CURSOR_UNDERLINE_LINES 3U
/// @brief Pixel mask of the bar cursor: the two leftmost columns of the cell.
#define GPU_CURSOR_BAR_COLUMNS     2U

/// @brief Compile-time checks on the window and the bound that guards it.
///
/// Four separate claims, all of which have to hold:
///
///  - the window starts above the virtio register window, which is the nearest
///    thing mapped below it, so the two cannot overlap;
///  - it ends at or below the I/O APIC, which nothing may be mapped at or past;
///  - the largest mode this backend claims to support actually fits in it;
///  - and GPU_MAX_DIMENSION is small enough that squaring it and multiplying by
///    four stays inside 32 bits, which is what makes the runtime size arithmetic
///    safe once that bound has been applied.
typedef char gpu_window_check
    [((GPU_FB_VIRT_BASE >= (VIRTIO_MMIO_VIRT_BASE + (VIRTIO_MMIO_SLOTS * VIRTIO_MMIO_WINDOW))) &&
      ((GPU_FB_VIRT_BASE + GPU_FB_VIRT_SIZE) <= 0xFEC00000U) &&
      (GPU_LARGEST_MODE_BYTES <= GPU_FB_VIRT_SIZE) &&
      (GPU_MAX_DIMENSION <= ((0xFFFFFFFFU / 4U) / GPU_MAX_DIMENSION)))
         ? 1
         : -1];

/// @brief One header, shared by every control command and response.
typedef struct {
    uint32_t type;     ///< Command or response code.
    uint32_t flags;    ///< Request flags; unused here.
    uint32_t fence_lo; ///< Fence id, low half. Split for the reason in virtio.h.
    uint32_t fence_hi; ///< Fence id, high half.
    uint32_t ctx_id;   ///< 3D context; always zero.
    uint8_t ring_idx;  ///< Ring index; always zero.
    uint8_t padding[3];///< Padding to a 24-byte header.
} gpu_ctrl_hdr_t;

/// @brief Compile-time check that the header is the size the device expects.
typedef char gpu_hdr_size_check[(sizeof(gpu_ctrl_hdr_t) == 24) ? 1 : -1];

/// @brief A rectangle, as the protocol expresses one.
typedef struct {
    uint32_t x;      ///< Left edge.
    uint32_t y;      ///< Top edge.
    uint32_t width;  ///< Width.
    uint32_t height; ///< Height.
} gpu_rect_t;

/// @brief VIRTIO_GPU_CMD_RESOURCE_CREATE_2D payload.
typedef struct {
    gpu_ctrl_hdr_t hdr;   ///< Command header.
    uint32_t resource_id; ///< Id to give the new resource.
    uint32_t format;      ///< Pixel format.
    uint32_t width;       ///< Width in pixels.
    uint32_t height;      ///< Height in pixels.
} gpu_create_2d_t;

/// @brief One entry of a resource's backing list.
typedef struct {
    uint32_t addr_lo; ///< Physical address, low half.
    uint32_t addr_hi; ///< Physical address, high half; zero on this kernel.
    uint32_t length;  ///< Length of the range.
    uint32_t padding; ///< Padding to 16 bytes.
} gpu_mem_entry_t;

/// @brief VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING payload.
typedef struct {
    gpu_ctrl_hdr_t hdr;   ///< Command header.
    uint32_t resource_id; ///< Resource to back.
    uint32_t nr_entries;  ///< How many entries follow.
    gpu_mem_entry_t entries[GPU_FB_MAX_BLOCKS]; ///< The ranges themselves.
} gpu_attach_backing_t;

/// @brief VIRTIO_GPU_CMD_SET_SCANOUT payload.
typedef struct {
    gpu_ctrl_hdr_t hdr;   ///< Command header.
    gpu_rect_t rect;      ///< Region of the resource to display.
    uint32_t scanout_id;  ///< Which scanout.
    uint32_t resource_id; ///< Which resource, or 0 to disable.
} gpu_set_scanout_t;

/// @brief VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D payload.
typedef struct {
    gpu_ctrl_hdr_t hdr;   ///< Command header.
    gpu_rect_t rect;      ///< Region to copy.
    uint32_t offset_lo;   ///< Offset into the backing, low half.
    uint32_t offset_hi;   ///< Offset into the backing, high half.
    uint32_t resource_id; ///< Which resource.
    uint32_t padding;     ///< Padding.
} gpu_transfer_2d_t;

/// @brief VIRTIO_GPU_CMD_RESOURCE_FLUSH payload.
typedef struct {
    gpu_ctrl_hdr_t hdr;   ///< Command header.
    gpu_rect_t rect;      ///< Region to show.
    uint32_t resource_id; ///< Which resource.
    uint32_t padding;     ///< Padding.
} gpu_flush_t;

/// @brief VIRTIO_GPU_RESP_OK_DISPLAY_INFO payload.
typedef struct {
    gpu_ctrl_hdr_t hdr; ///< Response header.
    struct {
        gpu_rect_t rect;  ///< Geometry the host wants for this scanout.
        uint32_t enabled; ///< Whether it is in use.
        uint32_t flags;   ///< Scanout flags.
    } pmodes[VIRTIO_GPU_MAX_SCANOUTS]; ///< One entry per scanout.
} gpu_display_info_t;

/// @brief One block of the framebuffer's backing.
typedef struct {
    page_t *pages;    ///< The allocation, for freeing.
    uint32_t physical;///< Its physical base.
    uint32_t bytes;   ///< How many bytes of it the framebuffer uses.
} gpu_block_t;

/// @name The three separate quantities
/// @brief Kept apart on purpose, because two of them move independently.
///
/// The scanout size in pixels is what the host is showing. The font decides how
/// many pixels a cell takes. The cell counts follow from dividing one by the
/// other, and they are the only one of the three the generic console knows
/// about. A display change moves the first, a font change moves the second, and
/// either way the third is recomputed rather than assumed -- which is what keeps
/// a font change from disturbing the scanout, and a window resize from
/// forgetting which font was chosen.
/// @{
static unsigned gpu_target_width  = 0; ///< Scanout width the host wants filled.
static unsigned gpu_target_height = 0; ///< Scanout height the host wants filled.

static const video_font_t *gpu_font = NULL; ///< Font the cells are drawn with.
static unsigned gpu_cell_width      = 0;    ///< Pixels a cell occupies across.
static unsigned gpu_cell_height     = 0;    ///< Scan lines a cell occupies.

static unsigned gpu_columns   = 0; ///< Console width in cells.
static unsigned gpu_rows      = 0; ///< Console height in cells.
static unsigned gpu_width     = 0; ///< Framebuffer width in pixels.
static unsigned gpu_height    = 0; ///< Framebuffer height in scan lines.
static unsigned gpu_stride    = 0; ///< Bytes per scan line.
static unsigned gpu_row_bytes = 0; ///< Bytes one text row occupies.
static unsigned gpu_fb_bytes  = 0; ///< Bytes the framebuffer occupies.
/// @}

/// @brief Whether the scanout has ever been pointed at one of our resources.
static bool_t scanout_ever_set        = false;

/// @brief Whether the display change the hand-over provokes is still expected.
///
/// Taking the console over replaces the surface the host was showing, and QEMU
/// answers that with a display-change event of its own. Under `-display gtk` it
/// reported 640x480 while the console was displaying 1024x768 -- once, right
/// after the first SET_SCANOUT, with nobody having touched the window. Acting on
/// it undoes the decision made moments earlier to carry the compiled geometry
/// through the hand-over, and it does so destructively.
///
/// Set where that event is caused, which is the first time the scanout is
/// pointed at us, and consumed by the first display change serviced afterwards.
/// A one-shot tied to the transition rather than a window of time: there is
/// nothing to tune and nothing that can fire twice.
static bool_t promotion_echo_pending  = false;

/// @brief Font a geometry change in progress is being built for.
///
/// Set only for the duration of one video_change_geometry() call, and cleared
/// whatever the outcome. A font is staged rather than adopted because the
/// geometry the console ends up with is not this backend's to decide: the
/// allocation may fail, or the cell counts may be outside what the console
/// supports, and the font that is drawn with has to be the one the framebuffer
/// on the scanout was built for. Clearing it on the way out is what stops a
/// refused font from leaking into the next display resize.
static const video_font_t *gpu_pending_font = NULL;

/// @brief Whether the whole framebuffer still has to reach the device.
///
/// Damage is tracked in cell rows, which cannot describe the margin left over
/// when the cells do not divide the scanout exactly. After a geometry change
/// there is a fresh resource and possibly such a margin, so the first transfer
/// covers everything instead.
static bool_t transfer_all = false;

/// @brief Whether the backend can touch the device and the framebuffer.
static bool_t active                = false;
/// @brief Whether the scanout has been pointed at our resource yet.
///
/// Deliberately deferred past bring-up: the switch happens on the first flush,
/// by which time the generic layer's repaint has put real content in the
/// resource. Setting it during bring-up would show an empty screen first.
static bool_t scanout_set           = false;

/// @brief The device.
static virtio_device_t gpu;
/// @brief Its control queue.
static virtq_t controlq;

/// @brief One page staging every request and response.
static page_t *command_pages        = NULL;
/// @brief Physical address of that page.
static uint32_t command_physical    = 0;
/// @brief Kernel virtual address of that page.
static uint32_t command_virtual     = 0;

/// @brief Offset of the response within the command page.
#define GPU_RESPONSE_OFFSET 2048U

/// @brief Compile-time check that the longest request still fits before it.
///
/// The backing list is the longest command this backend sends, and it grows with
/// GPU_FB_MAX_BLOCKS. Requests are written at the start of the command page and
/// the response is read from GPU_RESPONSE_OFFSET, so a request that reached that
/// far would be overwritten by the answer to it.
typedef char gpu_request_fits_check[(sizeof(gpu_attach_backing_t) <= GPU_RESPONSE_OFFSET) ? 1 : -1];

/// @brief The resource currently backing the console.
static uint32_t resource_id         = VIRTIO_GPU_RESOURCE;

/// @name The previous resource, waiting to be released
/// @brief Kept alive until the next resize, not freed at the switch.
///
/// The scanout switch happens in __gpu_publish(), which is reachable from a
/// `printf` in an interrupt handler, and the page allocator is not
/// interrupt-safe. So the old resource and its pages are released at the start of
/// the *next* resize instead. At most two framebuffers are live at once, which is
/// bounded rather than leaked.
/// @{
static uint32_t retired_resource    = 0;
static gpu_block_t retired_blocks[GPU_FB_MAX_BLOCKS];
static unsigned retired_count       = 0;
/// @}

/// @brief The framebuffer's backing blocks.
static gpu_block_t blocks[GPU_FB_MAX_BLOCKS];
/// @brief How many of them are in use.
static unsigned block_count         = 0;
/// @brief The framebuffer, linear in kernel virtual space.
static uint8_t *framebuffer         = NULL;

/// @brief Attribute nibble to pixel value.
static uint32_t palette[16];

/// @brief Whether any rows are waiting to be sent to the device.
static bool_t damage_valid          = false;
/// @brief First cell row of the pending damage.
static unsigned damage_first        = 0;
/// @brief Last cell row of the pending damage, inclusive.
static unsigned damage_last         = 0;

/// @brief Whether a submission is in progress; see the re-entrancy note above.
static bool_t submit_busy           = false;

/// @name The display-change event path
/// @{
/// @brief Set by the interrupt handler; means "the geometry needs re-reading".
///
/// A flag rather than a queue of sizes, because the event carries no dimensions
/// and several can arrive in a burst. Coalescing them into one "stale" condition
/// and then asking the device once is both simpler and more correct than trying
/// to replay them.
static volatile bool_t geometry_stale = false;
/// @brief The interrupt line the device is wired to, or 0 if not installed.
static uint8_t gpu_irq_line           = 0;
/// @}

/// @brief Column the cursor is drawn at.
static unsigned cursor_column       = 0;
/// @brief Row the cursor is drawn at.
static unsigned cursor_row          = 0;
/// @brief Current cursor style.
static video_cursor_style_t cursor_style = {VIDEO_CURSOR_BLOCK, true};
/// @brief The cell underneath the cursor; the only state duplicated from the console.
static video_cell_t cursor_cell     = {' ', 0x07};
/// @brief Whether an overlay is currently on the display.
static bool_t cursor_drawn          = false;

/// @brief Adopts a font, caching the cell size it draws at.
/// @param font The font to draw with.
static void __gpu_set_font(const video_font_t *font)
{
    gpu_font        = font;
    gpu_cell_width  = video_font_width(font);
    gpu_cell_height = video_font_height(font);
}

/// @brief Works out the pixel geometry a cell geometry implies.
/// @param columns Width in cells.
/// @param rows Height in cells.
/// @return 0 when it is a geometry this backend can build, -1 otherwise.
///
/// The single place cells become pixels. The framebuffer covers the union of the
/// cell area and the target scanout: normally the cells fit inside the scanout
/// with at most a cell of margin, and covering the target rather than the cells
/// is what lets the scanout stay exactly the size the host asked for while the
/// font changes underneath it. A cell area larger than the target -- which only
/// a geometry asked for directly can produce -- grows the framebuffer instead.
///
/// Two bounds, in this order and for different reasons. GPU_MAX_DIMENSION is
/// applied first and is what makes the multiplication that follows it safe: the
/// target comes from the device and is not otherwise bounded, so it has to be
/// rejected before it is ever squared. The window size is the real limit, and it
/// is a byte count because that is what a framebuffer actually consumes -- one
/// bound per axis would let a mode through that no window could hold, and would
/// refuse a portrait mode that fits perfectly.
static int __gpu_set_dimensions(unsigned columns, unsigned rows)
{
    if ((columns == 0U) || (rows == 0U) || (gpu_font == NULL)) {
        return -1;
    }
    unsigned cells_width  = columns * gpu_cell_width;
    unsigned cells_height = rows * gpu_cell_height;
    unsigned width        = (cells_width > gpu_target_width) ? cells_width : gpu_target_width;
    unsigned height       = (cells_height > gpu_target_height) ? cells_height : gpu_target_height;
    if ((width > GPU_MAX_DIMENSION) || (height > GPU_MAX_DIMENSION)) {
        pr_err("%ux%u cells of %ux%u is %ux%u pixels, past the %u this backend builds in either axis.\n", columns,
               rows, gpu_cell_width, gpu_cell_height, width, height, (unsigned)GPU_MAX_DIMENSION);
        return -1;
    }
    unsigned bytes = (width * 4U) * height;
    if (bytes > GPU_FB_VIRT_SIZE) {
        pr_err("%ux%u pixels needs %u KiB, past the %u KiB this backend can map.\n", width, height, bytes / 1024U,
               (unsigned)(GPU_FB_VIRT_SIZE / 1024U));
        return -1;
    }

    gpu_columns   = columns;
    gpu_rows      = rows;
    gpu_width     = width;
    gpu_height    = height;
    gpu_stride    = width * 4U;
    gpu_row_bytes = gpu_stride * gpu_cell_height;
    gpu_fb_bytes  = gpu_stride * height;
    return 0;
}

/// @brief Interrupt handler for the device; defined below, needed by teardown.
static void virtio_gpu_isr(pt_regs_t *registers);

/// @brief Marks cell rows as needing to reach the device.
/// @param first First row.
/// @param last Last row, inclusive.
static void __gpu_damage(unsigned first, unsigned last)
{
    if (last >= gpu_rows) {
        last = gpu_rows - 1U;
    }
    if (first > last) {
        return;
    }
    if (!damage_valid) {
        damage_first = first;
        damage_last  = last;
        damage_valid = true;
        return;
    }
    if (first < damage_first) {
        damage_first = first;
    }
    if (last > damage_last) {
        damage_last = last;
    }
}

/// @brief Sends one command and checks its response.
/// @param length Bytes of the request, already built at the command page.
/// @param expected The response code that means success.
/// @return 0 on success, -1 on failure.
static int __gpu_command(uint32_t length, uint32_t expected)
{
    gpu_ctrl_hdr_t *response = (gpu_ctrl_hdr_t *)(command_virtual + GPU_RESPONSE_OFFSET);
    virtq_buffer_t buffers[2];

    memset(response, 0, sizeof(gpu_display_info_t));

    buffers[0].phys  = command_physical;
    buffers[0].len   = length;
    buffers[0].write = false;
    buffers[1].phys  = command_physical + GPU_RESPONSE_OFFSET;
    buffers[1].len   = sizeof(gpu_display_info_t);
    buffers[1].write = true;

    int result = virtq_submit_sync(&controlq, buffers, 2, NULL);
    if (result < 0) {
        pr_err("Command 0x%04x did not complete (%d).\n", ((gpu_ctrl_hdr_t *)command_virtual)->type, result);
        return -1;
    }
    if (response->type != expected) {
        pr_err("Command 0x%04x answered 0x%04x, expected 0x%04x.\n", ((gpu_ctrl_hdr_t *)command_virtual)->type,
               response->type, expected);
        return -1;
    }
    return 0;
}

/// @brief Fills in a command header at the command page.
/// @param type The command code.
static void __gpu_header(uint32_t type)
{
    gpu_ctrl_hdr_t *header = (gpu_ctrl_hdr_t *)command_virtual;
    memset(header, 0, sizeof(*header));
    header->type = type;
}

/// @brief Sends the pending damage to the device and shows it.
///
/// Also performs the very first SET_SCANOUT, so the display only moves to this
/// backend once the resource holds a rendered console.
///
/// Skips the device entirely if a submission is already in progress, leaving the
/// damage pending for the next call; see the re-entrancy note at the top.
/// @brief Depth of the output batch, zero when none.
///
/// While it is non-zero the damage keeps accumulating and nothing is sent: the
/// framebuffer is guest RAM, so drawing into it costs nothing a device round trip
/// would not cost far more of.
static unsigned gpu_batch_depth = 0;

static void virtio_gpu_begin_batch(void) { ++gpu_batch_depth; }

static void __gpu_publish(void);

static void virtio_gpu_end_batch(void)
{
    if (gpu_batch_depth > 0) {
        --gpu_batch_depth;
    }
    if (gpu_batch_depth == 0) {
        __gpu_publish();
    }
}

static void __gpu_publish(void)
{
    // Inside a batch the damage is recorded and left for its end. Nothing is
    // lost: __gpu_damage() has already merged it.
    if (gpu_batch_depth > 0) {
        return;
    }
    if (!active || !damage_valid) {
        return;
    }

    // Claim the submission path, or give up and leave the damage recorded.
    uint8_t flags = irq_disable();
    if (submit_busy) {
        irq_enable(flags);
        return;
    }
    submit_busy = true;
    irq_enable(flags);

    unsigned first = damage_first;
    unsigned last  = damage_last;
    damage_valid   = false;

    uint32_t y      = first * gpu_cell_height;
    uint32_t height = ((last - first) + 1U) * gpu_cell_height;
    uint32_t offset = first * gpu_row_bytes;

    // A fresh resource, or cells that do not divide the scanout exactly, leave
    // pixels no cell row covers. Send the lot once rather than teach damage
    // tracking about a margin it will only ever describe as "all of it".
    if (transfer_all) {
        y      = 0;
        height = gpu_height;
        offset = 0;
    }

    gpu_transfer_2d_t *transfer = (gpu_transfer_2d_t *)command_virtual;
    __gpu_header(VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D);
    transfer->rect.x      = 0;
    transfer->rect.y      = y;
    transfer->rect.width  = gpu_width;
    transfer->rect.height = height;
    transfer->offset_lo   = offset;
    transfer->offset_hi   = 0;
    transfer->resource_id = resource_id;
    transfer->padding     = 0;
    if (__gpu_command(sizeof(*transfer), VIRTIO_GPU_RESP_OK_NODATA) < 0) {
        // Put the damage back: it never reached the device.
        __gpu_damage(first, last);
        submit_busy = false;
        return;
    }
    transfer_all = false;

    if (!scanout_set) {
        gpu_set_scanout_t *scanout = (gpu_set_scanout_t *)command_virtual;
        __gpu_header(VIRTIO_GPU_CMD_SET_SCANOUT);
        scanout->rect.x      = 0;
        scanout->rect.y      = 0;
        scanout->rect.width  = gpu_width;
        scanout->rect.height = gpu_height;
        scanout->scanout_id  = VIRTIO_GPU_SCANOUT;
        scanout->resource_id = resource_id;
        if (__gpu_command(sizeof(*scanout), VIRTIO_GPU_RESP_OK_NODATA) < 0) {
            pr_err("Failed to point the scanout at the resource.\n");
            submit_busy = false;
                return;
        }
        scanout_set = true;
        if (!scanout_ever_set) {
            scanout_ever_set       = true;
            promotion_echo_pending = true;
        }
        pr_notice("Scanout %u now shows resource %u (%ux%u).\n", (unsigned)VIRTIO_GPU_SCANOUT, resource_id, gpu_width,
                  gpu_height);
    }

    gpu_flush_t *flush = (gpu_flush_t *)command_virtual;
    __gpu_header(VIRTIO_GPU_CMD_RESOURCE_FLUSH);
    flush->rect.x      = 0;
    flush->rect.y      = y;
    flush->rect.width  = gpu_width;
    flush->rect.height = height;
    flush->resource_id = resource_id;
    flush->padding     = 0;
    (void)__gpu_command(sizeof(*flush), VIRTIO_GPU_RESP_OK_NODATA);

    submit_busy = false;
}

/// @brief Draws one cell, into memory only.
/// @param column The column.
/// @param row The row.
/// @param character The character to draw.
/// @param attribute Foreground index in the low nibble, background in the high.
/// @param overlay Cursor shape to draw over the glyph, or VIDEO_CURSOR_HIDDEN.
///
/// The overlay is folded in here rather than by handing this function a doctored
/// bitmap, because at a magnified size the shapes are no longer expressible as
/// one byte per scan line: an underline is as many scan lines thick as the
/// magnification, and a bar as many pixels wide.
static void __gpu_draw_cell(
    unsigned column,
    unsigned row,
    uint8_t character,
    uint8_t attribute,
    video_cursor_shape_t overlay)
{
    uint32_t foreground = palette[attribute & 0x0FU];
    uint32_t background = palette[(attribute >> 4U) & 0x0FU];
    uint8_t *base       = framebuffer + ((size_t)row * gpu_row_bytes) + ((size_t)column * gpu_cell_width * 4U);
    uint32_t solid      = (gpu_cell_width >= 32U) ? 0xFFFFFFFFU : ((1U << gpu_cell_width) - 1U);
    unsigned scale      = gpu_font->scale;
    unsigned underline  = GPU_CURSOR_UNDERLINE_LINES * scale;
    unsigned bar        = GPU_CURSOR_BAR_COLUMNS * scale;

    for (unsigned line = 0; line < gpu_cell_height; ++line) {
        uint32_t bits = video_font_scanline(gpu_font, character, line);
        if (overlay == VIDEO_CURSOR_BLOCK) {
            bits = solid;
        } else if ((overlay == VIDEO_CURSOR_UNDERLINE) && ((line + underline) >= gpu_cell_height)) {
            bits = solid;
        } else if (overlay == VIDEO_CURSOR_BAR) {
            bits |= solid & ~(solid >> bar);
        }
        uint32_t *pixels = (uint32_t *)(base + ((size_t)line * gpu_stride));
        for (unsigned x = 0; x < gpu_cell_width; ++x) {
            pixels[x] = ((bits & (1U << ((gpu_cell_width - 1U) - x))) != 0U) ? foreground : background;
        }
    }
}

/// @brief Whether the cursor position refers to the visible screen.
static inline bool_t __gpu_cursor_on_screen(void)
{
    return (cursor_column < gpu_columns) && (cursor_row < gpu_rows);
}

/// @brief Whether the overlay should be on the display.
///
/// The blink phase is not consulted: this backend has no blink, because making
/// one visible needs device round trips the timer interrupt may not make.
static inline bool_t __gpu_cursor_should_show(void) { return cursor_style.shape != VIDEO_CURSOR_HIDDEN; }

/// @brief Takes the overlay off the display, restoring the cell underneath.
static void __gpu_cursor_hide(void)
{
    if (!cursor_drawn) {
        return;
    }
    cursor_drawn = false;
    if (active && __gpu_cursor_on_screen()) {
        __gpu_draw_cell(cursor_column, cursor_row, cursor_cell.character, cursor_cell.attribute,
                        VIDEO_CURSOR_HIDDEN);
        __gpu_damage(cursor_row, cursor_row);
    }
}

/// @brief Puts the overlay on the display, if it belongs there.
static void __gpu_cursor_show(void)
{
    if (cursor_drawn || !active) {
        return;
    }
    if (!__gpu_cursor_should_show() || !__gpu_cursor_on_screen()) {
        return;
    }

    // As the text-mode hardware cursor does, the overlay takes the cell's own
    // foreground colour -- falling back to the default when the cell was erased
    // to attribute 0, which would otherwise be black on black.
    uint8_t attribute = cursor_cell.attribute;
    if ((attribute & 0x0FU) == ((attribute >> 4U) & 0x0FU)) {
        attribute = (uint8_t)((attribute & 0xF0U) | 0x07U);
    }

    __gpu_draw_cell(cursor_column, cursor_row, cursor_cell.character, attribute, cursor_style.shape);
    __gpu_damage(cursor_row, cursor_row);
    cursor_drawn = true;
}

/// @brief Builds the attribute-to-pixel table.
///
/// The components are quantised to 6 bits before being widened back, which
/// reproduces exactly what the VGA DAC does to the same palette in the other two
/// backends. That is deliberate: `docs/maintainer/video-backends.md` makes "all
/// backends produce the same set of RGB values for the same console" a
/// verification gate, and a 32 bpp backend writing the unquantised values would
/// fail it while looking correct. Dropping the quantisation is a one-line change
/// if true colour is ever wanted more than cross-backend equivalence.
static void __gpu_build_palette(void)
{
    for (unsigned index = 0; index < count_of(video_palette_16); ++index) {
        uint32_t red   = (uint32_t)(video_palette_16[index].red >> 2U) << 2U;
        uint32_t green = (uint32_t)(video_palette_16[index].green >> 2U) << 2U;
        uint32_t blue  = (uint32_t)(video_palette_16[index].blue >> 2U) << 2U;
        palette[index] = (red << 16U) | (green << 8U) | blue;
    }
}

/// @brief Largest allocation order whose block is no bigger than `bytes`.
static uint32_t __gpu_order_floor(uint32_t bytes)
{
    uint32_t order = 0;
    while ((order < GPU_FB_MAX_ORDER) && ((PAGE_SIZE << (order + 1U)) <= bytes)) {
        ++order;
    }
    return order;
}

/// @brief Reserves the whole framebuffer window's page tables.
/// @return 0 on success, -1 on failure.
///
/// **This has to happen before any process exists**, and getting it wrong is a
/// page fault that looks like a mapping bug when it is not one.
///
/// `mm_create_blank()` builds a process page directory with
/// `memcpy(pdir_cpy, main_pgd, sizeof(page_directory_t))` -- a snapshot. Page
/// tables that exist at that moment are *shared*, because the copied directory
/// entries point at the same frames. But a directory entry added to the main
/// page directory **afterwards** is invisible to every process already created,
/// and a syscall-serviced resize runs in exactly such a process.
///
/// So the whole window's directory entries are created here, at bring-up, while
/// the main page directory is still the only one. The pages are mapped
/// not-present -- there is nothing behind most of the window and never will be --
/// which is enough to get the page tables allocated and the directory entries
/// populated. Every later remap then only rewrites entries *inside* those shared
/// tables, which every process sees.
///
/// Costs four page tables, 16 KiB, for a 16 MiB window.
static int __gpu_reserve_window(void)
{
    page_directory_t *pgd = paging_get_main_pgd();
    if (pgd == NULL) {
        pr_err("No main page directory to reserve the framebuffer window in.\n");
        return -1;
    }
    // Deliberately without MM_PRESENT and without MM_UPDADDR: the point is the
    // page tables, not the mappings. The entries stay absent until a real block
    // is mapped over them.
    if (mem_upd_vm_area(pgd, GPU_FB_VIRT_BASE, 0, GPU_FB_VIRT_SIZE, MM_RW | MM_GLOBAL) < 0) {
        pr_err("Failed to reserve the framebuffer window at 0x%08x.\n", (unsigned)GPU_FB_VIRT_BASE);
        return -1;
    }
    pr_notice("Reserved %u KiB of address space at 0x%08x for the framebuffer.\n",
              (unsigned)(GPU_FB_VIRT_SIZE / 1024U), (unsigned)GPU_FB_VIRT_BASE);
    return 0;
}

/// @brief Maps the current blocks consecutively at the framebuffer window.
/// @return 0 on success, -1 on failure.
///
/// Separate from allocation because a failed resize has to put the previous
/// mapping back: the old blocks are still allocated and still displayed, and the
/// drawing code has to be able to reach them again.
static int __gpu_remap_blocks(void)
{
    page_directory_t *pgd = paging_get_main_pgd();
    if (pgd == NULL) {
        pr_err("No main page directory to map the framebuffer into.\n");
        return -1;
    }
    uint32_t virtual = GPU_FB_VIRT_BASE;
    for (unsigned index = 0; index < block_count; ++index) {
        uint32_t span = (blocks[index].bytes + (PAGE_SIZE - 1U)) & ~(PAGE_SIZE - 1U);
        if (mem_upd_vm_area(pgd, virtual, blocks[index].physical, span,
                            MM_RW | MM_PRESENT | MM_GLOBAL | MM_UPDADDR) < 0) {
            pr_err("Failed to map framebuffer block %u at 0x%08x.\n", index, virtual);
            return -1;
        }
        virtual += span;
    }
    framebuffer = (uint8_t *)GPU_FB_VIRT_BASE;
    return 0;
}

/// @brief Releases the framebuffer's blocks.
static void __gpu_free_framebuffer(void)
{
    for (unsigned index = 0; index < block_count; ++index) {
        if (blocks[index].pages != NULL) {
            free_pages(blocks[index].pages);
        }
    }
    memset(blocks, 0, sizeof(blocks));
    block_count = 0;
    framebuffer = NULL;
}

/// @brief Allocates the framebuffer and maps it linearly.
/// @return 0 on success, -1 on failure.
///
/// Largest-block-first, stepping down an order at a time when an allocation
/// fails, so a fragmented system still gets its framebuffer -- just in more
/// pieces. Each block becomes one backing entry for the device, and all of them
/// are mapped consecutively so the CPU sees one flat array.
static int __gpu_alloc_framebuffer(void)
{
    memset(blocks, 0, sizeof(blocks));
    block_count = 0;

    uint32_t remaining = gpu_fb_bytes;
    while ((remaining > 0U) && (block_count < GPU_FB_MAX_BLOCKS)) {
        uint32_t order = __gpu_order_floor(remaining);
        page_t *pages  = NULL;
        for (;;) {
            pages = alloc_pages(GFP_KERNEL, order);
            if (pages != NULL) {
                break;
            }
            if (order == 0U) {
                pr_err("Failed to allocate the last %u bytes of the framebuffer.\n", remaining);
                __gpu_free_framebuffer();
                return -1;
            }
            --order;
        }

        uint32_t bytes = (uint32_t)(PAGE_SIZE << order);
        if (bytes > remaining) {
            bytes = remaining;
        }
        blocks[block_count].pages    = pages;
        blocks[block_count].physical = get_physical_address_from_page(pages);
        blocks[block_count].bytes    = bytes;
        remaining -= bytes;
        ++block_count;
    }

    if (remaining > 0U) {
        pr_err("The framebuffer needs more than %u blocks.\n", (unsigned)GPU_FB_MAX_BLOCKS);
        __gpu_free_framebuffer();
        return -1;
    }

    // Map the blocks consecutively, so the drawing code sees one flat array
    // while the device is given the list.
    if (__gpu_remap_blocks() < 0) {
        __gpu_free_framebuffer();
        return -1;
    }
    memset(framebuffer, 0, gpu_fb_bytes);
    pr_notice("Framebuffer: %u KiB in %u block(s), mapped at 0x%08x.\n", (unsigned)(gpu_fb_bytes / 1024U), block_count,
              (unsigned)GPU_FB_VIRT_BASE);
    return 0;
}

/// @brief Creates a resource at the current geometry and attaches the blocks.
/// @param id The resource id to create.
/// @return 0 on success, -1 on failure.
static int __gpu_create_resource(uint32_t id)
{
    gpu_create_2d_t *create = (gpu_create_2d_t *)command_virtual;
    __gpu_header(VIRTIO_GPU_CMD_RESOURCE_CREATE_2D);
    create->resource_id = id;
    create->format      = VIRTIO_GPU_FORMAT_B8G8R8X8_UNORM;
    create->width       = gpu_width;
    create->height      = gpu_height;
    if (__gpu_command(sizeof(*create), VIRTIO_GPU_RESP_OK_NODATA) < 0) {
        return -1;
    }

    gpu_attach_backing_t *attach = (gpu_attach_backing_t *)command_virtual;
    __gpu_header(VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING);
    attach->resource_id = id;
    attach->nr_entries  = block_count;
    for (unsigned index = 0; index < block_count; ++index) {
        attach->entries[index].addr_lo = blocks[index].physical;
        attach->entries[index].addr_hi = 0;
        attach->entries[index].length  = blocks[index].bytes;
        attach->entries[index].padding = 0;
    }
    // Only the entries actually used are sent.
    uint32_t length = sizeof(gpu_ctrl_hdr_t) + (2U * sizeof(uint32_t)) + (block_count * sizeof(gpu_mem_entry_t));
    return __gpu_command(length, VIRTIO_GPU_RESP_OK_NODATA);
}

/// @brief Releases the resource a previous resize left behind.
///
/// Deliberately here rather than at the moment of the switch; see the comment on
/// `retired_resource`.
static void __gpu_release_retired(void)
{
    if (retired_resource != 0U) {
        gpu_ctrl_hdr_t *unref = (gpu_ctrl_hdr_t *)command_virtual;
        __gpu_header(VIRTIO_GPU_CMD_RESOURCE_UNREF);
        // The unref payload is the header followed by the id and padding.
        *(uint32_t *)(command_virtual + sizeof(gpu_ctrl_hdr_t))      = retired_resource;
        *(uint32_t *)(command_virtual + sizeof(gpu_ctrl_hdr_t) + 4U) = 0;
        (void)unref;
        (void)__gpu_command(sizeof(gpu_ctrl_hdr_t) + 8U, VIRTIO_GPU_RESP_OK_NODATA);
        retired_resource = 0;
    }
    for (unsigned index = 0; index < retired_count; ++index) {
        if (retired_blocks[index].pages != NULL) {
            free_pages(retired_blocks[index].pages);
        }
    }
    memset(retired_blocks, 0, sizeof(retired_blocks));
    retired_count = 0;
}

/// @brief Prepares to materialize a different cell geometry.
/// @param columns The new console width in cells.
/// @param rows The new console height in cells.
/// @return 0 when ready, -1 on failure.
///
/// Builds a whole second framebuffer and a second resource while the first is
/// still scanning out, and does **not** switch: `scanout_set` is cleared so the
/// switch happens on the next publish, which the generic layer only reaches
/// after it has repainted the console at the new geometry. So the display goes
/// straight from a correct old screen to a correct new one.
///
/// Every failure path leaves the current resource, framebuffer and scanout
/// exactly as they were, so the console keeps working at its old size.
///
/// One deliberate imprecision: this adopts the new pixel geometry before the
/// generic layer publishes the new cell geometry, so for the handful of
/// instructions in between an interrupt-context `printf` would draw at old
/// coordinates into the new framebuffer. That is bounded rather than dangerous --
/// put_cells() clamps to the current geometry, so nothing can be written outside
/// the framebuffer -- and the repaint that follows immediately overwrites it.
static int virtio_gpu_set_geometry(unsigned columns, unsigned rows)
{
    if (!active) {
        return -1;
    }

    // Release whatever the previous resize left behind, before allocating more.
    __gpu_release_retired();

    // Remember everything needed to put things back.
    const video_font_t *old_font = gpu_font;
    unsigned old_cell_width      = gpu_cell_width;
    unsigned old_cell_height     = gpu_cell_height;
    unsigned old_columns = gpu_columns;
    unsigned old_rows    = gpu_rows;
    unsigned old_width   = gpu_width;
    unsigned old_height  = gpu_height;
    unsigned old_stride  = gpu_stride;
    unsigned old_rowb    = gpu_row_bytes;
    unsigned old_bytes   = gpu_fb_bytes;
    gpu_block_t old_blocks[GPU_FB_MAX_BLOCKS];
    unsigned old_count = block_count;
    memcpy(old_blocks, blocks, sizeof(old_blocks));

    // A font change arrives here as a staged font plus the cell counts it
    // produced; a display change leaves the font alone. Either way what follows
    // builds a framebuffer for whatever is now current.
    if (gpu_pending_font != NULL) {
        __gpu_set_font(gpu_pending_font);
    }

    if (__gpu_set_dimensions(columns, rows) < 0) {
        gpu_columns = old_columns; gpu_rows = old_rows; gpu_width = old_width;
        gpu_height = old_height;   gpu_stride = old_stride; gpu_row_bytes = old_rowb;
        gpu_fb_bytes = old_bytes;
        gpu_font = old_font; gpu_cell_width = old_cell_width; gpu_cell_height = old_cell_height;
        return -1;
    }

    // A fresh framebuffer, mapped over the same virtual window. The old one is
    // only read by the device from now on, and the device uses physical
    // addresses, so replacing the mapping does not disturb what is displayed.
    block_count = 0;
    if (__gpu_alloc_framebuffer() < 0) {
        gpu_columns = old_columns; gpu_rows = old_rows; gpu_width = old_width;
        gpu_height = old_height;   gpu_stride = old_stride; gpu_row_bytes = old_rowb;
        gpu_fb_bytes = old_bytes;
        gpu_font = old_font; gpu_cell_width = old_cell_width; gpu_cell_height = old_cell_height;
        memcpy(blocks, old_blocks, sizeof(blocks));
        block_count = old_count;
        // Put the old mapping back, so drawing keeps reaching the live pixels.
        (void)__gpu_remap_blocks();
        return -1;
    }

    uint32_t next_resource = (resource_id == 1U) ? 2U : 1U;
    if (__gpu_create_resource(next_resource) < 0) {
        __gpu_free_framebuffer();
        gpu_columns = old_columns; gpu_rows = old_rows; gpu_width = old_width;
        gpu_height = old_height;   gpu_stride = old_stride; gpu_row_bytes = old_rowb;
        gpu_fb_bytes = old_bytes;
        gpu_font = old_font; gpu_cell_width = old_cell_width; gpu_cell_height = old_cell_height;
        memcpy(blocks, old_blocks, sizeof(blocks));
        block_count = old_count;
        (void)__gpu_remap_blocks();
        return -1;
    }

    // Committed. The old resource stays on the scanout until the next publish.
    retired_resource = resource_id;
    memcpy(retired_blocks, old_blocks, sizeof(retired_blocks));
    retired_count = old_count;

    resource_id  = next_resource;
    scanout_set  = false;
    damage_valid = false;
    cursor_drawn = false;
    transfer_all = true;

    pr_notice("Prepared %ux%u cells of %s (%ux%u pixels) as resource %u.\n", columns, rows, gpu_font->name, gpu_width,
              gpu_height, resource_id);
    return 0;
}

/// @brief Changes font size, and reshapes the console to what still fits.
/// @param reset Start from the default size rather than the current one.
/// @param steps Signed number of size steps; positive is larger.
/// @return 0 when the font is now the wanted one, -1 when it could not change.
///
/// The scanout is not touched. Its size is what the host asked for and has
/// nothing to do with how big the glyphs are, so the cell counts are recomputed
/// by dividing it by the new cell size and the display stays exactly as large as
/// it was. That is also why a resize arriving afterwards keeps this font: the
/// division is done with whatever font is current, in both directions.
///
/// Transactional by construction, because it is the console's own resize doing
/// the work: the font is staged, the resize either completes or leaves
/// everything alone, and the staging is dropped either way. A failure here leaves
/// the old font drawing the old geometry on the same scanout.
static int virtio_gpu_request_font(bool_t reset, int steps)
{
    if (!active) {
        return -1;
    }

    const video_font_t *next = video_font_step(gpu_font, reset, steps);
    unsigned cell_width      = video_font_width(next);
    unsigned cell_height     = video_font_height(next);
    unsigned columns         = gpu_target_width / cell_width;
    unsigned rows            = gpu_target_height / cell_height;

    if ((columns == 0U) || (rows == 0U)) {
        pr_err("%s does not fit a %ux%u display at all.\n", next->name, gpu_target_width, gpu_target_height);
        return -1;
    }

    // Already there with nothing to redraw. Reported as success: the console is
    // in the state that was asked for, and a resize waiting on this same scanout
    // would divide out to these same counts and change nothing either.
    if ((next == gpu_font) && (columns == gpu_columns) && (rows == gpu_rows)) {
        return 0;
    }

    gpu_pending_font = next;
    int result       = video_change_geometry(columns, rows);
    gpu_pending_font = NULL;

    if (result < 0) {
        pr_err("%s would need %ux%u cells, which was refused; keeping %s.\n", next->name, columns, rows,
               gpu_font->name);
        return -1;
    }

    pr_notice("Font is now %s: %ux%u cells on a %ux%u display.\n", gpu_font->name, gpu_columns, gpu_rows, gpu_width,
              gpu_height);
    return 0;
}

/// @brief Tears down everything late_init() built.
static void __gpu_teardown(void)
{
    active      = false;
    scanout_set = false;
    if (gpu_irq_line != 0U) {
        irq_uninstall_handler(gpu_irq_line, virtio_gpu_isr);
        gpu_irq_line = 0;
    }
    virtq_free(&gpu, &controlq);
    virtio_reset(&gpu);
    virtio_pci_release(&gpu);
    if (command_pages != NULL) {
        free_pages(command_pages);
        command_pages    = NULL;
        command_physical = 0;
        command_virtual  = 0;
    }
    __gpu_free_framebuffer();
}

/// @brief Brings up the device, the queue and the resource.
/// @return 0 on success, a negative value on failure.
///
/// Nothing here touches the display: the scanout is switched on the first flush,
/// once the generic layer has repainted into the resource. So every failure path
/// leaves whichever backend was displaying still displaying.
static int virtio_gpu_late_init(void)
{
    // The console is whatever shape it was compiled as when this first runs.
    // The compiled geometry, at the compiled font: that product is the scanout
    // this backend starts with, and the target it fills until the host says
    // otherwise. Nothing here consults the host yet -- promotion must not change
    // what the user is already looking at.
    __gpu_set_font(video_font_default());
    gpu_target_width  = VIDEO_COLUMNS * gpu_cell_width;
    gpu_target_height = VIDEO_ROWS * gpu_cell_height;
    if (__gpu_set_dimensions(VIDEO_COLUMNS, VIDEO_ROWS) < 0) {
        return -1;
    }

    uint32_t pci_device = 0;
    int found           = virtio_pci_find(VIRTIO_ID_GPU, &pci_device);
    if (found != 0) {
        pr_notice("No virtio-gpu device present.\n");
        return -1;
    }
    if (virtio_pci_setup(pci_device, &gpu) < 0) {
        return -1;
    }

    virtio_features_t wanted   = {0, 0};
    virtio_features_t required = {VIRTIO_FEATURE_LOW(VIRTIO_F_VERSION_1), VIRTIO_FEATURE_HIGH(VIRTIO_F_VERSION_1)};
    if (virtio_negotiate(&gpu, wanted, required) < 0) {
        virtio_pci_release(&gpu);
        return -1;
    }

    if (virtq_setup(&gpu, 0, 64, &controlq) < 0) {
        virtio_reset(&gpu);
        virtio_pci_release(&gpu);
        return -1;
    }
    if (virtio_driver_ok(&gpu) < 0) {
        __gpu_teardown();
        return -1;
    }

    uint32_t scanouts = *(volatile uint32_t *)(gpu.device_config + VIRTIO_GPU_CFG_NUM_SCANOUTS);
    if (scanouts == 0U) {
        pr_err("The device reports no scanouts.\n");
        __gpu_teardown();
        return -1;
    }

    command_pages = alloc_pages(GFP_KERNEL, 0);
    if (command_pages == NULL) {
        pr_err("Failed to allocate the command page.\n");
        __gpu_teardown();
        return -1;
    }
    command_physical = get_physical_address_from_page(command_pages);
    command_virtual  = get_virtual_address_from_page(command_pages);
    if (command_virtual == 0U) {
        pr_err("No low-memory address for the command page.\n");
        __gpu_teardown();
        return -1;
    }
    memset((void *)command_virtual, 0, PAGE_SIZE);

    // Ask what the host would like. Not acted on yet -- the console's geometry
    // is fixed at compile time -- but worth recording, because it is the value a
    // runtime-sized console will follow.
    __gpu_header(VIRTIO_GPU_CMD_GET_DISPLAY_INFO);
    if (__gpu_command(sizeof(gpu_ctrl_hdr_t), VIRTIO_GPU_RESP_OK_DISPLAY_INFO) == 0) {
        const gpu_display_info_t *info = (const gpu_display_info_t *)(command_virtual + GPU_RESPONSE_OFFSET);
        pr_notice("Host would like %ux%u (scanout %u, enabled %u); using the compiled %ux%u.\n",
                  info->pmodes[VIRTIO_GPU_SCANOUT].rect.width, info->pmodes[VIRTIO_GPU_SCANOUT].rect.height,
                  (unsigned)VIRTIO_GPU_SCANOUT, info->pmodes[VIRTIO_GPU_SCANOUT].enabled, (unsigned)gpu_width,
                  (unsigned)gpu_height);
    }

    // Before the first framebuffer, and before any process can exist to snapshot
    // an incomplete page directory. See __gpu_reserve_window().
    if (__gpu_reserve_window() < 0) {
        __gpu_teardown();
        return -1;
    }

    if (__gpu_alloc_framebuffer() < 0) {
        __gpu_teardown();
        return -1;
    }

    if (__gpu_create_resource(resource_id) < 0) {
        __gpu_teardown();
        return -1;
    }

    __gpu_build_palette();

    // The interrupt line is read straight from configuration space rather than
    // through pci_get_interrupt(), which resolves it via the PIRQ routing table
    // that pci_remap() fills in -- and nothing calls pci_remap(), so that table
    // is all zeroes. What the firmware programmed is what the PIC will deliver.
    uint8_t line = 0;
    if (pci_read_8(pci_device, PCI_INTERRUPT_LINE, &line) || (line == 0U) || (line >= 16U)) {
        pr_warning("No usable interrupt line (read %u); display changes will not be noticed.\n", line);
    } else if (irq_install_handler(line, virtio_gpu_isr, "virtio-gpu") < 0) {
        pr_warning("Could not install a handler on IRQ %u; display changes will not be noticed.\n", line);
    } else {
        // Every line starts masked, so it has to be let through explicitly.
        pic8259_irq_enable(line);
        gpu_irq_line = line;
        pr_notice("Listening for display changes on IRQ %u.\n", line);
    }

    active       = true;
    transfer_all = true;
    pr_notice("Ready: %ux%u at 32 bpp, %ux%u cells of %s, resource %u backed by %u block(s).\n", (unsigned)gpu_width,
              (unsigned)gpu_height, (unsigned)gpu_columns, (unsigned)gpu_rows, gpu_font->name, resource_id,
              block_count);
    return 0;
}

/// @brief Draws cells to the framebuffer and sends the rows that changed.
static void virtio_gpu_put_cells(unsigned column, unsigned row, const video_cell_t *cells, unsigned count)
{
    if (!active || (cells == NULL) || (column >= gpu_columns) || (row >= gpu_rows)) {
        return;
    }

    unsigned next   = 0;
    bool_t touched  = false;
    while ((count > 0) && (row < gpu_rows)) {
        unsigned run = gpu_columns - column;
        if (run > count) {
            run = count;
        }

        // A run covering the cursor's cell replaces what the overlay sits on, so
        // refresh the cache and let the render wipe the overlay; it is put back
        // once the whole range is drawn.
        if ((row == cursor_row) && (cursor_column >= column) && (cursor_column < (column + run))) {
            cursor_cell  = cells[next + (cursor_column - column)];
            cursor_drawn = false;
            touched      = true;
        }

        for (unsigned index = 0; index < run; ++index) {
            const video_cell_t cell = cells[next + index];
            __gpu_draw_cell(column + index, row, cell.character, cell.attribute, VIDEO_CURSOR_HIDDEN);
        }
        __gpu_damage(row, row);

        next += run;
        count -= run;
        column = 0;
        ++row;
    }

    if (touched) {
        __gpu_cursor_show();
    }

    // One transfer and one flush for everything this call touched, rather than
    // a device round trip per row.
    __gpu_publish();
}

/// @brief Moves the displayed content vertically.
///
/// There is no panning here: a 2D resource has no scanout offset this backend
/// can move cheaply, so the pixels are moved and the whole screen resent. That
/// makes a scroll the expensive operation, in exchange for a keystroke costing
/// one row.
static void virtio_gpu_scroll(int rows)
{
    if (!active || (rows == 0)) {
        return;
    }

    // The overlay is pixels like everything else and the move carries it along.
    if (cursor_drawn) {
        int moved = (int)cursor_row - rows;
        if ((moved < 0) || (moved >= (int)gpu_rows)) {
            cursor_drawn = false;
        }
        cursor_row = (unsigned)moved;
    }

    unsigned distance = (rows > 0) ? (unsigned)rows : (unsigned)(-rows);
    if (distance < gpu_rows) {
        unsigned keep = gpu_rows - distance;
        uint8_t *from = framebuffer + ((rows > 0) ? ((size_t)distance * gpu_row_bytes) : 0U);
        uint8_t *to   = framebuffer + ((rows > 0) ? 0U : ((size_t)distance * gpu_row_bytes));
        memmove(to, from, (size_t)keep * gpu_row_bytes);
    }

    // Everything moved, so everything has to be resent. The caller repaints the
    // rows the move uncovered, which will land in the same damage range.
    __gpu_damage(0, gpu_rows - 1U);
    __gpu_publish();
}

/// @brief Places the cursor.
static void virtio_gpu_set_cursor_position(unsigned column, unsigned row, video_cell_t cell)
{
    if (column >= gpu_columns) {
        column = gpu_columns - 1;
    }
    if (row >= gpu_rows) {
        row = gpu_rows - 1;
    }

    __gpu_cursor_hide();
    cursor_column = column;
    cursor_row    = row;
    cursor_cell   = cell;
    __gpu_cursor_show();
    __gpu_publish();
}

/// @brief Selects the cursor appearance.
///
/// The blink flag is recorded but has no effect; see the note at the top.
static void virtio_gpu_set_cursor_style(video_cursor_style_t style)
{
    if ((cursor_style.shape == style.shape) && (cursor_style.blinking == style.blinking)) {
        return;
    }
    __gpu_cursor_hide();
    cursor_style = style;
    __gpu_cursor_show();
    __gpu_publish();
}

/// @brief Interrupt handler for the device.
///
/// Deliberately trivial, and it has to be: this may not allocate, may not wait
/// for a virtqueue completion, and may not migrate the console. So it reads the
/// interrupt status -- which is what acknowledges it -- notes that the geometry
/// needs re-reading, acknowledges the event bits, and wakes whatever is blocked
/// in the console's read path so that a process context comes along promptly.
///
/// Everything else happens in virtio_gpu_service().
static void virtio_gpu_isr(pt_regs_t *registers)
{
    (void)registers;

    // Reading clears the register, so the value has to be examined rather than
    // tested twice.
    uint8_t status = virtio_read_isr(&gpu);
    if ((status & VIRTIO_ISR_CONFIG) == 0) {
        return;
    }

    // Acknowledge whatever the device is reporting. Writing the bits back is the
    // protocol's acknowledgement; leaving them set would keep the interrupt
    // asserted.
    volatile uint32_t *events_read  = (volatile uint32_t *)(gpu.device_config + VIRTIO_GPU_CFG_EVENTS_READ);
    volatile uint32_t *events_clear = (volatile uint32_t *)(gpu.device_config + VIRTIO_GPU_CFG_EVENTS_CLEAR);
    uint32_t events                 = *events_read;
    if (events != 0U) {
        *events_clear = events;
    }

    geometry_stale = true;
    keyboard_wake_readers();
}

/// @brief Turns a noticed display change into a resize request.
///
/// Runs in process context, from video_service_pending(). Asking the device for
/// its display info needs a virtqueue round trip, which is exactly what the
/// interrupt handler could not do.
///
/// The scanout geometry is in pixels; the console's is in cells. This is the one
/// place the two meet, and it is the backend's business precisely because the
/// backend owns the font.
static void virtio_gpu_service(void)
{
    if (!active) {
        return;
    }
    if (!geometry_stale) {
        // Nothing pending, so the hand-over's echo never materialised -- this
        // host does not send one. Disarm, or the next genuine resize would be
        // mistaken for it and swallowed. Under VNC, where the only events are
        // explicit client requests, this is the path that always runs first.
        promotion_echo_pending = false;
        return;
    }
    geometry_stale = false;

    // The hand-over's own echo. Dropped without even reading the size out of it,
    // because none of it is anything the user asked for.
    if (promotion_echo_pending) {
        promotion_echo_pending = false;
        pr_notice("Ignoring the display change the hand-over caused; keeping %ux%u.\n", gpu_width, gpu_height);
        return;
    }

    __gpu_header(VIRTIO_GPU_CMD_GET_DISPLAY_INFO);
    if (__gpu_command(sizeof(gpu_ctrl_hdr_t), VIRTIO_GPU_RESP_OK_DISPLAY_INFO) < 0) {
        pr_err("Could not read the display info after a display-change event.\n");
        return;
    }

    const gpu_display_info_t *info = (const gpu_display_info_t *)(command_virtual + GPU_RESPONSE_OFFSET);
    uint32_t width                 = info->pmodes[VIRTIO_GPU_SCANOUT].rect.width;
    uint32_t height                = info->pmodes[VIRTIO_GPU_SCANOUT].rect.height;
    uint32_t enabled               = info->pmodes[VIRTIO_GPU_SCANOUT].enabled;

    // A disabled or zero-sized scanout means "nothing is being displayed", not
    // "display nothing": keep showing what we have.
    if ((enabled == 0U) || (width == 0U) || (height == 0U)) {
        pr_notice("Scanout %u reports %ux%u enabled=%u; keeping %ux%u.\n", (unsigned)VIRTIO_GPU_SCANOUT, width, height,
                  enabled, gpu_width, gpu_height);
        return;
    }

    // Record what the host wants even when it makes no difference to the cell
    // counts, so that whatever acts next -- a later resize, or a font change --
    // works from the current window rather than from the last one that happened
    // to cross a cell boundary. The cost is that the scanout can lag the window
    // by less than one cell until something does act.
    gpu_target_width  = (unsigned)width;
    gpu_target_height = (unsigned)height;

    // Whole cells only; a few leftover pixels at the right or bottom edge are
    // margin. No assumption is made about which dimension is larger.
    unsigned columns = (unsigned)(width / gpu_cell_width);
    unsigned rows    = (unsigned)(height / gpu_cell_height);
    if ((columns == gpu_columns) && (rows == gpu_rows)) {
        return;
    }

    pr_notice("Display is %ux%u pixels, which is %ux%u cells.\n", width, height, columns, rows);

    // The generic layer decides whether that geometry is acceptable and does the
    // work; refusing it here would duplicate its limits.
    video_request_resize(columns, rows);
}

/// @brief The virtio-gpu backend.
///
/// Not named `video_backend`: this is never the boot backend, and the console
/// reaches it only after video_promote_backend() has accepted it.
static const video_backend_t virtio_gpu_backend = {
    .name                = "virtio-gpu-2d",
    .columns             = VIDEO_COLUMNS,
    .rows                = VIDEO_ROWS,
    .init                = NULL,
    .late_init           = virtio_gpu_late_init,
    .put_cells           = virtio_gpu_put_cells,
    .scroll              = virtio_gpu_scroll,
    .begin_batch         = virtio_gpu_begin_batch,
    .end_batch           = virtio_gpu_end_batch,
    .set_cursor_position = virtio_gpu_set_cursor_position,
    .set_cursor_style    = virtio_gpu_set_cursor_style,
    .set_geometry        = virtio_gpu_set_geometry,
    .request_font        = virtio_gpu_request_font,
    .service             = virtio_gpu_service,
    .cursor_blink        = NULL,
};

int virtio_gpu_promote(void) { return video_promote_backend(&virtio_gpu_backend); }
