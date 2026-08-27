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

#include "devices/virtio.h"
#include "klib/irqflags.h"
#include "mem/alloc/zone_allocator.h"
#include "mem/mm/page.h"
#include "mem/mm/vm_area.h"
#include "mem/paging.h"
#include "io/video_backend.h"
#include "stdbool.h"
#include "stddef.h"
#include "string.h"

#include "video_font_8x16.h"
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

/// @name The font's cell size
/// @brief The only place pixels and cells meet.
///
/// Everything below derives pixels from cells through these. The generic console
/// never sees them: it asks for a number of columns and rows, and this backend
/// works out how many pixels that is. That is also what would make a font change
/// the same operation as a display change -- a different pair of cell counts --
/// rather than a new mechanism.
/// @{
#define GPU_CELL_WIDTH  VIDEO_FONT_WIDTH  ///< Pixels a cell occupies across.
#define GPU_CELL_HEIGHT VIDEO_FONT_HEIGHT ///< Scan lines a cell occupies.
/// @}

/// @name Largest framebuffer this backend will build
/// @brief Bounds the virtual window and the backing-list length.
/// @{
#define GPU_MAX_WIDTH   2048U ///< Widest framebuffer, in pixels.
#define GPU_MAX_HEIGHT  2048U ///< Tallest framebuffer, in scan lines.
/// @}

/// @brief Kernel virtual base the framebuffer blocks are mapped consecutively at.
///
/// A third window in the unused KERNEL_HIGHMEM region; see the map in
/// devices/virtio.h. 16 MiB is room for a 2048x2048x32 console and then some.
#define GPU_FB_VIRT_BASE  0xFB000000U
/// @brief Size of that window.
#define GPU_FB_VIRT_SIZE  0x01000000U

/// @brief Most blocks the framebuffer may be split into.
///
/// Each becomes one memory entry in the backing list. A fresh system needs two
/// or three; measured across repeated resizes, fragmentation pushed it to seven,
/// so sixteen is the headroom that keeps a resize from failing on a machine that
/// has been running a while. Sixteen entries is 288 bytes of command, well inside
/// the command page.
#define GPU_FB_MAX_BLOCKS 16U

/// @brief Largest allocation order used for a framebuffer block.
///
/// The buddy allocator tops out at MAX_BUDDYSYSTEM_GFP_ORDER; staying a little
/// below it avoids leaning on the single largest block the system has.
#define GPU_FB_MAX_ORDER  10U

/// @brief Number of scan lines the underline cursor covers.
#define GPU_CURSOR_UNDERLINE_LINES 3U
/// @brief Pixel mask of the bar cursor: the two leftmost columns of the cell.
#define GPU_CURSOR_BAR_MASK        0xC0U

/// @brief Compile-time check that the largest framebuffer fits its virtual window.
typedef char gpu_window_check[(((GPU_MAX_WIDTH * GPU_MAX_HEIGHT * 4U) <= GPU_FB_VIRT_SIZE) &&
                               (GPU_FB_VIRT_BASE >= 0xF8000000U) &&
                               ((GPU_FB_VIRT_BASE + GPU_FB_VIRT_SIZE) <= 0xFEC00000U))
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

/// @name The geometry currently materialized
/// @brief Derived from the console's cell counts and the font, and nothing else.
/// @{
static unsigned gpu_columns   = 0; ///< Console width in cells.
static unsigned gpu_rows      = 0; ///< Console height in cells.
static unsigned gpu_width     = 0; ///< Framebuffer width in pixels.
static unsigned gpu_height    = 0; ///< Framebuffer height in scan lines.
static unsigned gpu_stride    = 0; ///< Bytes per scan line.
static unsigned gpu_row_bytes = 0; ///< Bytes one text row occupies.
static unsigned gpu_fb_bytes  = 0; ///< Bytes the framebuffer occupies.
/// @}

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

/// @brief Works out the pixel geometry a cell geometry implies.
/// @param columns Width in cells.
/// @param rows Height in cells.
/// @return 0 when it is a geometry this backend can build, -1 otherwise.
///
/// The single place cells become pixels. Bounded by GPU_MAX_*, which is what the
/// virtual window and the backing list were sized for, and checked before the
/// multiplication so the byte count cannot overflow.
static int __gpu_set_dimensions(unsigned columns, unsigned rows)
{
    if ((columns == 0U) || (rows == 0U)) {
        return -1;
    }
    unsigned width  = columns * GPU_CELL_WIDTH;
    unsigned height = rows * GPU_CELL_HEIGHT;
    if ((width > GPU_MAX_WIDTH) || (height > GPU_MAX_HEIGHT)) {
        pr_err("%ux%u cells is %ux%u pixels, past the %ux%u this backend builds.\n", columns, rows, width, height,
               (unsigned)GPU_MAX_WIDTH, (unsigned)GPU_MAX_HEIGHT);
        return -1;
    }

    gpu_columns   = columns;
    gpu_rows      = rows;
    gpu_width     = width;
    gpu_height    = height;
    gpu_stride    = width * 4U;
    gpu_row_bytes = gpu_stride * GPU_CELL_HEIGHT;
    gpu_fb_bytes  = gpu_stride * height;
    return 0;
}

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
static void __gpu_publish(void)
{
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

    uint32_t y      = first * GPU_CELL_HEIGHT;
    uint32_t height = ((last - first) + 1U) * GPU_CELL_HEIGHT;
    uint32_t offset = first * gpu_row_bytes;

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

/// @brief Glyph bitmap of a cell's character.
static inline const uint8_t *__gpu_cell_glyph(video_cell_t cell)
{
    return &video_font_8x16[(unsigned)cell.character * GPU_CELL_HEIGHT];
}

/// @brief Draws one cell from an explicit glyph, into memory only.
/// @param column The column.
/// @param row The row.
/// @param glyph GPU_CELL_HEIGHT bytes of bitmap.
/// @param attribute Foreground index in the low nibble, background in the high.
static void __gpu_draw_cell(unsigned column, unsigned row, const uint8_t *glyph, uint8_t attribute)
{
    uint32_t foreground = palette[attribute & 0x0FU];
    uint32_t background = palette[(attribute >> 4U) & 0x0FU];
    uint8_t *base       = framebuffer + ((size_t)row * gpu_row_bytes) + ((size_t)column * GPU_CELL_WIDTH * 4U);

    for (unsigned line = 0; line < GPU_CELL_HEIGHT; ++line) {
        uint32_t *pixels = (uint32_t *)(base + ((size_t)line * gpu_stride));
        uint8_t bits     = glyph[line];
        for (unsigned x = 0; x < GPU_CELL_WIDTH; ++x) {
            pixels[x] = ((bits & (0x80U >> x)) != 0U) ? foreground : background;
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
        __gpu_draw_cell(cursor_column, cursor_row, __gpu_cell_glyph(cursor_cell), cursor_cell.attribute);
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

    const uint8_t *source = __gpu_cell_glyph(cursor_cell);
    uint8_t glyph[GPU_CELL_HEIGHT];

    for (unsigned line = 0; line < GPU_CELL_HEIGHT; ++line) {
        uint8_t overlay = 0x00U;
        if (cursor_style.shape == VIDEO_CURSOR_BLOCK) {
            overlay = 0xFFU;
        } else if (cursor_style.shape == VIDEO_CURSOR_UNDERLINE) {
            overlay = (line >= (GPU_CELL_HEIGHT - GPU_CURSOR_UNDERLINE_LINES)) ? 0xFFU : 0x00U;
        } else if (cursor_style.shape == VIDEO_CURSOR_BAR) {
            overlay = GPU_CURSOR_BAR_MASK;
        }
        glyph[line] = (uint8_t)(source[line] | overlay);
    }

    // As the text-mode hardware cursor does, the overlay takes the cell's own
    // foreground colour -- falling back to the default when the cell was erased
    // to attribute 0, which would otherwise be black on black.
    uint8_t attribute = cursor_cell.attribute;
    if ((attribute & 0x0FU) == ((attribute >> 4U) & 0x0FU)) {
        attribute = (uint8_t)((attribute & 0xF0U) | 0x07U);
    }

    __gpu_draw_cell(cursor_column, cursor_row, glyph, attribute);
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

    if (__gpu_set_dimensions(columns, rows) < 0) {
        gpu_columns = old_columns; gpu_rows = old_rows; gpu_width = old_width;
        gpu_height = old_height;   gpu_stride = old_stride; gpu_row_bytes = old_rowb;
        gpu_fb_bytes = old_bytes;
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

    pr_notice("Prepared %ux%u cells (%ux%u pixels) as resource %u.\n", columns, rows, gpu_width, gpu_height,
              resource_id);
    return 0;
}

/// @brief Tears down everything late_init() built.
static void __gpu_teardown(void)
{
    active      = false;
    scanout_set = false;
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

    if (__gpu_alloc_framebuffer() < 0) {
        __gpu_teardown();
        return -1;
    }

    if (__gpu_create_resource(resource_id) < 0) {
        __gpu_teardown();
        return -1;
    }

    __gpu_build_palette();

    active = true;
    pr_notice("Ready: %ux%u at 32 bpp, %ux%u cells, resource %u backed by %u block(s).\n", (unsigned)gpu_width,
              (unsigned)gpu_height, (unsigned)gpu_columns, (unsigned)gpu_rows, resource_id, block_count);
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
            __gpu_draw_cell(column + index, row, __gpu_cell_glyph(cell), cell.attribute);
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
    .set_cursor_position = virtio_gpu_set_cursor_position,
    .set_cursor_style    = virtio_gpu_set_cursor_style,
    .set_geometry        = virtio_gpu_set_geometry,
    .cursor_blink        = NULL,
};

int virtio_gpu_promote(void) { return video_promote_backend(&virtio_gpu_backend); }
