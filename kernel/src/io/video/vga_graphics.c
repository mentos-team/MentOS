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

#include "io/port_io.h"
#include "io/video_backend.h"
#include "stdbool.h"
#include "stddef.h"
#include "string.h"

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
/// Graphics controller register selecting the plane reads come from.
#define VGA_GC_READ_MAP       0x04
/// CRT controller register whose bit 7 write-protects registers 0x00-0x07.
#define VGA_CRTC_PROTECT      0x11

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
/// Bytes occupied by one plane.
#define VGA_PLANE_BYTES       (VGA_HEIGHT * VGA_BYTES_PER_LINE)

/// Scan lines occupied by one cell.
#define VGA_CELL_HEIGHT       VGA_FONT_HEIGHT
/// Number of scan lines the underline cursor covers.
#define VGA_CURSOR_UNDERLINE_LINES 3
/// Pixel mask of the bar cursor: the two leftmost columns of the cell.
#define VGA_CURSOR_BAR_MASK   0xC0U

/// @brief Compile-time check that the geometry matches the mode and the font.
///
/// Fails the build with a negative array size if the geometry header and the
/// mode ever stop agreeing.
typedef char vga_graphics_geometry_check
    [((VIDEO_COLUMNS == (VGA_WIDTH / VGA_FONT_WIDTH)) && (VIDEO_ROWS == (VGA_HEIGHT / VGA_CELL_HEIGHT))) ? 1 : -1];

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
static video_cursor_style_t cursor_style = VIDEO_CURSOR_BLOCK;

/// @brief The cell underneath the cursor.
///
/// This is the only state duplicated from the generic console, and it is one
/// cell rather than a copy of the screen. Drawing the cursor means drawing that
/// cell with the cursor merged into its glyph, which needs both its character
/// (so an underline or bar cursor does not erase it) and its attribute (so the
/// cursor takes the cell's own foreground colour, as the text-mode hardware
/// cursor does). It is kept up to date by watching put_cells().
static video_cell_t cursor_cell = {' ', 0x07};

/// @brief Selects the plane that byte accesses read from and write to.
/// @param plane The plane to select, 0-3.
///
/// Both halves matter: the write mask decides which planes a store lands in,
/// and the read map decides which plane a load comes from. Scrolling moves
/// memory with memmove(), which does both.
static inline void __vga_select_plane(unsigned plane)
{
    plane &= (VGA_PLANES - 1);
    outportb(VGA_GC_INDEX, VGA_GC_READ_MAP);
    outportb(VGA_GC_DATA, (uint8_t)plane);
    outportb(VGA_SC_INDEX, VGA_SC_MAP_MASK);
    outportb(VGA_SC_DATA, (uint8_t)(1U << plane));
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

/// @brief Draws one cell from an explicit glyph.
/// @param column The column of the cell.
/// @param row The row of the cell.
/// @param glyph VGA_CELL_HEIGHT bytes of bitmap, one per scan line.
/// @param attribute Foreground index in the low nibble, background in the high.
///
/// For each plane, a scan line is foreground where the glyph has a set bit and
/// background elsewhere, so the byte to store is decided entirely by whether
/// that plane's bit is set in the foreground and background indices.
static void __vga_draw_glyph(unsigned column, unsigned row, const uint8_t *glyph, uint8_t attribute)
{
    uint8_t *cell     = vga_graphics_memory + (row * VGA_CELL_HEIGHT * VGA_BYTES_PER_LINE) + column;
    unsigned foreground = attribute & 0x0FU;
    unsigned background = (attribute >> 4U) & 0x0FU;

    for (unsigned plane = 0; plane < VGA_PLANES; ++plane) {
        __vga_select_plane(plane);
        bool_t foreground_set = ((foreground >> plane) & 1U) != 0U;
        bool_t background_set = ((background >> plane) & 1U) != 0U;

        // Both bits equal means the whole cell is one value in this plane.
        if (foreground_set == background_set) {
            uint8_t value = foreground_set ? 0xFFU : 0x00U;
            for (unsigned line = 0; line < VGA_CELL_HEIGHT; ++line) {
                cell[line * VGA_BYTES_PER_LINE] = value;
            }
            continue;
        }

        for (unsigned line = 0; line < VGA_CELL_HEIGHT; ++line) {
            uint8_t bits                    = glyph[line];
            cell[line * VGA_BYTES_PER_LINE] = foreground_set ? bits : (uint8_t)~bits;
        }
    }
}

/// @brief Draws one cell using the font glyph for its character.
/// @param column The column of the cell.
/// @param row The row of the cell.
/// @param cell The cell to draw.
static inline void __vga_draw_cell(unsigned column, unsigned row, video_cell_t cell)
{
    __vga_draw_glyph(column, row, &vga_graphics_font[(unsigned)cell.character * VGA_CELL_HEIGHT], cell.attribute);
}

/// @brief Draws the cursor over the cell it sits on.
///
/// The cursor is merged into the glyph rather than drawn on top of it, so the
/// whole cell is still written with plain byte stores. A block cursor fills the
/// cell; underline and bar leave the character visible around them.
static void __vga_draw_cursor(void)
{
    if (cursor_style == VIDEO_CURSOR_HIDDEN) {
        return;
    }

    const uint8_t *source = &vga_graphics_font[(unsigned)cursor_cell.character * VGA_CELL_HEIGHT];
    uint8_t glyph[VGA_CELL_HEIGHT];

    for (unsigned line = 0; line < VGA_CELL_HEIGHT; ++line) {
        uint8_t overlay = 0x00U;
        if (cursor_style == VIDEO_CURSOR_BLOCK) {
            overlay = 0xFFU;
        } else if (cursor_style == VIDEO_CURSOR_UNDERLINE) {
            overlay = (line >= (VGA_CELL_HEIGHT - VGA_CURSOR_UNDERLINE_LINES)) ? 0xFFU : 0x00U;
        } else if (cursor_style == VIDEO_CURSOR_BAR) {
            overlay = VGA_CURSOR_BAR_MASK;
        }
        glyph[line] = (uint8_t)(source[line] | overlay);
    }

    __vga_draw_glyph(cursor_column, cursor_row, glyph, cursor_cell.attribute);
}

/// @brief Brings up the mode.
/// @return Always 0.
static int vga_graphics_init(void)
{
    __vga_set_mode();
    __vga_load_palette();

    // Start from a blank display. The console clears the screen straight after
    // this returns, but leaving whatever the text mode had in memory on screen
    // in the meantime would show as noise.
    for (unsigned plane = 0; plane < VGA_PLANES; ++plane) {
        __vga_select_plane(plane);
        memset(vga_graphics_memory, 0, VGA_PLANE_BYTES);
    }

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

    unsigned next = 0;
    while ((count > 0) && (row < VIDEO_ROWS)) {
        // Draw as much of this row as the run covers, then wrap.
        unsigned run = VIDEO_COLUMNS - column;
        if (run > count) {
            run = count;
        }
        for (unsigned offset = 0; offset < run; ++offset) {
            video_cell_t cell = cells[next + offset];
            __vga_draw_cell(column + offset, row, cell);
            // Keep the cached cursor cell current, so that redrawing the cursor
            // does not resurrect stale content underneath it.
            if ((row == cursor_row) && ((column + offset) == cursor_column)) {
                cursor_cell = cell;
            }
        }
        next += run;
        count -= run;
        column = 0;
        ++row;
    }
}

/// @brief Moves the displayed content vertically.
/// @param rows Positive moves content up, negative moves it down.
static void vga_graphics_scroll(int rows)
{
    if (!mode_set || (rows == 0)) {
        return;
    }
    unsigned distance = (rows > 0) ? (unsigned)rows : (unsigned)(-rows);
    // Nothing would survive the move; the caller repaints what it needs.
    if (distance >= VIDEO_ROWS) {
        return;
    }

    unsigned offset = distance * VGA_CELL_HEIGHT * VGA_BYTES_PER_LINE;
    unsigned moved  = VGA_PLANE_BYTES - offset;

    // One memmove per plane, rather than redrawing thousands of glyphs.
    for (unsigned plane = 0; plane < VGA_PLANES; ++plane) {
        __vga_select_plane(plane);
        if (rows > 0) {
            memmove(vga_graphics_memory, vga_graphics_memory + offset, moved);
        } else {
            memmove(vga_graphics_memory + offset, vga_graphics_memory, moved);
        }
    }
}

/// @brief Places the cursor.
/// @param column The column.
/// @param row The row.
///
/// The generic console repaints the cell the cursor is leaving before calling
/// this, so there is no old cursor to erase here.
static void vga_graphics_set_cursor_position(unsigned column, unsigned row)
{
    if (column >= VIDEO_COLUMNS) {
        column = VIDEO_COLUMNS - 1;
    }
    if (row >= VIDEO_ROWS) {
        row = VIDEO_ROWS - 1;
    }
    cursor_column = column;
    cursor_row    = row;
    if (!mode_set) {
        return;
    }
    __vga_draw_cursor();
}

/// @brief Selects the cursor appearance.
/// @param style The style to adopt.
///
/// Unlike the text backend, this one can honour VIDEO_CURSOR_BAR literally: it
/// draws a vertical bar down the left of the cell, which is what the ANSI code
/// asks for and what VGA text mode is unable to produce.
static void vga_graphics_set_cursor_style(video_cursor_style_t style)
{
    if (cursor_style == style) {
        return;
    }
    video_cursor_style_t previous = cursor_style;
    cursor_style                  = style;
    if (!mode_set) {
        return;
    }
    // Repaint the cell first, to clear whatever the old style drew.
    if (previous != VIDEO_CURSOR_HIDDEN) {
        __vga_draw_cell(cursor_column, cursor_row, cursor_cell);
    }
    __vga_draw_cursor();
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
};
