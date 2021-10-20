#include "io/vga/vga.h"

#include "io/vga/vga_palette.h"
#include "io/vga/vga_mode.h"
#include "io/vga/vga_font.h"

#include "io/port_io.h"
#include "stdbool.h"
#include "string.h"
#include "misc/debug.h"
#include "math.h"

#define COUNT_OF(x) ((sizeof(x) / sizeof(0 [x])) / ((size_t)(!(sizeof(x) % sizeof(0 [x])))))

#define AC_INDEX          0x03C0
#define AC_WRITE          0x03C0
#define AC_READ           0x03C1
#define MISC_WRITE        0x03C2
#define MISC_READ         0x03CC
#define SC_INDEX          0x03C4 // VGA sequence controller.
#define SC_DATA           0x03C5
#define PALETTE_MASK      0x03C6
#define PALETTE_READ      0x03C7
#define PALETTE_INDEX     0x03C8 // VGA digital-to-analog converter.
#define PALETTE_DATA      0x03C9
#define GC_INDEX          0x03CE // VGA graphics controller.
#define GC_DATA           0x03CF
#define CRTC_INDEX        0x03D4 // VGA CRT controller.
#define CRTC_DATA         0x03D5
#define INPUT_STATUS_READ 0x03DA

/// VGA pointers for drawing operations.
typedef struct {
    /// Writes a pixel.
    void (*write_pixel)(unsigned int x, unsigned int y, unsigned char c);
    /// Reads a pixel.
    unsigned (*read_pixel)(unsigned int x, unsigned int y);
    /// Draws a rectangle.
    void (*draw_rect)(unsigned int x, unsigned int y, unsigned int wd, unsigned int ht, unsigned char c);
    /// Fills a rectangle.
    void (*fill_rect)(unsigned int x, unsigned int y, unsigned int wd, unsigned int ht, unsigned char c);
} vga_ops_t;

/// VGA font details.
typedef struct {
    unsigned char *font; ///< Pointer to the array holding the shape of each character.
    unsigned width;      ///< Width of the font.
    unsigned height;     ///< Height of the font.
} vga_font_t;

/// VGA driver details.
typedef struct {
    int width;      ///< Screen's width.
    int height;     ///< Screen's height.
    int bpp;        ///< Bits per pixel (bpp).
    char *address;  ///< Starting address of the screen.
    vga_ops_t *ops; ///< Writing operations.
} vga_driver_t;

static bool_t vga_enable = false;
palette_entry_t stored_palette[256];
char vidmem[262144];

static vga_driver_t *driver = NULL;
static vga_font_t *font;

// ============================================================================
// == VGA MODEs ===============================================================
#define MODE_NUM_SEQ_REGS  5
#define MODE_NUM_CRTC_REGS 25
#define MODE_NUM_GC_REGS   9
#define MODE_NUM_AC_REGS   (16 + 5)
#define MODE_NUM_REGS      (1 + MODE_NUM_SEQ_REGS + MODE_NUM_CRTC_REGS + MODE_NUM_GC_REGS + MODE_NUM_AC_REGS) // 61

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

void __vga_set_color_map(unsigned int index, unsigned char r, unsigned char g, unsigned char b)
{
    outportb(PALETTE_MASK, 0xFF);
    outportb(PALETTE_INDEX, index);
    outportl(PALETTE_DATA, r);
    outportl(PALETTE_DATA, g);
    outportl(PALETTE_DATA, b);
}

void __vga_get_color_map(unsigned int index, unsigned char *r, unsigned char *g, unsigned char *b)
{
    outportb(PALETTE_MASK, 0xFF);
    outportb(PALETTE_READ, index);
    *r = inportl(PALETTE_DATA);
    *g = inportl(PALETTE_DATA);
    *b = inportl(PALETTE_DATA);
}

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

static inline void __set_plane(unsigned int plane)
{
    unsigned char pmask;
    plane &= 3;
    pmask = 1 << plane;
    // Set read plane.
    outportb(GC_INDEX, 4);
    outportb(GC_DATA, plane);
    // Set write plane.
    outportb(SC_INDEX, 2);
    outportb(SC_DATA, pmask);
}

static unsigned char __read_byte(unsigned int offset)
{
    return (unsigned char)(*(driver->address + offset));
}

static void __write_byte(unsigned int offset, unsigned char value)
{
    *(char *)(driver->address + offset) = value;
}

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

// ============================================================================
// == WRITE PIXEL FUNCTIONS ===================================================

static void __write_pixel_1(unsigned int x, unsigned int y, unsigned char c)
{
    unsigned wd_in_bytes;
    unsigned off, mask;

    c           = (c & 1) * 0xFF;
    wd_in_bytes = driver->width / 8;
    off         = wd_in_bytes * y + x / 8;
    x           = (x & 7) * 1;
    mask        = 0x80 >> x;
    __write_byte(off, (__read_byte(off) & ~mask) | (c & mask));
}

static void __write_pixel_2(unsigned int x, unsigned int y, unsigned char c)
{
    unsigned wd_in_bytes;
    unsigned off, mask;

    c           = (c & 3) * 0x55;
    wd_in_bytes = driver->width / 4;
    off         = wd_in_bytes * y + x / 4;
    x           = (x & 3) * 2;
    mask        = 0xC0 >> x;
    __write_byte(off, (__read_byte(off) & ~mask) | (c & mask));
}

static void __write_pixel_4(unsigned int x, unsigned int y, unsigned char color)
{
    int rotation = 0;
    int16_t t;
    switch (rotation) {
    case 1:
        t = x;
        x = driver->width - 1 - y;
        y = t;
        break;
    case 2:
        x = driver->width - 1 - x;
        y = driver->height - 1 - y;
        break;
    case 3:
        t = x;
        x = y;
        y = driver->height - 1 - t;
        break;
    }

    unsigned wd_in_bytes, off, mask, p, pmask;

    wd_in_bytes = driver->width / 8;
    off         = wd_in_bytes * y + x / 8;
    x           = (x & 7) * 1;
    mask        = 0x80 >> x;
    pmask       = 1;
    for (p = 0; p < 4; p++) {
        __set_plane(p);
        if (pmask & color)
            __write_byte(off, __read_byte(off) | mask);
        else
            __write_byte(off, __read_byte(off) & ~mask);
        pmask <<= 1;
    }
}

static inline void __write_pixel_8(unsigned int x, unsigned int y, unsigned char color)
{
    __set_plane(x);
    __write_byte(((y * driver->width) + x) / 4, color);
    // (y << 6) + (y << 4) + (x >> 2)
}

unsigned int reverseBits(char num)
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

void vga_setfont(const vga_font_t *__font)
{
    font->font   = __font->font;
    font->width  = __font->width;
    font->height = __font->height;
}

void vga_clear_screen()
{
    for (int p = 0; p < 4; p++) {
        __set_plane(p);
        memset(driver->address, 0, 64 * 1024);
    }
}

void vga_draw_char(unsigned int x, unsigned int y, unsigned char c, unsigned char color)
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
    unsigned char *glyph = font->font + c * font->height;
    for (unsigned cy = 0; cy < font->height; ++cy) {
        for (unsigned cx = 0; cx < font->width; ++cx) {
            driver->ops->write_pixel(x + (font->width - cx), y + cy, glyph[cy] & mask[cx] ? color : 0x00u);
        }
    }
}

void vga_draw_string(int x, int y, char *str, unsigned char color)
{
    char i = 0;
    while (*str != '\0') {
        vga_draw_char(x + i * 8, y, *str, color);
        str++;
        i++;
    }
}

void vga_draw_line(unsigned int x0, unsigned int y0, unsigned int x1, unsigned int y1, unsigned char color)
{
    bool_t steep = abs(y1 - y0) > abs(x1 - x0);
    int tmp      = 0;
    if (steep) {
        tmp = x0;
        x0  = y0;
        y0  = tmp;

        tmp = x1;
        x1  = y1;
        y1  = tmp;
    }
    if (x0 > x1) {
        tmp = x0;
        x0  = x1;
        x1  = tmp;

        tmp = y0;
        y0  = y1;
        y1  = tmp;
    }
    int deltax = x1 - x0;
    int deltay = abs(y1 - y0);
    int error  = deltax / 2;
    int ystep;
    int y = y0;
    if (y0 < y1)
        ystep = 1;
    else
        ystep = -1;
    for (int x = x0; x < x1; x++) {
        if (steep)
            driver->ops->write_pixel(y, x, color);
        else
            driver->ops->write_pixel(x, y, color);
        error = error - deltay;
        if (error < 0) {
            y     = y + ystep;
            error = error + deltax;
        }
    }
}

void vga_draw_rectangle(unsigned int sx, unsigned int sy, unsigned int ex, unsigned int ey, unsigned char fill)
{
    vga_draw_line(sx, sy, ex, sy, fill);
    vga_draw_line(sx, sy, sx, ey, fill);
    vga_draw_line(sx, ey, ex, ey, fill);
    vga_draw_line(ex, ey, ex, sy, fill);
}

void vga_draw_circle(unsigned int xc, unsigned int yc, unsigned int r, unsigned char fill)
{
    unsigned int x = 0;
    unsigned int y = r;
    unsigned int p = 3 - 2 * r;
    if (!r)
        return;

    while (y >= x) // only formulate 1/8 of circle
    {
        driver->ops->write_pixel(xc - x, yc - y, fill); //upper left left
        driver->ops->write_pixel(xc - y, yc - x, fill); //upper upper left
        driver->ops->write_pixel(xc + y, yc - x, fill); //upper upper right
        driver->ops->write_pixel(xc + x, yc - y, fill); //upper right right
        driver->ops->write_pixel(xc - x, yc + y, fill); //lower left left
        driver->ops->write_pixel(xc - y, yc + x, fill); //lower lower left
        driver->ops->write_pixel(xc + y, yc + x, fill); //lower lower right
        driver->ops->write_pixel(xc + x, yc + y, fill); //lower right right
        if (p < 0)
            p += 4 * x++ + 6;
        else
            p += 4 * (x++ - y--) + 10;
    }
}

void vga_draw_triangle(unsigned int x1, unsigned int y1, unsigned int x2, unsigned int y2, unsigned int x3, unsigned int y3, unsigned char fill)
{
    vga_draw_line(x1, y1, x2, y2, fill);
    vga_draw_line(x2, y2, x3, y3, fill);
    vga_draw_line(x3, y3, x1, y1, fill);
}

static void __test_vga(void)
{
    vga_clear_screen();
    vga_draw_rectangle(1, 1, driver->width - 1, driver->height - 1, 3);
    for (unsigned i = 1; i < driver->height - 1; i++) {
        driver->ops->write_pixel((driver->width - driver->height) / 2 + i, i, 1);
        driver->ops->write_pixel((driver->height + driver->width) / 2 - i, i, 1);
    }
}

// == MODEs and DRIVERs =======================================================

static vga_ops_t ops_720_480_16 = {
    .write_pixel = __write_pixel_4,
    .read_pixel  = NULL,
    .draw_rect   = NULL,
    .fill_rect   = NULL,
};

static vga_ops_t ops_640_480_16 = {
    .write_pixel = __write_pixel_4,
    .read_pixel  = NULL,
    .draw_rect   = NULL,
    .fill_rect   = NULL,
};

static vga_ops_t ops_320_200_256 = {
    .write_pixel = __write_pixel_8,
    .read_pixel  = NULL,
    .draw_rect   = NULL,
    .fill_rect   = NULL,
};

static vga_font_t font_8x8 = {
    .font   = arr_8x8_font,
    .width  = 8,
    .height = 8,
};

static vga_font_t font_8x8_basic = {
    .font   = arr_8x8_basic_font,
    .width  = 8,
    .height = 8,
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
    // Initialize the desired mode.
#if defined(VGA_MODE_320_200_256)
    // Save the current palette.
    __save_palette(stored_palette, 256);
    // Write the registers.
    __set_mode(&_mode_320_200_256);
    // Initialize the mode.
    driver = &driver_320_200_256;
    // Load the color palette.
    __load_palette(ansi_256_palette, 256);
#elif defined(VGA_MODE_640_480_16)
    // Save the current palette.
    __save_palette(stored_palette, 256);
    // Write the registers.
    __set_mode(&_mode_640_480_16);
    // Initialize the mode.
    driver = &driver_640_480_16;
    // Load the color palette.
    __load_palette(ansi_16_palette, 16);
#elif defined(VGA_MODE_720_480_16)
    // Save the current palette.
    __save_palette(stored_palette, 256);
    // Write the registers.
    __set_mode(&_mode_720_480_16);
    // Initialize the mode.
    driver = &driver_720_480_16;
    // Load the color palette.
    __load_palette(ansi_16_palette, 16);
#else // VGA_TEXT_MODE
    __set_mode(&_mode_80_25_text);
    return;
#endif
    // Set the address.
    driver->address = __get_seg();

    // Set the font.
    vga_setfont(&font_8x8);

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
