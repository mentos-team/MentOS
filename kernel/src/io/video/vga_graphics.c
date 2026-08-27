/// @file vga_graphics.c
/// @brief Graphical VGA video backend: 640x480, 16 colours, planar.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.
///
/// Renders the console as text drawn with an 8x16 bitmap font, giving exactly
/// 80x30 cells. The mode is planar: four one-bit-per-pixel planes overlaid at
/// 0xA0000, with the four bits of a pixel forming an index into a 16-entry
/// palette. That maps straight onto the console's attribute byte, so this
/// backend needs no colour translation.
///
/// The font being 8 pixels wide is what makes this cheap: a glyph scan line is
/// exactly one byte per plane at a byte-aligned address, so a cell is drawn with
/// plain byte stores, with no read-modify-write and no per-pixel loop.
///
/// Every access to the graphics window traps into the emulated adapter and costs
/// on the order of 580 cycles whatever its width, so the two things that decide
/// how this performs are how many accesses it makes and how many of them it can
/// avoid. Drawing therefore batches in both directions at once, plane-major and
/// four cells to a 32-bit write (see __vga_draw_run), and scrolling makes no
/// memory accesses at all: video memory is a ring of rows and scrolling moves
/// the window the display reads from (see vga_graphics_scroll).
///
/// Unlike the text backend, this hardware is NOT usable at reset: until init()
/// has programmed the mode, the adapter is still in text mode and writes to
/// 0xA0000 would go nowhere useful. Every operation is therefore gated on
/// `mode_set`, as the backend contract requires. The generic console records its
/// state regardless and clears the screen right after init(), so nothing is left
/// half-drawn. The one visible consequence is that a panic before video_init()
/// leaves no output on screen; the serial log still has it.

// Setup the logging for this file (do this before any other include).
#include "sys/kernel_levels.h"           // Include kernel log levels.
#define __DEBUG_HEADER__ "[VGAGFX]"      ///< Change header.
#define __DEBUG_LEVEL__  LOGLEVEL_NOTICE ///< Set log level.
#include "io/debug.h"                    // Include debugging functions.

#include "hardware/timer.h"
#include "io/port_io.h"
#include "io/video_backend.h"
#include "stdbool.h"
#include "stddef.h"

#include "vga_graphics_font.h"
#include "vga_graphics_mode.h"
#include "vga_graphics_palette.h"

/// Attribute controller index and write port.
#define VGA_AC_INDEX          0x03C0
/// Miscellaneous output register (write).
#define VGA_MISC_WRITE        0x03C2
/// Sequencer index port.
#define VGA_SC_INDEX          0x03C4
/// Sequencer data port.
#define VGA_SC_DATA           0x03C5
/// DAC pixel mask register.
#define VGA_DAC_MASK          0x03C6
/// DAC write index register.
#define VGA_DAC_WRITE_INDEX   0x03C8
/// DAC data register.
#define VGA_DAC_DATA          0x03C9
/// Graphics controller index port.
#define VGA_GC_INDEX          0x03CE
/// Graphics controller data port.
#define VGA_GC_DATA           0x03CF
/// CRT controller index port.
#define VGA_CRTC_INDEX        0x03D4
/// CRT controller data port.
#define VGA_CRTC_DATA         0x03D5
/// Input status register; reading it resets the attribute controller flip-flop.
#define VGA_INPUT_STATUS      0x03DA

/// Sequencer register holding the plane write mask.
#define VGA_SC_MAP_MASK       0x02
/// CRT controller register whose bit 7 write-protects registers 0x00-0x07.
#define VGA_CRTC_PROTECT      0x11
/// CRT controller register holding the high byte of the display start address.
#define VGA_CRTC_START_HIGH   0x0C
/// CRT controller register holding the low byte of the display start address.
#define VGA_CRTC_START_LOW    0x0D
/// CRT controller register carrying bit 8 of the line compare, among others.
#define VGA_CRTC_OVERFLOW     0x07
/// CRT controller register carrying bit 9 of the line compare, among others.
#define VGA_CRTC_MAX_SCAN     0x09
/// CRT controller register holding the low 8 bits of the line compare.
#define VGA_CRTC_LINE_COMPARE 0x18
/// Bit of the overflow register that holds line compare bit 8.
#define VGA_OVERFLOW_LC8      0x10U
/// Bit of the maximum scan line register that holds line compare bit 9.
#define VGA_MAX_SCAN_LC9      0x40U
/// Line compare value that disables the split: the largest the field can hold.
#define VGA_LINE_COMPARE_OFF  1023U

/// Base address of the graphics window this mode selects.
#define VGA_GRAPHICS_ADDRESS  0xA0000U

/// Number of colour planes.
#define VGA_PLANES            4
/// Horizontal resolution, in pixels.
#define VGA_WIDTH             640
/// Vertical resolution, in scan lines.
#define VGA_HEIGHT            480
/// Bytes per scan line, per plane: 640 pixels at one bit per pixel.
#define VGA_BYTES_PER_LINE    (VGA_WIDTH / 8)

/// Scan lines occupied by one cell.
#define VGA_CELL_HEIGHT       VGA_FONT_HEIGHT
/// Bytes one text row occupies in one plane.
#define VGA_ROW_BYTES         (VGA_CELL_HEIGHT * VGA_BYTES_PER_LINE)
/// Whole text rows that fit the 64 KB window the CPU can address.
///
/// This is the size of the circular buffer scrolling pans around; 51 rows of
/// 1280 bytes is 65280, the largest whole number of rows inside 64 KB.
#define VGA_VIRTUAL_ROWS      (65536U / VGA_ROW_BYTES)
/// Bytes the circular buffer occupies.
#define VGA_BUFFER_BYTES      (VGA_VIRTUAL_ROWS * VGA_ROW_BYTES)
/// Number of scan lines the underline cursor covers.
#define VGA_CURSOR_UNDERLINE_LINES 3
/// Pixel mask of the bar cursor: the two leftmost columns of the cell.
#define VGA_CURSOR_BAR_MASK   0xC0U
/// Timer ticks between blink toggles, giving a toggle about three times a
/// second, which is the usual terminal cursor rate.
#define VGA_CURSOR_BLINK_TICKS (TICKS_PER_SECOND / 3U)

/// @brief Compile-time check that the geometry matches the mode and the font.
///
/// Fails the build with a negative array size if the geometry header and the
/// mode ever stop agreeing.
typedef char vga_graphics_geometry_check
    [((VIDEO_COLUMNS == (VGA_WIDTH / VGA_FONT_WIDTH)) && (VIDEO_ROWS == (VGA_HEIGHT / VGA_CELL_HEIGHT))) ? 1 : -1];

/// @brief Compile-time check that the circular buffer is larger than the screen.
///
/// Scrolling pans around VGA_VIRTUAL_ROWS rows while showing VIDEO_ROWS of them,
/// so the buffer has to be the larger of the two for there to be anywhere to pan
/// to. Fails the build with a negative array size otherwise.
typedef char vga_graphics_buffer_check[(VGA_VIRTUAL_ROWS > VIDEO_ROWS) ? 1 : -1];

/// @brief The graphics window, addressed as bytes of the selected plane.
static uint8_t *const vga_graphics_memory = (uint8_t *)VGA_GRAPHICS_ADDRESS;

/// @brief Whether init() has programmed the mode.
///
/// Every operation is a no-op until this is true: see the file comment.
static bool_t mode_set = false;

/// @brief Column the cursor is drawn at.
static unsigned cursor_column = 0;
/// @brief Row the cursor is drawn at.
static unsigned cursor_row = 0;
/// @brief Current cursor style.
static video_cursor_style_t cursor_style = {VIDEO_CURSOR_BLOCK, true};

/// @brief The cell underneath the cursor.
///
/// This is the only state duplicated from the generic console, and it is one
/// cell rather than a copy of the screen. It is what makes the cursor removable:
/// the overlay is drawn over the cell, so putting the cell back is how the
/// overlay is taken away. Drawing needs both halves -- the character, so that an
/// underline or bar cursor does not erase it, and the attribute, so the cursor
/// takes the cell's own foreground colour the way the text-mode hardware cursor
/// does.
///
/// It is updated from two places: set_cursor_position(), when the cursor moves
/// onto a different cell, and put_cells(), when a run repaints the cell it is
/// sitting on. It never contains cursor pixels.
static video_cell_t cursor_cell = {' ', 0x07};

/// @brief Whether an overlay is currently on the display.
///
/// The whole lifecycle turns on this flag. Nothing may draw the overlay while it
/// is set, and nothing may restore the cell underneath while it is clear, or the
/// two get out of step and a stale block is left behind.
static bool_t cursor_drawn = false;

/// @brief Blink phase: whether the overlay should be shown at this instant.
static bool_t cursor_blink_phase = true;

/// @brief Ticks counted towards the next blink toggle.
static unsigned cursor_blink_ticks = 0;

/// @brief Virtual row currently shown at the top of the screen.
///
/// Video memory is treated as a ring of VGA_VIRTUAL_ROWS rows and scrolling
/// moves this window rather than the pixels; see __vga_program_window().
static unsigned start_row = 0;

/// @brief Address of a visible row's first byte in the current plane.
/// @param row The visible row, 0 to VIDEO_ROWS - 1.
/// @return Pointer to the row's first byte.
///
/// A cell never straddles the ring's seam, because rows are the unit the ring
/// is built from, so this is the only place the wrap has to be considered.
static inline uint8_t *__vga_row_address(unsigned row)
{
    unsigned virtual_row = start_row + row;
    // start_row and row are both below VGA_VIRTUAL_ROWS, so one fold is enough.
    if (virtual_row >= VGA_VIRTUAL_ROWS) {
        virtual_row -= VGA_VIRTUAL_ROWS;
    }
    return vga_graphics_memory + (virtual_row * VGA_ROW_BYTES);
}

/// @brief Points the display at the current window.
///
/// Two registers do the work. The start address picks the first byte shown,
/// which is what makes scrolling free. The line compare then closes the ring: it
/// is the scan line at which the display abandons the sequential address and
/// restarts from address 0, so it goes exactly where the window runs off the end
/// of the buffer.
///
/// The split is only programmed when the window actually runs off the end, and
/// parked at VGA_LINE_COMPARE_OFF otherwise. In principle any value past the
/// last scan line would be equally inert, but measurement says otherwise: with
/// the split parked at an intermediate value the display truncates instead of
/// ignoring it, so "off" means the maximum the field can hold and nothing else.
static void __vga_program_window(void)
{
    unsigned start        = start_row * VGA_ROW_BYTES;
    unsigned rows_to_end  = VGA_VIRTUAL_ROWS - start_row;
    unsigned line_compare = rows_to_end * VGA_CELL_HEIGHT;
    uint8_t register_value;

    if (line_compare >= VGA_HEIGHT) {
        line_compare = VGA_LINE_COMPARE_OFF;
    }

    // Line compare goes first. The two values are briefly inconsistent, and in
    // that window splitting too early shows stale but valid content, whereas
    // splitting too late would show memory past the end of the buffer.
    outportb(VGA_CRTC_INDEX, VGA_CRTC_LINE_COMPARE);
    outportb(VGA_CRTC_DATA, (uint8_t)(line_compare & 0xFFU));

    // The remaining two bits of the line compare live in registers that also
    // carry vertical timings. Those timings are rebuilt from the mode table
    // rather than read back, so a scroll can only ever disturb the one bit it
    // owns, whatever the adapter happens to return on a read.
    register_value =
        (uint8_t)((vga_mode_crtc[VGA_CRTC_OVERFLOW] & ~VGA_OVERFLOW_LC8) | (((line_compare >> 8U) & 1U) << 4U));
    outportb(VGA_CRTC_INDEX, VGA_CRTC_OVERFLOW);
    outportb(VGA_CRTC_DATA, register_value);

    register_value =
        (uint8_t)((vga_mode_crtc[VGA_CRTC_MAX_SCAN] & ~VGA_MAX_SCAN_LC9) | (((line_compare >> 9U) & 1U) << 6U));
    outportb(VGA_CRTC_INDEX, VGA_CRTC_MAX_SCAN);
    outportb(VGA_CRTC_DATA, register_value);

    outportb(VGA_CRTC_INDEX, VGA_CRTC_START_HIGH);
    outportb(VGA_CRTC_DATA, (uint8_t)((start >> 8U) & 0xFFU));
    outportb(VGA_CRTC_INDEX, VGA_CRTC_START_LOW);
    outportb(VGA_CRTC_DATA, (uint8_t)(start & 0xFFU));
}

/// @brief Selects the plane that byte writes land in.
/// @param plane The plane to select, 0-3.
///
/// This backend never reads video memory back, so only the sequencer's write
/// mask is needed; the graphics controller's read map can be left alone. Index
/// and data go out as a single 16-bit write, which the VGA splits into the index
/// register and the data register.
static inline void __vga_select_write_plane(unsigned plane)
{
    outports(VGA_SC_INDEX, (uint16_t)(((1U << (plane & (VGA_PLANES - 1))) << 8U) | VGA_SC_MAP_MASK));
}

/// @brief Programs the register set for this mode.
static void __vga_set_mode(void)
{
    unsigned index;

    outportb(VGA_MISC_WRITE, vga_mode_misc);

    for (index = 0; index < VGA_MODE_SEQUENCER_REGISTERS; ++index) {
        outportb(VGA_SC_INDEX, (uint8_t)index);
        outportb(VGA_SC_DATA, vga_mode_sequencer[index]);
    }

    // Registers 0x00-0x07 are write-protected while CRTC 0x11 bit 7 is set, and
    // the mode we are leaving may well have set it. Clear it before writing the
    // timings; the value written for 0x11 below keeps it clear.
    outportb(VGA_CRTC_INDEX, VGA_CRTC_PROTECT);
    outportb(VGA_CRTC_DATA, (uint8_t)(inportb(VGA_CRTC_DATA) & 0x7FU));

    for (index = 0; index < VGA_MODE_CRTC_REGISTERS; ++index) {
        outportb(VGA_CRTC_INDEX, (uint8_t)index);
        outportb(VGA_CRTC_DATA, vga_mode_crtc[index]);
    }

    for (index = 0; index < VGA_MODE_GRAPHICS_REGISTERS; ++index) {
        outportb(VGA_GC_INDEX, (uint8_t)index);
        outportb(VGA_GC_DATA, vga_mode_graphics[index]);
    }

    for (index = 0; index < VGA_MODE_ATTRIBUTE_REGISTERS; ++index) {
        // The attribute controller multiplexes index and data on one port, and
        // a read of the input status register puts it back into index state.
        (void)inportb(VGA_INPUT_STATUS);
        outportb(VGA_AC_INDEX, (uint8_t)index);
        outportb(VGA_AC_INDEX, vga_mode_attribute[index]);
    }

    // Lock the palette and switch the display back on.
    (void)inportb(VGA_INPUT_STATUS);
    outportb(VGA_AC_INDEX, 0x20);
}

/// @brief Loads the 16 console colours into the DAC.
static void __vga_load_palette(void)
{
    outportb(VGA_DAC_MASK, 0xFF);
    outportb(VGA_DAC_WRITE_INDEX, 0x00);
    for (unsigned index = 0; index < count_of(vga_graphics_palette); ++index) {
        // The DAC takes 6 bits per component, so drop the low two bits of each
        // 8-bit value rather than letting the hardware truncate the high ones.
        outportb(VGA_DAC_DATA, (uint8_t)(vga_graphics_palette[index].red >> 2U));
        outportb(VGA_DAC_DATA, (uint8_t)(vga_graphics_palette[index].green >> 2U));
        outportb(VGA_DAC_DATA, (uint8_t)(vga_graphics_palette[index].blue >> 2U));
    }
}

/// @brief The byte one cell contributes to one scan line of one plane.
/// @param cell The cell being drawn.
/// @param glyph The bitmap to use, which is not always the cell's own.
/// @param line The scan line within the cell.
/// @param plane The plane being written.
/// @return The byte to store.
///
/// A scan line is the glyph where the foreground contributes to this plane and
/// its complement where the background does, so when the two agree the whole
/// cell is one constant and the glyph is irrelevant.
static inline uint8_t __vga_cell_byte(video_cell_t cell, const uint8_t *glyph, unsigned line, unsigned plane)
{
    unsigned foreground_set = (cell.attribute >> plane) & 1U;
    unsigned background_set = (cell.attribute >> (4U + plane)) & 1U;
    if (foreground_set == background_set) {
        return foreground_set ? 0xFFU : 0x00U;
    }
    uint8_t bits = glyph[line];
    return foreground_set ? bits : (uint8_t)~bits;
}

/// @brief Glyph bitmap of a cell's character.
/// @param cell The cell.
/// @return VGA_CELL_HEIGHT bytes of bitmap, one per scan line.
static inline const uint8_t *__vga_cell_glyph(video_cell_t cell)
{
    return &vga_graphics_font[(unsigned)cell.character * VGA_CELL_HEIGHT];
}

/// @brief Draws a run of cells lying within a single row.
/// @param column The column of the first cell.
/// @param row The row the run lies in.
/// @param cells The cells to draw.
/// @param count How many cells the run covers.
///
/// Two things make this fast, both of them about batching:
///
/// Rendering is plane-major. Each plane is selected once and the entire run is
/// then written for it, instead of cycling through all four planes per cell, so
/// a run of N cells costs 4 port accesses rather than 4N. The console flushes
/// from the cursor to the end of the line on every character, so N is usually
/// most of a row.
///
/// Within a plane the loop is line-major, which makes the destination bytes of
/// one scan line contiguous and lets four cells go out in a single 32-bit device
/// access. That matters more than it looks: every access to the graphics window
/// is a trap into the emulated adapter, and measurements put a byte write at
/// roughly 580 cycles regardless of width, so quartering the number of accesses
/// is very nearly a straight 4x. Writes are aligned first, because an unaligned
/// access would be split back into pieces and give the saving away.
static void __vga_draw_run(unsigned column, unsigned row, const video_cell_t *cells, unsigned count)
{
    uint8_t *const base = __vga_row_address(row) + column;

    for (unsigned plane = 0; plane < VGA_PLANES; ++plane) {
        __vga_select_write_plane(plane);

        for (unsigned line = 0; line < VGA_CELL_HEIGHT; ++line) {
            uint8_t *const destination = base + (line * VGA_BYTES_PER_LINE);
            unsigned cell              = 0;

            // Lead-in, until the destination is word aligned.
            while ((cell < count) && ((((uintptr_t)(destination + cell)) & 3U) != 0U)) {
                destination[cell] = __vga_cell_byte(cells[cell], __vga_cell_glyph(cells[cell]), line, plane);
                ++cell;
            }

            // Four cells per device access.
            while ((cell + 4U) <= count) {
                uint32_t word =
                    ((uint32_t)__vga_cell_byte(cells[cell], __vga_cell_glyph(cells[cell]), line, plane)) |
                    ((uint32_t)__vga_cell_byte(cells[cell + 1U], __vga_cell_glyph(cells[cell + 1U]), line, plane)
                     << 8U) |
                    ((uint32_t)__vga_cell_byte(cells[cell + 2U], __vga_cell_glyph(cells[cell + 2U]), line, plane)
                     << 16U) |
                    ((uint32_t)__vga_cell_byte(cells[cell + 3U], __vga_cell_glyph(cells[cell + 3U]), line, plane)
                     << 24U);
                *(volatile uint32_t *)(destination + cell) = word;
                cell += 4U;
            }

            // Tail.
            while (cell < count) {
                destination[cell] = __vga_cell_byte(cells[cell], __vga_cell_glyph(cells[cell]), line, plane);
                ++cell;
            }
        }
    }
}

/// @brief Draws one cell from an explicit glyph.
/// @param column The column of the cell.
/// @param row The row of the cell.
/// @param glyph VGA_CELL_HEIGHT bytes of bitmap, one per scan line.
/// @param attribute Foreground index in the low nibble, background in the high.
///
/// Only the cursor needs this, because it draws a glyph that is not the font
/// entry for any character. Everything else goes through __vga_draw_run(). One
/// cell is too small for the batching above to be worth the bookkeeping.
static void __vga_draw_glyph(unsigned column, unsigned row, const uint8_t *glyph, uint8_t attribute)
{
    uint8_t *const cell     = __vga_row_address(row) + column;
    video_cell_t synthetic = {0, attribute};

    for (unsigned plane = 0; plane < VGA_PLANES; ++plane) {
        __vga_select_write_plane(plane);
        for (unsigned line = 0; line < VGA_CELL_HEIGHT; ++line) {
            cell[line * VGA_BYTES_PER_LINE] = __vga_cell_byte(synthetic, glyph, line, plane);
        }
    }
}

/// @brief Whether the cursor position currently refers to the visible screen.
/// @return true when it does.
///
/// Scrolling can carry the cursor row out of view before the console gets round
/// to placing it again, and nothing may be drawn or restored there meanwhile.
static inline bool_t __vga_cursor_on_screen(void)
{
    return (cursor_column < VIDEO_COLUMNS) && (cursor_row < VIDEO_ROWS);
}

/// @brief Whether the overlay should be on the display at this instant.
/// @return true when it should.
static inline bool_t __vga_cursor_should_show(void)
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
/// that is about to move the cursor, change its appearance, or repaint the cell
/// it sits on goes through here first, which is what keeps stale blocks from
/// being left behind.
static void __vga_cursor_hide(void)
{
    if (!cursor_drawn) {
        return;
    }
    // Clear the flag first: the restore is an ordinary cell render, and leaving
    // the flag set through it would let a re-entrant call restore twice.
    cursor_drawn = false;
    if (mode_set && __vga_cursor_on_screen()) {
        __vga_draw_run(cursor_column, cursor_row, &cursor_cell, 1);
    }
}

/// @brief Puts the overlay on the display, if it belongs there now.
///
/// The cursor is merged into the cell's glyph rather than drawn on top of it, so
/// the whole cell is still written with plain byte stores. A block cursor fills
/// the cell; underline and bar leave the character visible around them.
static void __vga_cursor_show(void)
{
    if (cursor_drawn || !mode_set) {
        return;
    }
    if (!__vga_cursor_should_show() || !__vga_cursor_on_screen()) {
        return;
    }

    const uint8_t *source = &vga_graphics_font[(unsigned)cursor_cell.character * VGA_CELL_HEIGHT];
    uint8_t glyph[VGA_CELL_HEIGHT];

    for (unsigned line = 0; line < VGA_CELL_HEIGHT; ++line) {
        uint8_t overlay = 0x00U;
        if (cursor_style.shape == VIDEO_CURSOR_BLOCK) {
            overlay = 0xFFU;
        } else if (cursor_style.shape == VIDEO_CURSOR_UNDERLINE) {
            overlay = (line >= (VGA_CELL_HEIGHT - VGA_CURSOR_UNDERLINE_LINES)) ? 0xFFU : 0x00U;
        } else if (cursor_style.shape == VIDEO_CURSOR_BAR) {
            overlay = VGA_CURSOR_BAR_MASK;
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

    __vga_draw_glyph(cursor_column, cursor_row, glyph, attribute);
    cursor_drawn = true;
}

/// @brief Brings up the mode.
/// @return Always 0.
static int vga_graphics_init(void)
{
    __vga_set_mode();
    __vga_load_palette();

    // Blank the whole ring, not just the visible part: scrolling pans into the
    // rest, and whatever the text mode left there would scroll into view. The
    // console clears the screen straight after this returns, but that only
    // covers the 30 rows currently on display.
    for (unsigned plane = 0; plane < VGA_PLANES; ++plane) {
        __vga_select_write_plane(plane);
        volatile uint32_t *word = (volatile uint32_t *)vga_graphics_memory;
        for (unsigned index = 0; index < (VGA_BUFFER_BYTES / 4U); ++index) {
            word[index] = 0;
        }
    }

    // Show the ring from the top.
    start_row = 0;
    __vga_program_window();

    mode_set = true;
    pr_notice("VGA graphics mode set: %dx%d, 16 colours, %ux%u cells.\n", VGA_WIDTH, VGA_HEIGHT,
              (unsigned)VIDEO_COLUMNS, (unsigned)VIDEO_ROWS);
    return 0;
}

/// @brief Draws cells to the display.
/// @param column Column of the first cell.
/// @param row Row of the first cell.
/// @param cells The cells to draw.
/// @param count How many cells to draw.
static void vga_graphics_put_cells(unsigned column, unsigned row, const video_cell_t *cells, unsigned count)
{
    if (!mode_set || (cells == NULL) || (column >= VIDEO_COLUMNS) || (row >= VIDEO_ROWS)) {
        return;
    }

    unsigned next  = 0;
    bool_t touched = false;
    while ((count > 0) && (row < VIDEO_ROWS)) {
        // Split the range at row boundaries and hand each piece over whole, so
        // that plane selection is paid once per row rather than once per cell.
        unsigned run = VIDEO_COLUMNS - column;
        if (run > count) {
            run = count;
        }

        // A run that covers the cursor's cell replaces what the overlay is
        // sitting on, so refresh the cache first, then let the render wipe the
        // overlay: it is redrawn once the whole range is on screen.
        if ((row == cursor_row) && (cursor_column >= column) && (cursor_column < (column + run))) {
            cursor_cell  = cells[next + (cursor_column - column)];
            cursor_drawn = false;
            touched      = true;
        }

        __vga_draw_run(column, row, &cells[next], run);

        next += run;
        count -= run;
        column = 0;
        ++row;
    }

    // Put the cursor back on top of whatever was just drawn under it.
    if (touched) {
        __vga_cursor_show();
    }
}

/// @brief Moves the displayed content vertically.
/// @param rows Positive moves content up, negative moves it down.
///
/// Nothing is copied. Video memory is a ring of VGA_VIRTUAL_ROWS rows and this
/// only moves the window the display reads from, so a scroll costs a handful of
/// port writes instead of moving every visible pixel in all four planes.
///
/// That is not merely faster, it is what makes scrolling look right. Copying the
/// planes one after another while the display is scanning them means that, for
/// the duration of the copy, the four bits of a pixel's colour come from
/// different scroll generations, which is what produced the coloured and
/// duplicated text. Moving the window instead is a register change the display
/// picks up between frames, so no intermediate state is ever shown.
static void vga_graphics_scroll(int rows)
{
    if (!mode_set || (rows == 0)) {
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

    // Panning is modular, so any distance is meaningful: the caller repaints
    // whatever the move uncovered.
    int ring     = (int)VGA_VIRTUAL_ROWS;
    int position = ((int)start_row + (rows % ring)) % ring;
    if (position < 0) {
        position += ring;
    }
    start_row = (unsigned)position;

    __vga_program_window();
}

/// @brief Places the cursor.
/// @param column The column.
/// @param row The row.
///
/// The generic console repaints the cell the cursor is leaving before calling
/// this, so there is no old cursor to erase here.
static void vga_graphics_set_cursor_position(unsigned column, unsigned row, video_cell_t cell)
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
    __vga_cursor_hide();
    cursor_column = column;
    cursor_row    = row;
    cursor_cell   = cell;
    __vga_cursor_show();
}

/// @brief Selects the cursor appearance.
/// @param style The style to adopt.
///
/// Unlike the text backend, this one can honour VIDEO_CURSOR_BAR literally: it
/// draws a vertical bar down the left of the cell, which is what the ANSI code
/// asks for and what VGA text mode is unable to produce.
static void vga_graphics_set_cursor_style(video_cursor_style_t style)
{
    if ((cursor_style.shape == style.shape) && (cursor_style.blinking == style.blinking)) {
        return;
    }
    // Same shape as every other transition: whatever the old style drew comes
    // off before the new one goes on.
    __vga_cursor_hide();
    cursor_style = style;
    // Start a fresh blink cycle visible, so a style change always shows.
    cursor_blink_phase = true;
    cursor_blink_ticks = 0;
    __vga_cursor_show();
}

/// @brief Advances the blink, once per timer tick.
///
/// Counting ticks here rather than asking for a periodic callback keeps the rate
/// the backend's own business and costs nothing on the ticks that do not toggle.
/// A steady cursor still counts but never changes what is displayed.
static void vga_graphics_cursor_blink(void)
{
    if (!mode_set || !cursor_style.blinking || (cursor_style.shape == VIDEO_CURSOR_HIDDEN)) {
        return;
    }
    if (++cursor_blink_ticks < VGA_CURSOR_BLINK_TICKS) {
        return;
    }
    cursor_blink_ticks = 0;
    cursor_blink_phase = !cursor_blink_phase;
    if (cursor_blink_phase) {
        __vga_cursor_show();
    } else {
        __vga_cursor_hide();
    }
}

/// @brief The graphical VGA backend.
const video_backend_t video_backend = {
    .name                = "vga-640x480x16",
    .columns             = VIDEO_COLUMNS,
    .rows                = VIDEO_ROWS,
    .init                = vga_graphics_init,
    .put_cells           = vga_graphics_put_cells,
    .scroll              = vga_graphics_scroll,
    .set_cursor_position = vga_graphics_set_cursor_position,
    .set_cursor_style    = vga_graphics_set_cursor_style,
    .cursor_blink        = vga_graphics_cursor_blink,
};
