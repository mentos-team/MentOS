/// @file vga.c
/// @brief Implementation of Video Graphics Array (VGA) drivers.
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

// Include the kernel log levels.
#include "sys/kernel_levels.h"
/// Change the header.
#define __DEBUG_HEADER__ "[VGA   ]"
/// Set the log level.
#define __DEBUG_LEVEL__ LOGLEVEL_NOTICE

#include "io/vga/vga.h"

#include "io/vga/vga_palette.h"
#include "io/vga/vga_mode.h"
#include "io/vga/vga_font.h"

#include "hardware/timer.h"
#include "io/port_io.h"
#include "io/debug.h"
#include "io/video.h"
#include "stdbool.h"
#include "string.h"
#include "math.h"

/// Counts the number of elements of an array.
#define COUNT_OF(x) ((sizeof(x) / sizeof(0 [x])) / ((size_t)(!(sizeof(x) % sizeof(0 [x])))))

/// Attribute Controller index port.
#define AC_INDEX 0x03C0
/// Attribute Controller write port.
#define AC_WRITE 0x03C0
/// Attribute Controller data port.
#define AC_READ 0x03C1
/// Miscellaneous output register.
#define MISC_WRITE 0x03C2
/// Miscellaneous input register.
#define MISC_READ 0x03CC
/// Sequence controller index.
#define SC_INDEX 0x03C4
/// Sequence controller data.
#define SC_DATA 0x03C5
/// DAC Mask Register.
#define PALETTE_MASK 0x03C6
/// Controls the DAC.
#define PALETTE_READ 0x03C7
/// Controls the DAC index.
#define PALETTE_INDEX 0x03C8
/// Controls the DAC data.
#define PALETTE_DATA 0x03C9
/// Graphics controller index.
#define GC_INDEX 0x03CE
/// Graphics controller data.
#define GC_DATA 0x03CF
/// CRT controller index.
#define CRTC_INDEX 0x03D4
/// CRT controller data.
#define CRTC_DATA 0x03D5
/// By reading this port it'll go to the index state.
#define INPUT_STATUS_READ 0x03DA

/// VGA pointers for drawing operations.
typedef struct {
    /// Writes a pixel.
    void (*write_pixel)(int x, int y, unsigned char c);
    /// Reads a pixel.
    unsigned (*read_pixel)(int x, int y);
    /// Draws a rectangle.
    void (*draw_rect)(int x, int y, int wd, int ht, unsigned char c);
    /// Fills a rectangle.
    void (*fill_rect)(int x, int y, int wd, int ht, unsigned char c);
} vga_ops_t;

/// VGA font details.
typedef struct {
    const unsigned char *font; ///< Pointer to the array holding the shape of each character.
    unsigned width;            ///< Width of the font.
    unsigned height;           ///< Height of the font.
} vga_font_t;

/// VGA driver details.
typedef struct {
    int width;        ///< Screen's width.
    int height;       ///< Screen's height.
    int bpp;          ///< Bits per pixel (bpp).
    char *address;    ///< Starting address of the screen.
    vga_ops_t *ops;   ///< Writing operations.
    vga_font_t *font; ///< The current font.
} vga_driver_t;

/// Is VGA enabled.
static bool_t vga_enable = false;
/// The stored palette.
palette_entry_t stored_palette[256];
/// A buffer for storing a copy of the video memory.
char vidmem[262144];
/// Current driver.
static vga_driver_t *driver = NULL;

// ============================================================================
// == VGA MODEs ===============================================================
/// Number of sequencer registers.
#define MODE_NUM_SEQ_REGS 5
/// Number of CRTC registers.
#define MODE_NUM_CRTC_REGS 25
/// Number of Graphics Controller (GC) registers.
#define MODE_NUM_GC_REGS 9
/// Number of Attribute Controller (AC) registers.
#define MODE_NUM_AC_REGS (16 + 5)
/// Total number of registers.
#define MODE_NUM_REGS (1 + MODE_NUM_SEQ_REGS + MODE_NUM_CRTC_REGS + MODE_NUM_GC_REGS + MODE_NUM_AC_REGS) // 61

/// @brief Returns the video address.
/// @return pointer to the video.
static inline char *__get_seg(void)
{
    unsigned int seg;
    outportb(GC_INDEX, 6);
    seg = (inportb(GC_DATA) >> 2) & 3;
    if ((seg == 0) || (seg == 1))
        return (char *)0xA0000;
    if (seg == 2)
        return (char *)0xB0000;
    if (seg == 3)
        return (char *)0xB8000;
    return (char *)seg;
}

/// @brief Sets the color at the given index.
/// @param index index of the palette we want to change.
/// @param r red.
/// @param g green.
/// @param b blue.
void __vga_set_color_map(unsigned int index, unsigned char r, unsigned char g, unsigned char b)
{
    outportb(PALETTE_MASK, 0xFF);
    outportb(PALETTE_INDEX, index);
    outportl(PALETTE_DATA, r);
    outportl(PALETTE_DATA, g);
    outportl(PALETTE_DATA, b);
}

/// @brief Gets the color at the given index.
/// @param index index of the palette we want to read.
/// @param r output value for red.
/// @param g output value for green.
/// @param b output value for blue.
void __vga_get_color_map(unsigned int index, unsigned char *r, unsigned char *g, unsigned char *b)
{
    outportb(PALETTE_MASK, 0xFF);
    outportb(PALETTE_READ, index);
    *r = inportl(PALETTE_DATA);
    *g = inportl(PALETTE_DATA);
    *b = inportl(PALETTE_DATA);
}

/// @brief Saves the current palette in p.
/// @param p output variable where we save the palette.
/// @param size the size of the palette.
static void __save_palette(palette_entry_t *p, size_t size)
{
    outportb(PALETTE_MASK, 0xFF);
    outportb(PALETTE_READ, 0x00);
    for (int i = 0; i < size; i++) {
        p[i].red   = inportl(PALETTE_DATA);
        p[i].green = inportl(PALETTE_DATA);
        p[i].blue  = inportl(PALETTE_DATA);
    }
}

/// @brief Loads the palette p.
/// @param p palette we are going to load.
/// @param size the size of the palette.
static void __load_palette(palette_entry_t *p, size_t size)
{
    outportb(PALETTE_MASK, 0xFF);
    outportb(PALETTE_INDEX, 0x00);
    for (int i = 0; i < size; i++) {
        outportl(PALETTE_DATA, p[i].red);
        outportl(PALETTE_DATA, p[i].green);
        outportl(PALETTE_DATA, p[i].blue);
    }
}

/// @brief Sets the current plane.
/// @param plane the plane to set.
static inline void __set_plane(unsigned int plane)
{
    static unsigned __current_plane = -1u;
    unsigned char pmask;
    plane &= 3;
    if (__current_plane == plane)
        return;
    // Store the current plane.
    __current_plane = plane;
    // Compute the plane mask.
    pmask = 1 << plane;
#if 0
    // Set read plane.
    outportb(GC_INDEX, 4);
    outportb(GC_DATA, plane);
    // Set write plane.
    outportb(SC_INDEX, 2);
    outportb(SC_DATA, pmask);
#else
    outports(GC_INDEX, (plane << 8u) | 4u);
    outports(SC_INDEX, (pmask << 8u) | 2u);
#endif
}

/// @brief Returns the current plane.
/// @return the current plane.
static inline unsigned __get_plane()
{
    // Set read plane.
    outportb(GC_INDEX, 4);
    return inportb(GC_DATA);
}

/// @brief Reads from the video memory.
/// @param offset where we are going to read.
/// @return the value we read.
static unsigned char __read_byte(unsigned int offset)
{
    return (unsigned char)(*(driver->address + offset));
}

/// @brief Writes onto the video memory.
/// @param offset where we are going to write.
static void __write_byte(unsigned int offset, unsigned char value)
{
    *(char *)(driver->address + offset) = value;
}

/// @brief Sets the given mode.
/// @param vga_mode the new mode we set.
static void __set_mode(vga_mode_t *vga_mode)
{
    unsigned char *ptr = &vga_mode->misc;

    // Write SEQUENCER regs.
    outportb(MISC_WRITE, *ptr);
    ++ptr;

    // Write SEQUENCER regs.
    for (unsigned i = 0; i < MODE_NUM_SEQ_REGS; i++) {
        outportb(SC_INDEX, i);
        outportb(SC_DATA, *ptr);
        ++ptr;
    }

    // Wnlock CRTC registers.
    outportb(CRTC_INDEX, 0x03);
    outportb(CRTC_DATA, inportb(CRTC_DATA) | 0x80);
    outportb(CRTC_INDEX, 0x11);
    outportb(CRTC_DATA, inportb(CRTC_DATA) & ~0x80);

    // Make sure they remain unlocked.
    ptr[0x03] |= 0x80;
    ptr[0x11] &= ~0x80;

    // write CRTC regs.
    for (unsigned i = 0; i < MODE_NUM_CRTC_REGS; i++) {
        outportb(CRTC_INDEX, i);
        outportb(CRTC_DATA, *ptr);
        ++ptr;
    }

    // write GRAPHICS CONTROLLER regs.
    for (unsigned i = 0; i < MODE_NUM_GC_REGS; i++) {
        outportb(GC_INDEX, i);
        outportb(GC_DATA, *ptr);
        ++ptr;
    }

    // write ATTRIBUTE CONTROLLER regs.
    for (unsigned i = 0; i < MODE_NUM_AC_REGS; i++) {
        inportb(INPUT_STATUS_READ);
        outportb(AC_INDEX, i);
        outportb(AC_WRITE, *ptr);
        ++ptr;
    }

    // lock 16-color palette and unblank display.
    (void)inportb(INPUT_STATUS_READ);
    outportb(AC_INDEX, 0x20);
}

/// @brief Reads the VGA registers.
/// @param vga_mode the current VGA mode.
static void __read_registers(vga_mode_t *vga_mode)
{
    unsigned char *ptr = &vga_mode->misc;

    // read MISCELLANEOUS
    *ptr = inportb(MISC_READ);
    ptr++;

    // read SEQUENCER
    for (unsigned i = 0; i < MODE_NUM_SEQ_REGS; i++) {
        outportb(SC_INDEX, i);
        *ptr = inportb(SC_DATA);
        ptr++;
    }

    // read CRTC
    for (unsigned i = 0; i < MODE_NUM_CRTC_REGS; i++) {
        outportb(CRTC_INDEX, i);
        *ptr = inportb(CRTC_DATA);
        ptr++;
    }

    // read GRAPHICS CONTROLLER
    for (unsigned i = 0; i < MODE_NUM_GC_REGS; i++) {
        outportb(GC_INDEX, i);
        *ptr = inportb(GC_DATA);
        ptr++;
    }

    // read ATTRIBUTE CONTROLLER
    for (unsigned i = 0; i < MODE_NUM_AC_REGS; i++) {
        (void)inportb(INPUT_STATUS_READ);
        outportb(AC_INDEX, i);
        *ptr = inportb(AC_READ);
        ptr++;
    }

    // lock 16-color palette and unblank display
    (void)inportb(INPUT_STATUS_READ);
    outportb(AC_INDEX, 0x20);
}

/// @brief Writes the font.
/// @param buf buffer where the font resides.
/// @param font_height the height of the font.
static void __write_font(unsigned char *buf, unsigned font_height)
{
    unsigned char seq2, seq4, gc4, gc5, gc6;
    unsigned i;

    /* save registers
__set_plane() modifies GC 4 and SEQ 2, so save them as well */
    outportb(SC_INDEX, 2);
    seq2 = inportb(SC_DATA);

    outportb(SC_INDEX, 4);
    seq4 = inportb(SC_DATA);
    /* turn off even-odd addressing (set flat addressing)
assume: chain-4 addressing already off */
    outportb(SC_DATA, seq4 | 0x04);

    outportb(GC_INDEX, 4);
    gc4 = inportb(GC_DATA);

    outportb(GC_INDEX, 5);
    gc5 = inportb(GC_DATA);
    /* turn off even-odd addressing */
    outportb(GC_DATA, gc5 & ~0x10);

    outportb(GC_INDEX, 6);
    gc6 = inportb(GC_DATA);
    /* turn off even-odd addressing */
    outportb(GC_DATA, gc6 & ~0x02);
    /* write font to plane P4 */
    __set_plane(2);
    /* write font 0 */
    for (i = 0; i < 256; i++) {
        memcpy((char *)buf, (char *)(i * 32 + 0xA0000), font_height);
        buf += font_height;
    }
    /* restore registers */
    outportb(SC_INDEX, 2);
    outportb(SC_DATA, seq2);
    outportb(SC_INDEX, 4);
    outportb(SC_DATA, seq4);
    outportb(GC_INDEX, 4);
    outportb(GC_DATA, gc4);
    outportb(GC_INDEX, 5);
    outportb(GC_DATA, gc5);
    outportb(GC_INDEX, 6);
    outportb(GC_DATA, gc6);
}

/// @brief Reverses the bits of the given number.
/// @param num the number of which we want to reverse the bits.
/// @return reversed bits.
static inline unsigned int __reverse_bits(char num)
{
    unsigned int NO_OF_BITS  = sizeof(num) * 8;
    unsigned int reverse_num = 0;
    int i;
    for (i = 0; i < NO_OF_BITS; i++) {
        if ((num & (1 << i)))
            reverse_num |= 1 << ((NO_OF_BITS - 1) - i);
    }
    return reverse_num;
}

// ============================================================================
// = WRITE/READ PIXEL FUNCTIONS (and support)
// ============================================================================

/// @brief Writes a pixel.
/// @param x x coordinates.
/// @param y y coordinates.
/// @param c color.
static inline void __write_pixel_1(int x, int y, unsigned char c)
{
    int off, mask;
    c    = (c & 1) * 0xFF;
    off  = (driver->width / 8) * y + x / 8;
    x    = (x & 7) * 1;
    mask = 0x80 >> x;
    __write_byte(off, (__read_byte(off) & ~mask) | (c & mask));
}

/// @brief Reads a pixel.
/// @param x x coordinates.
/// @param y y coordinates.
/// @return the pixel value.
static inline unsigned __read_pixel_1(unsigned x, unsigned y)
{
    unsigned off, mask;
    off  = (driver->width / 8) * y + x / 8;
    x    = (x & 7) * 1;
    mask = 0x80 >> x;
    return __read_byte(off) & mask;
}

/// @brief Writes a pixel.
/// @param x x coordinates.
/// @param y y coordinates.
/// @param c color.
static inline void __write_pixel_2(int x, int y, unsigned char c)
{
    unsigned off, mask;
    c    = (c & 3) * 0x55;
    off  = (driver->width / 4) * y + x / 4;
    x    = (x & 3) * 2;
    mask = 0xC0 >> x;
    __write_byte(off, (__read_byte(off) & ~mask) | (c & mask));
}

/// @brief Reads a pixel.
/// @param x x coordinates.
/// @param y y coordinates.
/// @return the pixel value.
static inline unsigned __read_pixel_2(unsigned x, unsigned y)
{
    unsigned off, mask;
    off  = (driver->width / 4) * y + x / 4;
    x    = (x & 3) * 2;
    mask = 0xC0 >> x;
    return __read_byte(off) & mask;
}

/// @brief Writes a pixel.
/// @param x x coordinates.
/// @param y y coordinates.
/// @param c color.
static inline void __write_pixel_4(int x, int y, unsigned char color)
{
    unsigned off, mask, plane, pmask;
    off   = (driver->width / 8) * y + x / 8;
    x     = (x & 7) * 1;
    mask  = 0x80 >> x;
    pmask = 1;
    for (plane = 0; plane < 4; ++plane) {
        __set_plane(plane);
        if (pmask & color)
            __write_byte(off, __read_byte(off) | mask);
        else
            __write_byte(off, __read_byte(off) & ~mask);
        pmask <<= 1;
    }
}

/// @brief Reads a pixel.
/// @param x x coordinates.
/// @param y y coordinates.
/// @return the pixel value.
static inline unsigned __read_pixel_4(int x, int y)
{
    int off;
    off = (driver->width / 8) * y + x / 8;
    return __read_byte(off);
}

/// @brief Writes a pixel.
/// @param x x coordinates.
/// @param y y coordinates.
/// @param c color.
static inline void __write_pixel_8(int x, int y, unsigned char color)
{
    __set_plane(x);
    __write_byte(((y * driver->width) + x) / 4, color);
}

/// @brief Reads a pixel.
/// @param x x coordinates.
/// @param y y coordinates.
/// @return the pixel value.
static inline unsigned __read_pixel_8(int x, int y)
{
    __set_plane(x);
    return __read_byte(((y * driver->width) + x) / 4);
}

// ============================================================================
// = VGA PUBLIC FUNCTIONS
// ============================================================================

int vga_is_enabled()
{
    return vga_enable;
}

int vga_width()
{
    if (vga_enable)
        return driver->width;
    return 0;
}

int vga_height()
{
    if (vga_enable)
        return driver->height;
    return 0;
}

void vga_draw_pixel(int x, int y, unsigned char color)
{
    driver->ops->write_pixel(x, y, color);
}

unsigned int vga_read_pixel(int x, int y)
{
    return driver->ops->read_pixel(x, y);
}

void vga_draw_char(int x, int y, unsigned char c, unsigned char color)
{
    static unsigned mask[] = {
        1u << 0u, //            1
        1u << 1u, //            2
        1u << 2u, //            4
        1u << 3u, //            8
        1u << 4u, //           16
        1u << 5u, //           32
        1u << 6u, //           64
        1u << 7u, //          128
        1u << 8u, //          256
    };
    const unsigned char *glyph = driver->font->font + c * driver->font->height;
    for (unsigned cy = 0; cy < driver->font->height; ++cy) {
        for (unsigned cx = 0; cx < driver->font->width; ++cx) {
            vga_draw_pixel(x + (driver->font->width - cx), y + cy, glyph[cy] & mask[cx] ? color : 0x00u);
        }
    }
}

void vga_draw_string(int x, int y, const char *str, unsigned char color)
{
    char i = 0;
    while (*str != '\0') {
        vga_draw_char(x + i * 8, y, *str, color);
        str++;
        i++;
    }
}

void vga_draw_line(int x0, int y0, int x1, int y1, unsigned char color)
{
    int dx = abs(x1 - x0), sx = sign(x1 - x0);
    int dy = abs(y1 - y0), sy = sign(y1 - y0);
    int err = (dx > dy ? dx : -dy) / 2;
    while (true) {
        vga_draw_pixel(x0, y0, color);
        if ((x0 == x1) && (y0 == y1))
            break;
        if (dx > dy) {
            x0 += sx;
            err -= dy;
            if (err < 0)
                err += dx;
            y0 += sy;
        } else {
            y0 += sy;
            err -= dx;
            if (err < 0)
                err += dy;
            x0 += sx;
        }
    }
}

void vga_draw_rectangle(int sx, int sy, int w, int h, unsigned char color)
{
    vga_draw_line(sx, sy, sx + w, sy, color);
    vga_draw_line(sx, sy, sx, sy + h, color);
    vga_draw_line(sx, sy + h, sx + w, sy + h, color);
    vga_draw_line(sx + w, sy, sx + w, sy + h, color);
}

void vga_draw_circle(int xc, int yc, int r, unsigned char color)
{
    int x = 0;
    int y = r;
    int p = 3 - 2 * r;
    if (!r)
        return;
    while (y >= x) // only formulate 1/8 of circle
    {
        vga_draw_pixel(xc - x, yc - y, color); //upper left left
        vga_draw_pixel(xc - y, yc - x, color); //upper upper left
        vga_draw_pixel(xc + y, yc - x, color); //upper upper right
        vga_draw_pixel(xc + x, yc - y, color); //upper right right
        vga_draw_pixel(xc - x, yc + y, color); //lower left left
        vga_draw_pixel(xc - y, yc + x, color); //lower lower left
        vga_draw_pixel(xc + y, yc + x, color); //lower lower right
        vga_draw_pixel(xc + x, yc + y, color); //lower right right
        if (p < 0)
            p += 4 * x++ + 6;
        else
            p += 4 * (x++ - y--) + 10;
    }
}

void vga_draw_triangle(int x1, int y1, int x2, int y2, int x3, int y3, unsigned char color)
{
    vga_draw_line(x1, y1, x2, y2, color);
    vga_draw_line(x2, y2, x3, y3, color);
    vga_draw_line(x3, y3, x1, y1, color);
}

void vga_run_test(void)
{
    vga_clear_screen();
    //pr_warning("%d\n", vga_read_pixel(10, 10));
    //vga_draw_pixel(10, 10, 2);
    //pr_warning("%d\n", vga_read_pixel(10, 10));
    //vga_draw_pixel(10, 10, 3);
    //pr_warning("%d\n", vga_read_pixel(10, 10));
    //vga_draw_pixel(10, 10, 4);
    //pr_warning("%d\n", vga_read_pixel(10, 10));
    //vga_draw_pixel(10, 10, 5);
    //pr_warning("%d\n", vga_read_pixel(10, 10));
    //for (unsigned r = 0; r <= min(vga_width() / 2, vga_height() / 2); r += 4)
    //    vga_draw_circle(vga_width() / 2, vga_height() / 2, r, 2);
    //for (unsigned y = 0; y < vga_height(); y += 2)
    //    vga_draw_line(0, y, vga_width(), y, 3);
    //for (unsigned dim = 0; dim < min(vga_width(), vga_height()); dim += 4)
    //    vga_draw_rectangle((vga_width() / 2) - (dim / 2), (vga_height() / 2) - (dim / 2), dim, dim, 4);
    //vga_draw_triangle(0,  50, 50, 0, 100, 50, 2);
    //vga_draw_string(driver->font->width, driver->font->height * 2, "Hello World!", 1);
}

// == MODEs and DRIVERs =======================================================

static vga_ops_t ops_720_480_16 = {
    .write_pixel = __write_pixel_4,
    .read_pixel  = __read_pixel_4,
    .draw_rect   = NULL,
    .fill_rect   = NULL,
};

static vga_ops_t ops_640_480_16 = {
    .write_pixel = __write_pixel_4,
    .read_pixel  = __read_pixel_4,
    .draw_rect   = NULL,
    .fill_rect   = NULL,
};

static vga_ops_t ops_320_200_256 = {
    .write_pixel = __write_pixel_8,
    .read_pixel  = __read_pixel_8,
    .draw_rect   = NULL,
    .fill_rect   = NULL,
};

static vga_font_t font_4x6 = {
    .font   = arr_4x6_font,
    .width  = 4,
    .height = 6,
};

static vga_font_t font_5x6 = {
    .font   = arr_5x6_font,
    .width  = 5,
    .height = 6,
};

static vga_font_t font_8x8 = {
    .font   = arr_8x8_font,
    .width  = 8,
    .height = 8,
};

static vga_font_t font_8x14 = {
    .font   = arr_8x14_font,
    .width  = 8,
    .height = 14,
};

static vga_font_t font_8x16 = {
    .font   = arr_8x16_font,
    .width  = 8,
    .height = 16,
};

static vga_driver_t driver_720_480_16 = {
    .width   = 720,
    .height  = 480,
    .bpp     = 16,
    .address = (char *)NULL,
    .ops     = &ops_720_480_16,
};

static vga_driver_t driver_640_480_16 = {
    .width   = 640,
    .height  = 480,
    .bpp     = 16,
    .address = (char *)NULL,
    .ops     = &ops_640_480_16,
};

static vga_driver_t driver_320_200_256 = {
    .width   = 320,
    .height  = 200,
    .bpp     = 256,
    .address = (char *)NULL,
    .ops     = &ops_320_200_256,
};

// == INITIALIZE and FINALIZE =================================================

void vga_initialize()
{
    // Save the current palette.
    __save_palette(stored_palette, 256);

    // Initialize the desired mode.

#if defined(VGA_MODE_320_200_256) // 40x25
    // Write the registers.
    __set_mode(&_mode_320_200_256);
    // Initialize the mode.
    driver = &driver_320_200_256;
    // Load the color palette.
    __load_palette(ansi_256_palette, 256);
    // Set the font.
    driver->font = &font_5x6;
#elif defined(VGA_MODE_640_480_16) // 80x60
    // Write the registers.
    __set_mode(&_mode_640_480_16);
    // Initialize the mode.
    driver = &driver_640_480_16;
    // Load the color palette.
    __load_palette(ansi_16_palette, 16);
    // Set the font.
    driver->font = &font_8x14;
#elif defined(VGA_MODE_720_480_16) // 90x60
    // Write the registers.
    __set_mode(&_mode_720_480_16);
    // Initialize the mode.
    driver = &driver_720_480_16;
    // Load the color palette.
    __load_palette(ansi_16_palette, 16);
    // Set the font.
    driver->font = &font_8x16;
#else                              // VGA_TEXT_MODE
    return;
#endif
    // Set the address.
    driver->address = __get_seg();
    // Save the content of the memory.
    memcpy(vidmem, driver->address, 0x4000);
    // Clears the screen.
    vga_clear_screen();
    // Set the vga as enabled.
    vga_enable = true;
}

void vga_finalize()
{
    memcpy(driver->address, vidmem, 256 * 1024);
    __set_mode(&_mode_80_25_text);
    __load_palette(stored_palette, 256);
    vga_enable = false;
}

static int _x               = 0;
static int _y               = 0;
static unsigned char _color = 7;
static int _cursor_state    = 0;

inline static void __vga_clear_cursor()
{
    for (unsigned cy = 0; cy < driver->font->height; ++cy)
        for (unsigned cx = 0; cx < driver->font->width; ++cx)
            vga_draw_pixel(_x + cx, _y + cy, 0);
}

inline static void __vga_draw_cursor()
{
    unsigned char color = (_cursor_state = (_cursor_state == 0)) * _color;
    for (unsigned cy = 0; cy < driver->font->height; ++cy)
        for (unsigned cx = 0; cx < driver->font->width; ++cx)
            vga_draw_pixel(_x + cx, _y + cy, color);
}

void vga_putc(int c)
{
    if (_cursor_state)
        __vga_clear_cursor();
    // If the character is '\n' go the new line.
    if (c == '\n') {
        vga_new_line();
    } else if ((c >= 0x20) && (c <= 0x7E)) {
        vga_draw_char(_x, _y, c, _color);
        if ((_x += driver->font->width) >= driver->width)
            vga_new_line();
    } else {
        return;
    }
}

void vga_puts(const char *str)
{
    while ((*str) != 0) {
        vga_putc((*str++));
    }
}

void vga_move_cursor(unsigned int x, unsigned int y)
{
    _x = x * driver->font->width;
    _y = y * driver->font->height;
    __vga_draw_cursor();
}

void vga_get_cursor_position(unsigned int *x, unsigned int *y)
{
    if (x)
        *x = _x / driver->font->width;
    if (y)
        *y = _y / driver->font->height;
}

void vga_get_screen_size(unsigned int *width, unsigned int *height)
{
    if (width)
        *width = driver->width / driver->font->width;
    if (height)
        *height = driver->height / driver->font->height;
}

void vga_clear_screen()
{
    unsigned original_plane = __get_plane();
    for (unsigned plane = 0; plane < 4; ++plane) {
        __set_plane(plane);
        memset(driver->address, 0, 64 * 1024);
    }
    __set_plane(original_plane);
    _x = 0, _y = 0;
}

void vga_new_line()
{
    // Just the 5x6 font needs some space.
    const unsigned int vertical_space = (driver->font == &font_5x6);
    // Go back at the beginning of the line.
    _x = 0;
    if ((_y += driver->font->height + vertical_space) >= (driver->height - driver->font->height)) {
        _y = 0;
        vga_clear_screen();
    }
}

void vga_update()
{
    if ((timer_get_ticks() % (TICKS_PER_SECOND / 2)) == 0) {
        __vga_draw_cursor();
    }
}

void vga_set_color(unsigned int color)
{
    _color = color;
}
