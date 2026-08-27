/// @file vbe_lfb.c
/// @brief VBE linear-framebuffer backend: 1024x768, 256-entry palette, 8 bpp.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.
///
/// Renders the console as text drawn with the shared 8x16 font into a linear
/// framebuffer, giving exactly 128x48 cells. That is the reason this backend
/// exists: the same font on a larger mode shows more of the terminal, instead of
/// showing the same 80x30 terminal scaled up.
///
/// The mode is set through the Bochs VBE (DISPI) extension, which QEMU's
/// `-vga std`, `-vga virtio` and `bochs-display` all implement. One byte per
/// pixel indexes the DAC, and the console's attribute nibbles are already
/// palette indices, so -- exactly as in the planar backend -- no colour
/// translation is needed and both backends load the same 16 DAC entries and so
/// render the same console in the same colours.
///
/// One byte per pixel is also what makes drawing cheap. A glyph scan line is
/// eight consecutive bytes at an eight-byte-aligned address, so a cell's scan
/// line is two aligned 32-bit stores with no read-modify-write and no per-pixel
/// loop.
///
/// Unlike the planar backend, the framebuffer is ordinary write-back memory
/// rather than a trapped aperture: QEMU exposes it as a plain RAM region behind
/// a PCI BAR. Measured on the target, one cell costs 310 cycles here against
/// roughly 13700 in the planar mode, and the 45730 cycles that the same stores
/// take through the 0xA0000 window say why. So this backend is both bigger and
/// far faster, and it needs none of the trap-avoidance shape the planar one has.
///
/// ## Two-stage initialization
///
/// The framebuffer lives in a PCI BAR high above RAM -- 0xFD000000 on QEMU's
/// stdvga -- and the bootloader's page directory identity-maps only the first
/// 896 MB. video_init() therefore cannot reach it, and cannot map it either: it
/// runs before pmmngr_init(), kmem_cache_init() and paging_init(), so there is
/// no allocator to build page tables with.
///
/// So init() only *discovers*, using port I/O and PCI configuration cycles,
/// neither of which needs a mapping, and late_init() does everything that
/// touches memory once paging_init() has run. Until late_init() has returned 0
/// this backend is completely inert: `mapped` gates every entry point, and no
/// framebuffer byte and no DISPI register is touched before it is set. The
/// generic console records into its cell buffer throughout and repaints all of
/// it when late_init() succeeds, so nothing printed in between is lost.
///
/// **Documented limitation:** a panic between video_init() and
/// video_late_init() leaves nothing on screen in this build. The serial log
/// still has it. This is the same trade the planar backend already makes for a
/// panic before video_init(), extended by the handful of boot steps between the
/// two calls.
///
/// **Documented limitation:** the mapping is write-back cacheable. There is no
/// cache-attribute support in mem_upd_vm_area() to ask for anything else --
/// MM_CACHE_DISABLE exists in the flag enum but __set_pg_table_flags() does not
/// implement it -- and under QEMU the region is coherent with the guest's view
/// regardless. Real hardware would want write-combining here.

// Setup the logging for this file (do this before any other include).
#include "sys/kernel_levels.h"           // Include kernel log levels.
#define __DEBUG_HEADER__ "[VBELFB]"      ///< Change header.
#define __DEBUG_LEVEL__  LOGLEVEL_NOTICE ///< Set log level.
#include "io/debug.h"                    // Include debugging functions.

#include "devices/pci.h"
#include "hardware/timer.h"
#include "io/port_io.h"
#include "io/video_backend.h"
#include "mem/paging.h"
#include "stdbool.h"
#include "stddef.h"
#include "string.h"

#include "video_font_8x16.h"
#include "video_palette_16.h"

/// @name Bochs VBE (DISPI) interface
/// @brief Index/data port pair, and the register indices behind it.
///
/// These are plain I/O ports, which is what lets the mode be programmed without
/// any memory mapping. The same registers are also reachable through an MMIO
/// window in BAR2, which would need one, so the ports are used deliberately.
/// @{
#define VBE_DISPI_IOPORT_INDEX  0x01CE ///< Register index port.
#define VBE_DISPI_IOPORT_DATA   0x01CF ///< Register data port.

#define VBE_DISPI_INDEX_ID      0x0 ///< Interface revision.
#define VBE_DISPI_INDEX_XRES    0x1 ///< Visible width, in pixels.
#define VBE_DISPI_INDEX_YRES    0x2 ///< Visible height, in scan lines.
#define VBE_DISPI_INDEX_BPP     0x3 ///< Bits per pixel.
#define VBE_DISPI_INDEX_ENABLE  0x4 ///< Mode enable and framebuffer selection.
#define VBE_DISPI_INDEX_BANK    0x5 ///< Window bank, unused here.
#define VBE_DISPI_INDEX_VIRT_W  0x6 ///< Framebuffer width; sets the stride.
#define VBE_DISPI_INDEX_VIRT_H  0x7 ///< Framebuffer height, in scan lines.
#define VBE_DISPI_INDEX_X_OFF   0x8 ///< Horizontal pan, in pixels.
#define VBE_DISPI_INDEX_Y_OFF   0x9 ///< Vertical pan, in scan lines.
#define VBE_DISPI_INDEX_VMEM64K 0xA ///< Video memory size, in 64 KiB units.

#define VBE_DISPI_DISABLED      0x00U ///< Leave the extension off.
#define VBE_DISPI_ENABLED       0x01U ///< Adopt the programmed mode.
#define VBE_DISPI_LFB_ENABLED   0x40U ///< Address memory through the BAR, not the window.

/// Lowest interface revision that has the registers used here.
#define VBE_DISPI_ID_MIN        0xB0C0U
/// Highest interface revision currently defined.
#define VBE_DISPI_ID_MAX        0xB0C5U
/// @}

/// @name VGA DAC
/// @brief The palette is loaded through the ordinary VGA DAC ports.
/// @{
#define VGA_DAC_MASK            0x03C6 ///< Pixel mask register.
#define VGA_DAC_WRITE_INDEX     0x03C8 ///< Write index register.
#define VGA_DAC_DATA            0x03C9 ///< Data register.
/// @}

/// @name PCI
/// @{
/// Device type of a VGA-compatible controller: class 3, subclass 0, interface 0.
///
/// Matching on the class rather than on 1234:1111 is what makes this backend
/// work unchanged on QEMU's stdvga, on virtio-vga and on bochs-display. It is
/// not sufficient on its own -- cirrus-vga is the same class and has no DISPI --
/// so the interface revision is checked separately, and that is the real gate.
#define PCI_TYPE_VGA_CONTROLLER 0x030000

/// Bit 0 of a BAR: 0 selects a memory BAR, 1 an I/O BAR.
#define PCI_BAR_TYPE_IO         0x00000001U
/// Bits 2:1 of a memory BAR: the address width.
#define PCI_BAR_MEM_TYPE_MASK   0x00000006U
/// Bits 2:1 value meaning a 32-bit memory BAR.
#define PCI_BAR_MEM_TYPE_32     0x00000000U
/// Address bits of a memory BAR; the low four carry flags.
#define PCI_BAR_MEM_ADDR_MASK   0xFFFFFFF0U
/// @}

/// @name Mode
/// @{
#define VBE_WIDTH               1024U ///< Horizontal resolution, in pixels.
#define VBE_HEIGHT              768U  ///< Vertical resolution, in scan lines.
#define VBE_BPP                 8U    ///< Bits per pixel.

/// Bytes per scan line. VIRT_W is programmed to the visible width, so the
/// stride is the width, and at one byte per pixel that is the width in bytes.
#define VBE_STRIDE              VBE_WIDTH

/// Pixels a cell occupies horizontally.
#define VBE_CELL_WIDTH          VIDEO_FONT_WIDTH
/// Scan lines a cell occupies.
#define VBE_CELL_HEIGHT         VIDEO_FONT_HEIGHT
/// Bytes one text row occupies.
#define VBE_ROW_BYTES           (VBE_CELL_HEIGHT * VBE_STRIDE)
/// @}

/// @name Virtual buffer
/// @{
/// Text rows the framebuffer is treated as, of which VIDEO_ROWS are visible.
///
/// Scrolling pans this buffer instead of moving pixels, so it has to be taller
/// than the screen for there to be anywhere to pan to. 256 rows is 4 MiB, which
/// is one page-directory entry's worth of mapping and leaves 208 distinct
/// viewport positions before a rebase is needed. See __vbe_rebase() for why it
/// is this much larger than the screen rather than merely larger.
#define VBE_RING_ROWS           256U

/// Bytes the virtual buffer occupies, and therefore how much is mapped.
#define VBE_RING_BYTES          (VBE_RING_ROWS * VBE_ROW_BYTES)

/// Kernel virtual address the framebuffer is mapped at.
///
/// Deliberately not the physical address: where the BAR happens to land is the
/// firmware's business, and keeping the two separate means the mapping does not
/// move when it does.
///
/// The window is free by construction. The kernel's linear map starts at
/// 0xC0000000 (boot/linker/kernel.lds) and covers at most the 896 MB of low
/// memory that boot.c admits, so it cannot reach 0xF8000000; that is exactly
/// where the linker script's KERNEL_HIGHMEM region begins, and no section is
/// placed in it. vmem's mapping area is 0xE8000000-0xEFFFFFFF, below it. The
/// checks below pin both ends against those facts.
#define VBE_FB_VIRT_BASE        0xF9000000U

/// First address above the window that must stay clear: the I/O APIC.
#define VBE_FB_VIRT_LIMIT       0xFEC00000U
/// Start of the linker script's KERNEL_HIGHMEM region.
#define VBE_KERNEL_HIGHMEM_BASE 0xF8000000U
/// @}

/// Number of scan lines the underline cursor covers.
#define VBE_CURSOR_UNDERLINE_LINES 3U
/// Pixel mask of the bar cursor: the two leftmost columns of the cell.
#define VBE_CURSOR_BAR_MASK        0xC0U
/// Timer ticks between blink toggles, giving a toggle about three times a
/// second, which is the usual terminal cursor rate.
#define VBE_CURSOR_BLINK_TICKS     (TICKS_PER_SECOND / 3U)

/// @brief Compile-time check that the geometry matches the mode and the font.
///
/// Fails the build with a negative array size if the geometry header and the
/// mode ever stop agreeing.
typedef char vbe_geometry_check
    [((VIDEO_COLUMNS == (VBE_WIDTH / VBE_CELL_WIDTH)) && (VIDEO_ROWS == (VBE_HEIGHT / VBE_CELL_HEIGHT))) ? 1 : -1];

/// @brief Compile-time check that a rebase can never overlap.
///
/// __vbe_rebase() copies the surviving rows to the opposite end of the buffer
/// while the display is still reading the old end. Source and destination must
/// be disjoint, and the destination must be clear of what the display is
/// showing. The binding case is a rebase downwards, which copies from as high as
/// row 2*VIDEO_ROWS-2 to a destination starting at VBE_RING_ROWS-VIDEO_ROWS, so
/// three screens' worth of rows is the requirement. See __vbe_rebase().
typedef char vbe_ring_headroom_check[(VBE_RING_ROWS >= (3U * VIDEO_ROWS)) ? 1 : -1];

/// @brief Compile-time check that every pan offset fits a DISPI register.
///
/// Y_OFFSET is 16 bits, so the buffer cannot be so tall that its top row is
/// unreachable.
typedef char vbe_pan_range_check[((VBE_RING_ROWS * VBE_CELL_HEIGHT) <= 0xFFFFU) ? 1 : -1];

/// @brief Compile-time check that the mapping lands in unused kernel space.
typedef char vbe_virt_window_check
    [((VBE_FB_VIRT_BASE >= VBE_KERNEL_HIGHMEM_BASE) &&
      ((VBE_FB_VIRT_BASE + VBE_RING_BYTES) <= VBE_FB_VIRT_LIMIT))
         ? 1
         : -1];

/// @brief Expansion of a glyph nibble into four pixel masks.
///
/// One entry per four glyph bits, giving 0xFF where a pixel is foreground and
/// 0x00 where it is background. Bit 7 of a glyph byte is the leftmost pixel and
/// the leftmost pixel is the lowest address, which on a little-endian machine is
/// the lowest byte of the word -- so the highest bit of the nibble selects the
/// lowest byte. Getting that backwards mirrors every glyph, which is why the
/// values are written out rather than computed.
static const uint32_t vbe_nibble_mask[16] = {
    0x00000000U, // ....
    0xFF000000U, // ...X
    0x00FF0000U, // ..X.
    0xFFFF0000U, // ..XX
    0x0000FF00U, // .X..
    0xFF00FF00U, // .X.X
    0x00FFFF00U, // .XX.
    0xFFFFFF00U, // .XXX
    0x000000FFU, // X...
    0xFF0000FFU, // X..X
    0x00FF00FFU, // X.X.
    0xFFFF00FFU, // X.XX
    0x0000FFFFU, // XX..
    0xFF00FFFFU, // XX.X
    0x00FFFFFFU, // XXX.
    0xFFFFFFFFU, // XXXX
};

/// @brief Whether discovery found a usable adapter.
static bool_t adapter_found        = false;

/// @brief Whether late_init() has mapped the framebuffer and set the mode.
///
/// Every operation is a no-op until this is true: see the file comment. It gates
/// on the mapping rather than on the mode because the mapping is what makes a
/// framebuffer store legal, and a store through an absent mapping is a page
/// fault, not a lost pixel.
static bool_t mapped               = false;

/// @brief Physical address of the framebuffer, from PCI BAR0.
static uint32_t framebuffer_phys   = 0;

/// @brief Video memory the adapter reports, in bytes.
static uint32_t framebuffer_bytes  = 0;

/// @brief The framebuffer, at the virtual address it was mapped to.
static uint8_t *framebuffer        = NULL;

/// @brief Column the cursor is drawn at.
static unsigned cursor_column      = 0;
/// @brief Row the cursor is drawn at.
static unsigned cursor_row         = 0;
/// @brief Current cursor style.
static video_cursor_style_t cursor_style = {VIDEO_CURSOR_BLOCK, true};

/// @brief The cell underneath the cursor.
///
/// The only state duplicated from the generic console, and one cell rather than
/// a copy of the screen. It is what makes the overlay removable: the overlay is
/// drawn over the cell, so putting the cell back is how it is taken away. It
/// never contains cursor pixels.
static video_cell_t cursor_cell    = {' ', 0x07};

/// @brief Whether an overlay is currently on the display.
static bool_t cursor_drawn         = false;

/// @brief Blink phase: whether the overlay should be shown at this instant.
static bool_t cursor_blink_phase   = true;

/// @brief Ticks counted towards the next blink toggle.
static unsigned cursor_blink_ticks = 0;

/// @brief Virtual row currently shown at the top of the screen.
///
/// Always within [0, VBE_RING_ROWS - VIDEO_ROWS], so the whole screen is always
/// inside the buffer and no row ever straddles the end. Scrolling moves this and
/// reprograms the pan; see vbe_lfb_scroll().
static unsigned start_row          = 0;

/// @brief Writes a DISPI register.
/// @param index The register index.
/// @param value The value to write.
static inline void __vbe_write(uint16_t index, uint16_t value)
{
    outports(VBE_DISPI_IOPORT_INDEX, index);
    outports(VBE_DISPI_IOPORT_DATA, value);
}

/// @brief Reads a DISPI register.
/// @param index The register index.
/// @return The value read.
static inline uint16_t __vbe_read(uint16_t index)
{
    outports(VBE_DISPI_IOPORT_INDEX, index);
    return inports(VBE_DISPI_IOPORT_DATA);
}

/// @brief Address of a virtual row's first byte.
/// @param row The virtual row.
/// @return Pointer to the row's first byte.
static inline uint8_t *__vbe_virtual_row(unsigned row) { return framebuffer + ((size_t)row * VBE_ROW_BYTES); }

/// @brief Address of a visible row's first byte.
/// @param row The visible row, 0 to VIDEO_ROWS - 1.
/// @return Pointer to the row's first byte.
static inline uint8_t *__vbe_row_address(unsigned row) { return __vbe_virtual_row(start_row + row); }

/// @brief Points the display at the current window.
///
/// One register does the whole job, which is why scrolling is nearly free: the
/// vertical pan is where in the framebuffer the display starts reading. There is
/// no wrap and no split to program, and deliberately no reliance on either --
/// past the end of video memory the display would read whatever is there, so
/// start_row is kept in range by the caller instead.
static inline void __vbe_program_pan(void) { __vbe_write(VBE_DISPI_INDEX_Y_OFF, (uint16_t)(start_row * VBE_CELL_HEIGHT)); }

/// @brief Moves the visible content to the opposite end of the buffer.
/// @param rows The scroll that ran out of room; sign gives the direction.
///
/// The buffer is finite and does not wrap, so panning eventually reaches an end.
/// When it does, the content that is still going to be visible is copied to the
/// far end and the pan restarts from there. This happens once every
/// `VBE_RING_ROWS - VIDEO_ROWS` scrolls in one direction -- 208 with the current
/// sizes -- and costs one copy of at most a screenful, about 1.3 Mcycles
/// measured, which is why a plain copy is preferred to anything cleverer.
///
/// It has to work in both directions. Forward scrolling runs off the far end;
/// paging back into scrollback runs off the near end just as readily, and the
/// generic layer will happily do 480 backward scrolls in a row.
///
/// Nothing is copied out of the region the display is currently reading, and
/// nothing is copied into it, so no partially-moved state is ever shown: the pan
/// is reprogrammed only once the copy is complete, and a register change is
/// picked up between frames. The compile-time headroom check is what guarantees
/// the two regions are disjoint -- see vbe_ring_headroom_check.
static void __vbe_rebase(int rows)
{
    unsigned base     = (rows > 0) ? 0U : (VBE_RING_ROWS - VIDEO_ROWS);
    unsigned distance = (rows > 0) ? (unsigned)rows : (unsigned)(-rows);

    // A scroll of a whole screen or more leaves nothing to preserve; the caller
    // repaints every row it uncovered, which is all of them.
    if (distance < VIDEO_ROWS) {
        unsigned keep = VIDEO_ROWS - distance;
        // Forward: the rows below the scroll survive, and land at the top of the
        // new window. Backward: the rows above it survive, and land `distance`
        // rows further down, leaving the uncovered rows at the top.
        unsigned source      = (rows > 0) ? (start_row + distance) : start_row;
        unsigned destination = (rows > 0) ? base : (base + distance);
        memcpy(__vbe_virtual_row(destination), __vbe_virtual_row(source), (size_t)keep * VBE_ROW_BYTES);
    }

    start_row = base;
}

/// @brief Loads the 16 console colours into the DAC.
///
/// Identical to the planar backend's load, and identical on purpose: the two
/// backends must put the same colours on screen for the same console.
static void __vbe_load_palette(void)
{
    outportb(VGA_DAC_MASK, 0xFF);
    outportb(VGA_DAC_WRITE_INDEX, 0x00);
    for (unsigned index = 0; index < count_of(video_palette_16); ++index) {
        // The DAC takes 6 bits per component, so drop the low two bits of each
        // 8-bit value rather than letting the hardware truncate the high ones.
        outportb(VGA_DAC_DATA, (uint8_t)(video_palette_16[index].red >> 2U));
        outportb(VGA_DAC_DATA, (uint8_t)(video_palette_16[index].green >> 2U));
        outportb(VGA_DAC_DATA, (uint8_t)(video_palette_16[index].blue >> 2U));
    }
}

/// @brief Glyph bitmap of a cell's character.
/// @param cell The cell.
/// @return VBE_CELL_HEIGHT bytes of bitmap, one per scan line.
static inline const uint8_t *__vbe_cell_glyph(video_cell_t cell)
{
    return &video_font_8x16[(unsigned)cell.character * VBE_CELL_HEIGHT];
}

/// @brief Draws one cell from an explicit glyph.
/// @param destination Address of the cell's top-left pixel.
/// @param glyph VBE_CELL_HEIGHT bytes of bitmap, one per scan line.
/// @param attribute Foreground index in the low nibble, background in the high.
///
/// A cell is eight pixels wide, so a scan line is eight bytes at an
/// eight-byte-aligned address: two aligned 32-bit stores, built by selecting
/// between a foreground and a background byte replicated four ways. No
/// read-modify-write, and no per-pixel loop.
static inline void __vbe_draw_cell(uint8_t *destination, const uint8_t *glyph, uint8_t attribute)
{
    uint32_t foreground = 0x01010101U * (uint32_t)(attribute & 0x0FU);
    uint32_t background = 0x01010101U * (uint32_t)((attribute >> 4U) & 0x0FU);

    for (unsigned line = 0; line < VBE_CELL_HEIGHT; ++line) {
        uint8_t bits              = glyph[line];
        uint32_t high             = vbe_nibble_mask[bits >> 4U];
        uint32_t low              = vbe_nibble_mask[bits & 0x0FU];
        volatile uint32_t *pixels = (volatile uint32_t *)(destination + ((size_t)line * VBE_STRIDE));
        pixels[0]                 = (foreground & high) | (background & ~high);
        pixels[1]                 = (foreground & low) | (background & ~low);
    }
}

/// @brief Draws a run of cells lying within a single row.
/// @param column The column of the first cell.
/// @param row The row the run lies in.
/// @param cells The cells to draw.
/// @param count How many cells the run covers.
///
/// The loop is line-major, so one pass writes one scan line across the whole run
/// and the stores walk forward through consecutive addresses. That is what the
/// cache and any write-combining want; going cell-major instead would jump the
/// stride between every pair of stores.
static void __vbe_draw_run(unsigned column, unsigned row, const video_cell_t *cells, unsigned count)
{
    uint8_t *const base = __vbe_row_address(row) + ((size_t)column * VBE_CELL_WIDTH);

    for (unsigned line = 0; line < VBE_CELL_HEIGHT; ++line) {
        volatile uint32_t *pixels = (volatile uint32_t *)(base + ((size_t)line * VBE_STRIDE));

        for (unsigned cell = 0; cell < count; ++cell) {
            uint8_t attribute = cells[cell].attribute;
            uint32_t foreground = 0x01010101U * (uint32_t)(attribute & 0x0FU);
            uint32_t background = 0x01010101U * (uint32_t)((attribute >> 4U) & 0x0FU);
            uint8_t bits        = __vbe_cell_glyph(cells[cell])[line];
            uint32_t high       = vbe_nibble_mask[bits >> 4U];
            uint32_t low        = vbe_nibble_mask[bits & 0x0FU];

            *pixels++ = (foreground & high) | (background & ~high);
            *pixels++ = (foreground & low) | (background & ~low);
        }
    }
}

/// @brief Whether the cursor position currently refers to the visible screen.
/// @return true when it does.
///
/// Scrolling can carry the cursor row out of view before the console gets round
/// to placing it again, and nothing may be drawn or restored there meanwhile.
static inline bool_t __vbe_cursor_on_screen(void)
{
    return (cursor_column < VIDEO_COLUMNS) && (cursor_row < VIDEO_ROWS);
}

/// @brief Whether the overlay should be on the display at this instant.
/// @return true when it should.
static inline bool_t __vbe_cursor_should_show(void)
{
    if (cursor_style.shape == VIDEO_CURSOR_HIDDEN) {
        return false;
    }
    // A steady cursor ignores the blink phase entirely.
    return cursor_style.blinking ? cursor_blink_phase : true;
}

/// @brief Takes the overlay off the display, restoring the cell underneath.
///
/// Idempotent: it does nothing unless an overlay is actually drawn. Every path
/// about to move the cursor, change its appearance, or repaint the cell it sits
/// on goes through here first, which is what keeps stale blocks from being left
/// behind.
static void __vbe_cursor_hide(void)
{
    if (!cursor_drawn) {
        return;
    }
    // Clear the flag first: the restore is an ordinary cell render, and leaving
    // the flag set through it would let a re-entrant call restore twice.
    cursor_drawn = false;
    if (mapped && __vbe_cursor_on_screen()) {
        __vbe_draw_run(cursor_column, cursor_row, &cursor_cell, 1);
    }
}

/// @brief Puts the overlay on the display, if it belongs there now.
///
/// The cursor is merged into the cell's glyph rather than drawn on top of it, so
/// the whole cell is still written with the same two stores per scan line. A
/// block cursor fills the cell; underline and bar leave the character visible
/// around them.
static void __vbe_cursor_show(void)
{
    if (cursor_drawn || !mapped) {
        return;
    }
    if (!__vbe_cursor_should_show() || !__vbe_cursor_on_screen()) {
        return;
    }

    const uint8_t *source = __vbe_cell_glyph(cursor_cell);
    uint8_t glyph[VBE_CELL_HEIGHT];

    for (unsigned line = 0; line < VBE_CELL_HEIGHT; ++line) {
        uint8_t overlay = 0x00U;
        if (cursor_style.shape == VIDEO_CURSOR_BLOCK) {
            overlay = 0xFFU;
        } else if (cursor_style.shape == VIDEO_CURSOR_UNDERLINE) {
            overlay = (line >= (VBE_CELL_HEIGHT - VBE_CURSOR_UNDERLINE_LINES)) ? 0xFFU : 0x00U;
        } else if (cursor_style.shape == VIDEO_CURSOR_BAR) {
            overlay = VBE_CURSOR_BAR_MASK;
        }
        glyph[line] = (uint8_t)(source[line] | overlay);
    }

    // The cursor takes the cell's own foreground colour, as the text-mode
    // hardware cursor does. A cell erased to attribute 0 would give a black
    // block on black, so fall back to the default foreground to keep the cursor
    // visible -- which is what makes it survive backspace and delete.
    uint8_t attribute = cursor_cell.attribute;
    if ((attribute & 0x0FU) == ((attribute >> 4U) & 0x0FU)) {
        attribute = (uint8_t)((attribute & 0xF0U) | 0x07U);
    }

    __vbe_draw_cell(__vbe_row_address(cursor_row) + ((size_t)cursor_column * VBE_CELL_WIDTH), glyph, attribute);
    cursor_drawn = true;
}

/// @brief pci_scan() callback recording the first VGA controller found.
/// @param device The PCI device identifier.
/// @param vendor_id The vendor ID.
/// @param device_id The device ID.
/// @param extra Where to store the device identifier.
/// @return Always 0, so the scan visits every device.
static int __vbe_match_adapter(uint32_t device, uint16_t vendor_id, uint16_t device_id, void *extra)
{
    uint32_t *found = (uint32_t *)extra;

    // Keep the first match. A machine with two VGA-class devices would need a
    // way to choose; none of the configurations this backend targets has one.
    if ((found == NULL) || (*found != PCI_NONE)) {
        return 0;
    }
    *found = device;
    pr_debug("Found VGA controller %04x:%04x at PCI device 0x%08x.\n", vendor_id, device_id, device);
    return 0;
}

/// @brief Extracts a usable physical address from a BAR value.
/// @param bar The raw BAR contents.
/// @param address Where to store the address.
/// @return 0 on success, -1 if the BAR is not one this backend can use.
///
/// The encoding is checked rather than assumed. An I/O BAR is not memory at all;
/// a 64-bit BAR spans two registers and reports its type in bits 2:1, and while
/// one with a zero upper half would happen to work on a 32-bit kernel, silently
/// treating it as 32-bit would be wrong the first time it was not. Both are
/// rejected with a diagnostic instead.
static int __vbe_bar_address(uint32_t bar, uint32_t *address)
{
    if ((bar & PCI_BAR_TYPE_IO) != 0U) {
        pr_emerg("Framebuffer BAR 0x%08x is an I/O BAR, not memory.\n", bar);
        return -1;
    }
    if ((bar & PCI_BAR_MEM_TYPE_MASK) != PCI_BAR_MEM_TYPE_32) {
        pr_emerg("Framebuffer BAR 0x%08x is not a 32-bit memory BAR.\n", bar);
        return -1;
    }

    uint32_t candidate = bar & PCI_BAR_MEM_ADDR_MASK;
    if (candidate == 0U) {
        pr_emerg("Framebuffer BAR is unassigned; the firmware left it at zero.\n");
        return -1;
    }
    // The mapping is built a page at a time, so the base has to be a page base.
    // A 16 MiB BAR is 16 MiB aligned in practice; this catches the case where it
    // is not, rather than mapping something shifted.
    if ((candidate & (PAGE_SIZE - 1U)) != 0U) {
        pr_emerg("Framebuffer BAR 0x%08x is not page aligned.\n", candidate);
        return -1;
    }

    *address = candidate;
    return 0;
}

/// @brief Discovers the adapter, without touching memory.
/// @return 0 on success, -1 when there is no usable adapter.
///
/// Runs from video_init(), before there is any allocator or any mapping, so it
/// is restricted to what needs neither: DISPI port reads and PCI configuration
/// cycles. pci.c takes no locks and allocates nothing, so it is safe this early.
///
/// The interface revision is the real gate. The PCI class only says "this is a
/// VGA-compatible controller", which cirrus-vga also is without having any of
/// these registers; a revision in the DISPI range says the registers are there.
static int vbe_lfb_init(void)
{
    uint16_t revision = __vbe_read(VBE_DISPI_INDEX_ID);
    if ((revision < VBE_DISPI_ID_MIN) || (revision > VBE_DISPI_ID_MAX)) {
        pr_emerg("No Bochs VBE adapter: interface revision reads 0x%04x.\n", revision);
        return -1;
    }

    uint32_t device = PCI_NONE;
    if (pci_scan(&__vbe_match_adapter, PCI_TYPE_VGA_CONTROLLER, &device) != 0) {
        pr_emerg("Failed to scan the PCI bus for a VGA controller.\n");
        return -1;
    }
    if (device == PCI_NONE) {
        pr_emerg("No VGA-class PCI device found to take the framebuffer from.\n");
        return -1;
    }

    uint32_t bar = 0;
    if (pci_read_32(device, PCI_BASE_ADDRESS_0, &bar) != 0) {
        pr_emerg("Failed to read BAR0 of PCI device 0x%08x.\n", device);
        return -1;
    }
    if (__vbe_bar_address(bar, &framebuffer_phys) < 0) {
        return -1;
    }

    // What the adapter says it has, which is the only bound on how much of the
    // buffer can be used. 64 KiB units, so this cannot overflow 32 bits.
    framebuffer_bytes = (uint32_t)__vbe_read(VBE_DISPI_INDEX_VMEM64K) * 64U * 1024U;
    if (framebuffer_bytes < VBE_RING_BYTES) {
        pr_emerg(
            "Adapter reports %u KiB of video memory; %u KiB is needed.\n", framebuffer_bytes / 1024U,
            (unsigned)(VBE_RING_BYTES / 1024U));
        return -1;
    }

    adapter_found = true;
    pr_notice(
        "Bochs VBE adapter found: revision 0x%04x, framebuffer at 0x%08x, %u KiB of video memory.\n", revision,
        framebuffer_phys, framebuffer_bytes / 1024U);
    // Nothing is drawn and no register is programmed yet: the framebuffer is not
    // mapped, and will not be until late_init(). The console keeps recording.
    return 0;
}

/// @brief Maps the framebuffer and brings up the mode.
/// @return 0 on success, -1 on failure.
///
/// Runs from video_late_init(), after paging_init(). Order matters: the mapping
/// comes first, because programming the mode is what starts the display reading
/// the framebuffer, and the buffer is cleared through the mapping.
static int vbe_lfb_late_init(void)
{
    if (!adapter_found) {
        pr_emerg("No adapter was found during discovery; the console stays on serial only.\n");
        return -1;
    }

    page_directory_t *pgd = paging_get_main_pgd();
    if (pgd == NULL) {
        pr_emerg("No main page directory to map the framebuffer into.\n");
        return -1;
    }

    if (mem_upd_vm_area(
            pgd, VBE_FB_VIRT_BASE, framebuffer_phys, VBE_RING_BYTES,
            MM_RW | MM_PRESENT | MM_GLOBAL | MM_UPDADDR) < 0) {
        pr_emerg("Failed to map %u KiB of framebuffer at 0x%08x.\n", (unsigned)(VBE_RING_BYTES / 1024U),
                 (unsigned)VBE_FB_VIRT_BASE);
        return -1;
    }
    framebuffer = (uint8_t *)VBE_FB_VIRT_BASE;

    // The extension has to be off while the mode registers are written; it
    // latches them when it is switched back on.
    __vbe_write(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_DISABLED);
    __vbe_write(VBE_DISPI_INDEX_XRES, VBE_WIDTH);
    __vbe_write(VBE_DISPI_INDEX_YRES, VBE_HEIGHT);
    __vbe_write(VBE_DISPI_INDEX_BPP, VBE_BPP);
    // The framebuffer is wider than nothing and exactly as wide as the screen,
    // which is what makes the stride the width. Setting it explicitly rather
    // than inheriting whatever the previous mode left keeps __vbe_row_address()
    // honest.
    __vbe_write(VBE_DISPI_INDEX_VIRT_W, VBE_WIDTH);
    __vbe_write(VBE_DISPI_INDEX_X_OFF, 0);
    __vbe_write(VBE_DISPI_INDEX_Y_OFF, 0);
    __vbe_write(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_ENABLED | VBE_DISPI_LFB_ENABLED);

    // Confirm the adapter took the mode. It clamps rather than refuses if it
    // cannot, and a silently clamped mode would put every subsequent address in
    // the wrong place.
    uint16_t width  = __vbe_read(VBE_DISPI_INDEX_XRES);
    uint16_t height = __vbe_read(VBE_DISPI_INDEX_YRES);
    uint16_t depth  = __vbe_read(VBE_DISPI_INDEX_BPP);
    uint16_t stride = __vbe_read(VBE_DISPI_INDEX_VIRT_W);
    if ((width != VBE_WIDTH) || (height != VBE_HEIGHT) || (depth != VBE_BPP) || (stride != VBE_STRIDE)) {
        pr_emerg(
            "Adapter did not take the mode: asked for %ux%u at %u bpp, stride %u; got %ux%u at %u bpp, stride %u.\n",
            (unsigned)VBE_WIDTH, (unsigned)VBE_HEIGHT, (unsigned)VBE_BPP, (unsigned)VBE_STRIDE, width, height, depth,
            stride);
        __vbe_write(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_DISABLED);
        framebuffer = NULL;
        return -1;
    }

    __vbe_load_palette();

    // Blank the whole buffer, not just the visible part: scrolling pans into the
    // rest, and whatever was there would scroll into view. Setting the mode
    // clears video memory on the adapters this targets, but that is their
    // behaviour rather than a guarantee of the interface.
    volatile uint32_t *word = (volatile uint32_t *)framebuffer;
    for (size_t index = 0; index < (VBE_RING_BYTES / sizeof(uint32_t)); ++index) {
        word[index] = 0;
    }

    // Show the buffer from the top.
    start_row = 0;
    __vbe_program_pan();

    // Last, and only now: from here on every entry point will touch memory.
    mapped = true;
    pr_notice(
        "VBE mode set: %ux%u at %u bpp, mapped 0x%08x at 0x%08x, %ux%u cells.\n", (unsigned)VBE_WIDTH,
        (unsigned)VBE_HEIGHT, (unsigned)VBE_BPP, framebuffer_phys, (unsigned)VBE_FB_VIRT_BASE,
        (unsigned)VIDEO_COLUMNS, (unsigned)VIDEO_ROWS);
    return 0;
}

/// @brief Draws cells to the display.
/// @param column Column of the first cell.
/// @param row Row of the first cell.
/// @param cells The cells to draw.
/// @param count How many cells to draw.
static void vbe_lfb_put_cells(unsigned column, unsigned row, const video_cell_t *cells, unsigned count)
{
    if (!mapped || (cells == NULL) || (column >= VIDEO_COLUMNS) || (row >= VIDEO_ROWS)) {
        return;
    }

    unsigned next  = 0;
    bool_t touched = false;
    while ((count > 0) && (row < VIDEO_ROWS)) {
        // Split the range at row boundaries and hand each piece over whole, so
        // the line-major loop inside covers as much as possible in one sweep.
        unsigned run = VIDEO_COLUMNS - column;
        if (run > count) {
            run = count;
        }

        // A run covering the cursor's cell replaces what the overlay is sitting
        // on, so refresh the cache first, then let the render wipe the overlay:
        // it is redrawn once the whole range is on screen.
        if ((row == cursor_row) && (cursor_column >= column) && (cursor_column < (column + run))) {
            cursor_cell  = cells[next + (cursor_column - column)];
            cursor_drawn = false;
            touched      = true;
        }

        __vbe_draw_run(column, row, &cells[next], run);

        next += run;
        count -= run;
        column = 0;
        ++row;
    }

    // Put the cursor back on top of whatever was just drawn under it.
    if (touched) {
        __vbe_cursor_show();
    }
}

/// @brief Moves the displayed content vertically.
/// @param rows Positive moves content up, negative moves it down.
///
/// Usually nothing is copied: the framebuffer is taller than the screen and this
/// only moves where the display starts reading, so a scroll is one register
/// write. When the pan runs out of room -- in either direction -- the content is
/// rebased to the far end first; see __vbe_rebase().
static void vbe_lfb_scroll(int rows)
{
    if (!mapped || (rows == 0)) {
        return;
    }

    // The overlay is pixels like everything else, so the move carries it along.
    // Follow it, or the next hide would restore the cell at the wrong row and
    // leave a block behind at the right one. If it lands off screen there is
    // nothing to take away: the console repaints every row it uncovers before
    // that row can come back into view.
    if (cursor_drawn) {
        int moved = (int)cursor_row - rows;
        if ((moved < 0) || (moved >= (int)VIDEO_ROWS)) {
            cursor_drawn = false;
        }
        cursor_row = (unsigned)moved;
    }

    // The display reads forward from the pan offset and does not wrap, so the
    // window has to stay inside the buffer. Both ends are reachable: forward
    // scrolling walks off the far end, and paging back through scrollback walks
    // off the near end.
    int limit    = (int)(VBE_RING_ROWS - VIDEO_ROWS);
    int position = (int)start_row + rows;
    if ((position < 0) || (position > limit)) {
        __vbe_rebase(rows);
    } else {
        start_row = (unsigned)position;
    }

    __vbe_program_pan();
}

/// @brief Places the cursor.
/// @param column The column.
/// @param row The row.
/// @param cell The cell the cursor now sits on.
///
/// The generic console repaints the cell the cursor is leaving before calling
/// this, so there is no old cursor to erase here.
static void vbe_lfb_set_cursor_position(unsigned column, unsigned row, video_cell_t cell)
{
    if (column >= VIDEO_COLUMNS) {
        column = VIDEO_COLUMNS - 1;
    }
    if (row >= VIDEO_ROWS) {
        row = VIDEO_ROWS - 1;
    }

    // Take the old overlay away before forgetting where it was, then adopt the
    // new position and the cell it sits on, then draw. Hiding first is the whole
    // point: doing it in any other order is what leaves a block behind on the
    // line the cursor just left.
    __vbe_cursor_hide();
    cursor_column = column;
    cursor_row    = row;
    cursor_cell   = cell;
    __vbe_cursor_show();
}

/// @brief Selects the cursor appearance.
/// @param style The style to adopt.
///
/// Like the planar backend and unlike the text one, this can honour
/// VIDEO_CURSOR_BAR literally: it draws a vertical bar down the left of the
/// cell, which is what the ANSI code asks for.
static void vbe_lfb_set_cursor_style(video_cursor_style_t style)
{
    if ((cursor_style.shape == style.shape) && (cursor_style.blinking == style.blinking)) {
        return;
    }
    // Same shape as every other transition: whatever the old style drew comes
    // off before the new one goes on.
    __vbe_cursor_hide();
    cursor_style = style;
    // Start a fresh blink cycle visible, so a style change always shows.
    cursor_blink_phase = true;
    cursor_blink_ticks = 0;
    __vbe_cursor_show();
}

/// @brief Advances the blink, once per timer tick.
///
/// Counting ticks here rather than asking for a periodic callback keeps the rate
/// the backend's own business and costs nothing on the ticks that do not toggle.
/// A steady cursor still counts but never changes what is displayed.
///
/// This runs in the timer interrupt, which is live before the framebuffer is
/// mapped, so the `mapped` gate here is load-bearing rather than defensive.
static void vbe_lfb_cursor_blink(void)
{
    if (!mapped || !cursor_style.blinking || (cursor_style.shape == VIDEO_CURSOR_HIDDEN)) {
        return;
    }
    if (++cursor_blink_ticks < VBE_CURSOR_BLINK_TICKS) {
        return;
    }
    cursor_blink_ticks = 0;
    cursor_blink_phase = !cursor_blink_phase;
    if (cursor_blink_phase) {
        __vbe_cursor_show();
    } else {
        __vbe_cursor_hide();
    }
}

/// @brief The VBE linear-framebuffer backend.
const video_backend_t video_backend = {
    .name                = "vbe-1024x768x8",
    .columns             = VIDEO_COLUMNS,
    .rows                = VIDEO_ROWS,
    .init                = vbe_lfb_init,
    .late_init           = vbe_lfb_late_init,
    .put_cells           = vbe_lfb_put_cells,
    .scroll              = vbe_lfb_scroll,
    .set_cursor_position = vbe_lfb_set_cursor_position,
    .set_cursor_style    = vbe_lfb_set_cursor_style,
    .cursor_blink        = vbe_lfb_cursor_blink,
};
