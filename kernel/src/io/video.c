/// @file video.c
/// @brief Generic console: cell state, cursor and escape-sequence handling.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.
///
/// This layer owns all terminal state and knows nothing about hardware. It
/// keeps the screen in a cell buffer, does every cursor, erase and scroll
/// operation in cell coordinates, and pushes the results to whichever backend
/// was compiled in (see io/video_backend.h). Addresses, memory layout, fonts,
/// palettes and I/O ports are the backend's business.
///
/// The buffer is the source of truth, so every mutation is followed by a
/// put_cells() call covering the range that changed. Anything that mutates
/// cells without flushing them will make the display drift out of sync.

// Setup the logging for this file (do this before any other include).
#include "sys/kernel_levels.h"
#define __DEBUG_HEADER__ "[VIDEO ]"      ///< Change header.
#define __DEBUG_LEVEL__  LOGLEVEL_NOTICE ///< Set log level.
#include "io/debug.h"

#include "ctype.h"
#include "io/video.h"
#include "io/video_backend.h"
#include "klib/irqflags.h"
#include "stdbool.h"
#include "stddef.h"
#include "stdio.h"
#include "string.h"

/// The number of screen-sized pages of scrollback history we keep.
#define STORED_PAGES       10

/// @name Boot console dimensions
/// @brief The compile-time geometry, which sizes the static storage below.
///
/// These are the *boot* console's dimensions, not necessarily the console's.
/// Everything past initialization works from the runtime values, so that the
/// console can one day be a different shape than the one it started as. Only the
/// static arrays and the backend geometry check use these.
/// @{
#define BOOT_SCREEN_CELLS  (VIDEO_COLUMNS * VIDEO_ROWS)
#define BOOT_HISTORY_ROWS  (STORED_PAGES * VIDEO_ROWS)
#define BOOT_HISTORY_CELLS (BOOT_HISTORY_ROWS * VIDEO_COLUMNS)
/// @}

/// @brief Stores the association between ANSI colors and pure VIDEO colors.
typedef struct {
    /// The ANSI color number.
    uint8_t ansi_color;
    /// The VIDEO color number.
    uint8_t video_color;
} ansi_color_map_t;

/// @brief The mapping.
ansi_color_map_t ansi_color_map[] = {
    {0, 7},

    {30, 0},
    {31, 4},
    {32, 2},
    {33, 6},
    {34, 1},
    {35, 5},
    {36, 3},
    {37, 7},

    {90, 8},
    {91, 12},
    {92, 10},
    {93, 14},
    {94, 9},
    {95, 13},
    {96, 11},
    {97, 15},

    {40, 0},
    {41, 4},
    {42, 2},
    {43, 6},
    {44, 1},
    {45, 5},
    {46, 3},
    {47, 7},

    {100, 8},
    {101, 12},
    {102, 10},
    {103, 14},
    {104, 9},
    {105, 13},
    {106, 11},
    {107, 15},
};

/// @brief Lookup table for foreground colors (ANSI codes 0-107).
static uint8_t fg_color_map[108] = {0};

/// @brief Lookup table for background colors (ANSI codes 0-107).
static uint8_t bg_color_map[108] = {0};

/// @brief The cell an erase operation leaves behind.
///
/// Erasing zeroes both halves of the cell rather than writing a space in the
/// current colour: character 0 with attribute 0 renders as blank. Erase-to-space
/// (backspace and delete) is a different operation with a different fill; see
/// __erase_to_space().
static const video_cell_t blank_cell = {0, 0};

/// @name The boot console's storage
/// @brief Static, sized at compile time, and used from the very first printf.
///
/// It has to be static: output exists long before there is an allocator, and the
/// pre-video_init() panic path must not depend on one. So the console starts
/// here and only moves to allocated storage if something ever asks it to.
/// @{

/// The visible screen, plus one guard cell.
///
/// The guard cell exists because the cursor is allowed to come to rest one cell
/// past the end of the screen; see __cursor_is_parked().
static video_cell_t boot_screen[BOOT_SCREEN_CELLS + 1];
/// Scrollback history. The most recently scrolled-off line is last.
static video_cell_t boot_history[BOOT_HISTORY_CELLS];
/// The live screen, saved while the user is scrolled back into history.
static video_cell_t boot_original_page[BOOT_SCREEN_CELLS];
/// @}

/// @name The console's live shape and storage
/// @brief What every operation below actually works from.
///
/// Separate from the compile-time macros so the console's dimensions are a
/// runtime property. They start as the boot console's and, for a backend that
/// cannot resize, stay that way for the whole life of the machine -- which is
/// why this costs nothing in the fixed builds.
///
/// The counts are stored rather than recomputed from columns and rows because
/// they appear in inner loops, and because keeping them together with the
/// pointers means the whole shape of the console is one group of values that
/// change together.
/// @{
static unsigned video_columns  = VIDEO_COLUMNS;        ///< Visible width in cells.
static unsigned video_rows     = VIDEO_ROWS;           ///< Visible height in cells.
static unsigned screen_cells   = BOOT_SCREEN_CELLS;    ///< Cells on the visible screen.
static unsigned history_rows   = BOOT_HISTORY_ROWS;    ///< Rows of scrollback.
static unsigned history_cells  = BOOT_HISTORY_CELLS;   ///< Cells of scrollback.

/// The visible screen. Indexed exactly as the array it replaces was.
static video_cell_t *screen        = boot_screen;
/// The scrollback history.
static video_cell_t *history       = boot_history;
/// The saved live screen, used while scrolled back.
static video_cell_t *original_page = boot_original_page;
/// @}

/// @brief Cursor position, as an index into `screen`.
///
/// Valid range is [0, screen_cells] inclusive: the upper end is the guard cell.
static unsigned cursor = 0;

/// @brief Cursor position saved by ESC [ s and restored by ESC [ u.
static unsigned saved_cursor = 0;

/// @brief The current color attribute (foreground and background).
unsigned char color = 7;

/// @brief Index for writing to escape_buffer. If -1, we are not parsing an escape sequence.
int escape_index = -1;

/// @brief Buffer used to store an escape sequence as it's being parsed.
char escape_buffer[256];

/// @brief Indicates if the screen is currently scrolled, and by how many lines.
int scrolled_lines = 0;

/// @brief Flag to batch cursor updates in video_puts to improve performance.
static int batch_cursor_updates = 0;

/// @brief The backend currently materializing the console.
///
/// Starts as the compile-time backend and stays there unless something promotes
/// a different one; see video_promote_backend(). The indirection exists because
/// a handoff needs two materializations alive at once -- one still displaying
/// while the other is built -- which a single compile-time symbol cannot
/// express.
///
/// Read from interrupt context, by video_cursor_blink_tick(). A single aligned
/// pointer store is atomic on this architecture, but the publish still masks
/// interrupts, both to say so explicitly and because that critical section is
/// where more state will be published later.
static const video_backend_t *video_active = &video_backend;

/// @brief Whether the cursor sits outside the visible screen.
/// @return true when the cursor is parked on the guard cell.
///
/// Moving forward off the bottom-right corner leaves the cursor one cell past
/// the screen, and from there some sequences can push it further still. The
/// original implementation kept writing through such a cursor; those writes
/// landed in the unused remainder of the VGA text aperture and were never
/// displayed. Here the cursor is parked on a single guard cell instead and the
/// invisible writes are skipped, which keeps everything observable identical --
/// the reported position, the rendered position and the recovery path -- without
/// ever writing out of bounds.
static inline bool_t __cursor_is_parked(void) { return cursor >= screen_cells; }

/// @brief Moves the cursor, keeping it within the buffer.
/// @param index The desired cell index.
static inline void __set_cursor(unsigned index) { cursor = (index > screen_cells) ? screen_cells : index; }

/// @brief Get the current column number.
/// @return The column number.
static inline unsigned __get_x(void)
{
    if (__cursor_is_parked()) {
        return 0;
    }
    return cursor % video_columns;
}

/// @brief Get the current row number.
/// @return The row number.
static inline unsigned __get_y(void)
{
    if (__cursor_is_parked()) {
        return 0;
    }
    return cursor / video_columns;
}

/// @brief First cell of the row the cursor is on.
/// @return The cell index of the start of the row.
static inline unsigned __row_start(void) { return (cursor / video_columns) * video_columns; }

/// @brief One past the last cell of the row the cursor is on.
/// @return The cell index just past the end of the row.
static inline unsigned __row_end(void) { return __row_start() + video_columns; }

/// @brief Pushes a range of cells to the backend.
/// @param first Index of the first cell.
/// @param count How many cells to push.
///
/// Ranges are clipped to the visible screen, so the guard cell is never sent.
static void __flush(unsigned first, unsigned count)
{
    if (first >= screen_cells) {
        return;
    }
    if (count > (screen_cells - first)) {
        count = screen_cells - first;
    }
    if (count == 0) {
        return;
    }
    video_active->put_cells(first % video_columns, first / video_columns, &screen[first], count);
}

/// @brief Pushes the whole screen to the backend.
static void __flush_all(void) { __flush(0, screen_cells); }

/// @brief Erases a range of cells.
/// @param first Index of the first cell.
/// @param count How many cells to erase.
static void __erase(unsigned first, unsigned count)
{
    if (first >= screen_cells) {
        return;
    }
    if (count > (screen_cells - first)) {
        count = screen_cells - first;
    }
    for (unsigned index = 0; index < count; ++index) {
        screen[first + index] = blank_cell;
    }
    __flush(first, count);
}

/// @brief Draws the given character at the current cursor position.
/// @param c The character to draw.
static inline void __draw_char(char c)
{
    // If we are scrolled, unscroll first to show current content.
    if (scrolled_lines) {
        video_scroll_up(scrolled_lines);
    }

    unsigned row_end = __row_end();

    if (!__cursor_is_parked()) {
        // Writing inserts rather than overwrites: the rest of the line shifts
        // right and its last cell falls off the end.
        if (cursor < (row_end - 1)) {
            memmove(&screen[cursor + 1], &screen[cursor], ((row_end - 1) - cursor) * sizeof(video_cell_t));
        }
        screen[cursor].character = (uint8_t)c;
        screen[cursor].attribute = color;
        // The shift dirtied everything from the cursor to the end of the line.
        __flush(cursor, row_end - cursor);
    }

    // Advance, wrapping at the end of the line.
    __set_cursor(cursor + 1);
    if (cursor >= row_end) {
        __set_cursor(row_end);
    }

    // If the cursor went past the end of the screen, scroll and sit on the last
    // line.
    if (cursor >= screen_cells) {
        video_shift_one_line_up();
        __set_cursor(screen_cells - video_columns);
    }
}

/// @brief Erases a character by shifting the rest of the line left.
///
/// Unlike __erase(), the cell freed at the end of the line is filled with a
/// space in the *current* colour rather than being zeroed.
static inline void __erase_to_space(void)
{
    if (__cursor_is_parked()) {
        return;
    }
    unsigned row_end     = __row_end();
    unsigned cells_after = row_end - (cursor + 1);
    if (cells_after > 0) {
        memmove(&screen[cursor], &screen[cursor + 1], cells_after * sizeof(video_cell_t));
    }
    screen[row_end - 1].character = ' ';
    screen[row_end - 1].attribute = color;
    __flush(cursor, row_end - cursor);
}

/// @brief Sets the provided ANSI color code.
/// @param ansi_code The ANSI code describing background and foreground color.
static inline void __set_color(uint8_t ansi_code)
{
    if (ansi_code == 0) {
        // Reset to default colors (white on black).
        color = 0x07;
    } else if (ansi_code == 1) {
        // Bold/bright - make foreground color bright by setting intensity bit.
        color = color | 0x08;
    } else if (ansi_code == 7) {
        // Reverse video - swap foreground and background.
        uint8_t fg = color & 0x0F;
        uint8_t bg = (color & 0xF0) >> 4;
        color      = (fg << 4) | bg;
    } else if (ansi_code == 22) {
        // Normal intensity - remove bright bit from foreground.
        color = color & ~0x08;
    } else if (ansi_code == 27) {
        // Reverse video off - swap back (same as reverse).
        uint8_t fg = color & 0x0F;
        uint8_t bg = (color & 0xF0) >> 4;
        color      = (fg << 4) | bg;
    } else if (ansi_code == 39) {
        // Default foreground color (white).
        color = (color & 0xF0U) | 0x07;
    } else if (ansi_code == 49) {
        // Default background color (black).
        color = (color & 0x0FU);
    } else if (ansi_code <= 107) {
        if ((ansi_code >= 30) && (ansi_code <= 37)) {
            // Normal foreground colors (30-37).
            color = (color & 0xF0U) | fg_color_map[ansi_code];
        } else if ((ansi_code >= 90) && (ansi_code <= 97)) {
            // Bright foreground colors (90-97).
            color = (color & 0xF0U) | fg_color_map[ansi_code];
        } else if ((ansi_code >= 40) && (ansi_code <= 47)) {
            // Normal background colors (40-47).
            color = (color & 0x0FU) | (bg_color_map[ansi_code] << 4U);
        } else if ((ansi_code >= 100) && (ansi_code <= 107)) {
            // Bright background colors (100-107).
            color = (color & 0x0FU) | (bg_color_map[ansi_code] << 4U);
        }
    }
}

/// @brief Moves the cursor backward by the specified amount.
/// @param erase If 1, also erase the character (backspace behavior).
/// @param amount How many times we move backward.
static inline void __move_cursor_backward(int erase, int amount)
{
    for (int i = 0; i < amount; ++i) {
        if (cursor >= 1) {
            // Move back one character position.
            __set_cursor(cursor - 1);
            if (erase) {
                __erase_to_space();
            }
        } else {
            break;
        }
    }
    video_update_cursor_position();
}

/// @brief Moves the cursor forward by the specified amount.
/// @param erase If 1, also erase the character (overwrite with space).
/// @param amount How many times we move forward.
static inline void __move_cursor_forward(int erase, int amount)
{
    for (int i = 0; i < amount; ++i) {
        if ((cursor + 1) <= screen_cells) {
            if (erase) {
                // Overwrite with space without shifting other characters.
                screen[cursor].character = ' ';
                screen[cursor].attribute = color;
                __flush(cursor, 1);
            }
            // Move forward one character position.
            __set_cursor(cursor + 1);
        } else {
            break;
        }
    }
    video_update_cursor_position();
}

/// @brief Parses the cursor shape escape code and sets the cursor shape accordingly.
/// @param shape The integer representing the cursor shape code.
static inline void __parse_cursor_escape_code(int shape)
{
    video_cursor_style_t style;

    switch (shape) {
    case 0: // Default: blinking block.
        style.shape = VIDEO_CURSOR_BLOCK, style.blinking = true;
        break;
    case 1: // Blinking block.
        style.shape = VIDEO_CURSOR_BLOCK, style.blinking = true;
        break;
    case 2: // Steady block.
        style.shape = VIDEO_CURSOR_BLOCK, style.blinking = false;
        break;
    case 3: // Blinking underline.
        style.shape = VIDEO_CURSOR_UNDERLINE, style.blinking = true;
        break;
    case 4: // Steady underline.
        style.shape = VIDEO_CURSOR_UNDERLINE, style.blinking = false;
        break;
    case 5: // Blinking bar.
        style.shape = VIDEO_CURSOR_BAR, style.blinking = true;
        break;
    case 6: // Steady bar.
        style.shape = VIDEO_CURSOR_BAR, style.blinking = false;
        break;
    default:
        // Anything else is ignored, as it always has been.
        return;
    }
    video_active->set_cursor_style(style);
}

void video_init(void)
{
    // Initialize color lookup tables from the ANSI color mapping.
    for (size_t i = 0; i < count_of(ansi_color_map); ++i) {
        uint8_t code = ansi_color_map[i].ansi_color;
        uint8_t vid  = ansi_color_map[i].video_color;
        if (code <= 107) {
            // Populate foreground color map for codes 0, 30-37, 90-97.
            if ((code == 0) || ((code >= 30) && (code <= 37)) || ((code >= 90) && (code <= 97))) {
                fg_color_map[code] = vid;
            } else {
                // Populate background color map for codes 40-47, 100-107.
                bg_color_map[code] = vid;
            }
        }
    }

    // Bring the hardware up before pushing anything to it. Until this returns,
    // a backend that cannot use its hardware yet has been ignoring us.
    if (video_backend.init() < 0) {
        pr_emerg("Failed to initialize the '%s' video backend.\n", video_backend.name);
    }

    // At this point the console's geometry is still the compile-time one, and
    // the backend's comes from the same macros, so a mismatch means the build is
    // inconsistent and every offset computed here would be wrong.
    if ((video_backend.columns != video_columns) || (video_backend.rows != video_rows)) {
        pr_emerg(
            "Video backend '%s' reports %ux%u but the console was built for %ux%u.\n", video_backend.name,
            video_backend.columns, video_backend.rows, (unsigned)video_columns, (unsigned)video_rows);
    }

    // Clearing the screen is what publishes the initialized console. Anything
    // printed before this point is discarded, which is what has always
    // happened.
    video_clear();

    // Use the default blinking block cursor.
    __parse_cursor_escape_code(0);
}

/// @brief Puts the whole console on the display and places the cursor.
///
/// Used whenever a backend starts materializing a console it has not been
/// drawing. The cell buffer is the source of truth and has been recording all
/// along, so one flush of the whole screen is all it takes to make the display
/// agree with it.
///
/// The cursor is placed afterwards rather than left to chance: the overlay
/// belongs to the backend, a backend that has just taken over has none drawn,
/// and going through the ordinary cursor path is what keeps the lifecycle
/// invariant true from the very first overlay.
static void __republish(void)
{
    __flush_all();
    video_update_cursor_position();
}

/// @brief Runs a backend's deferred initialization and makes it the active one.
/// @param next The backend to bring up and publish.
/// @return 0 on success, a negative value on failure.
///
/// Ordered so that failure is harmless: the deferred initialization runs
/// first, and `video_active` is only moved once it has succeeded. A backend that
/// fails half way through bringing up its hardware therefore leaves whichever
/// backend was displaying still displaying.
static int __publish_backend(const video_backend_t *next)
{
    if ((next->late_init != NULL) && (next->late_init() < 0)) {
        pr_emerg("Failed to complete late initialization of the '%s' video backend.\n", next->name);
        return -1;
    }

    // Interrupts are masked for the publish alone. The pointer is read from the
    // timer interrupt, and although a single aligned store is atomic here, this
    // is the critical section that later has more state to publish at once, so
    // it is written as a critical section from the start. Nothing expensive may
    // move inside it.
    uint8_t flags = irq_disable();
    video_active  = next;
    irq_enable(flags);

    __republish();
    return 0;
}

void video_late_init(void)
{
    // Most backends have nothing deferred; there is then nothing to repaint
    // either, because they have been drawing all along.
    if (video_backend.late_init == NULL) {
        return;
    }
    if (__publish_backend(&video_backend) < 0) {
        return;
    }
    pr_notice("Late initialization of the '%s' video backend complete.\n", video_backend.name);
}

int video_promote_backend(const video_backend_t *next)
{
    if (next == NULL) {
        pr_emerg("Cannot promote a null video backend.\n");
        return -1;
    }
    if (next == video_active) {
        return 0;
    }

    // A backend may only take over a console of the shape it can materialize.
    // Compared against the console's current geometry rather than the
    // compile-time one, so this stays correct once the console can be resized.
    if ((next->columns != video_columns) || (next->rows != video_rows)) {
        pr_emerg(
            "Cannot promote '%s': it reports %ux%u but the console is %ux%u.\n", next->name, next->columns, next->rows,
            (unsigned)video_columns, (unsigned)video_rows);
        return -1;
    }

    const char *previous = video_active->name;
    if (__publish_backend(next) < 0) {
        pr_emerg("Promotion to '%s' failed; '%s' is still displaying.\n", next->name, previous);
        return -1;
    }

    pr_notice("Video backend promoted from '%s' to '%s'.\n", previous, next->name);
    return 0;
}

void video_putc(int c)
{
    // Handle ANSI escape sequence start.
    if (c == '\033') {
        escape_index = 0;
        memset(escape_buffer, 0, sizeof(escape_buffer));
        return;
    }

    // Process escape sequence characters.
    if (escape_index >= 0) {
        // Handle special single-character escape sequences (not CSI).
        if (escape_index == 0) {
            // ESC c - RIS (Reset to Initial State) - Full terminal reset.
            if (c == 'c') {
                // Clear the screen.
                video_clear();
                // Reset to default colors.
                color = 0x07;
                // Reset cursor shape.
                __parse_cursor_escape_code(0);
                // Reset scrollback buffer.
                escape_index = -1;
                return;
            }
            // ESC [ - Start CSI sequence.
            else if (c == '[') {
                escape_index = 1;
                return;
            }
            // Unknown escape sequence, abort.
            else {
                escape_index = -1;
                return;
            }
        }
        // Check for buffer overflow.
        if (escape_index >= sizeof(escape_buffer) - 1) {
            escape_index = -1;
            return;
        }
        // Store character in escape buffer.
        escape_buffer[escape_index++] = c;
        escape_buffer[escape_index]   = 0;

        // Process escape sequence when we hit a letter (command character).
        if (isalpha(c)) {
            // Remove the command character from the buffer.
            if (escape_index > 1) {
                escape_buffer[--escape_index] = 0;
            } else {
                escape_index = -1;
            }

            // ESC [ <n> C - Cursor forward.
            if (c == 'C') {
                int amount = atoi(&escape_buffer[1]);
                if (amount <= 0)
                    amount = 1;
                __move_cursor_forward(false, amount);
            }
            // ESC [ <n> D - Cursor backward.
            else if (c == 'D') {
                int amount = atoi(&escape_buffer[1]);
                if (amount <= 0)
                    amount = 1;
                __move_cursor_backward(false, amount);
            }
            // ESC [ <n> A - Cursor up.
            else if (c == 'A') {
                int amount = atoi(&escape_buffer[1]);
                if (amount <= 0)
                    amount = 1;
                for (int i = 0; i < amount; ++i) {
                    if (__get_y() > 0) {
                        __set_cursor(cursor - video_columns);
                    }
                }
                video_update_cursor_position();
            }
            // ESC [ <n> B - Cursor down.
            else if (c == 'B') {
                int amount = atoi(&escape_buffer[1]);
                if (amount <= 0)
                    amount = 1;
                for (int i = 0; i < amount; ++i) {
                    if (__get_y() < (video_rows - 1)) {
                        __set_cursor(cursor + video_columns);
                    }
                }
                video_update_cursor_position();
            }
            // ESC [ <n> m or ESC [ <n>;<n> m - Set color/attributes.
            else if (c == 'm') {
                // Note: escape_buffer data starts at index 1 (we skip '[' at index 0).
                char *token = &escape_buffer[1];
                char *saveptr;

                // Empty sequence means reset (ESC [ m is same as ESC [ 0 m).
                if (escape_buffer[1] == '\0') {
                    __set_color(0);
                } else {
                    // Parse each semicolon-separated parameter.
                    while ((token = strtok_r(token, ";", &saveptr)) != NULL) {
                        int code = atoi(token);
                        __set_color(code);
                        token = NULL;
                    }
                }
            }
            // ESC [ <n> J - Clear screen.
            else if (c == 'J') {
                int mode = atoi(&escape_buffer[1]);
                if (mode == 0) {
                    // Clear from cursor to end of screen. A parked cursor is
                    // already past the end, so this erases nothing.
                    __erase(cursor, screen_cells - cursor);
                } else if (mode == 1) {
                    // Clear from start of screen to cursor (inclusive).
                    __erase(0, cursor + 1);
                } else if (mode == 3) {
                    // Clear entire screen AND scrollback buffer.
                    video_clear();
                } else {
                    // Mode 2 or default: Clear entire screen (but preserve scrollback).
                    for (unsigned index = 0; index < screen_cells; ++index) {
                        screen[index] = blank_cell;
                    }
                    __set_cursor(0);
                    scrolled_lines = 0;
                    __flush_all();
                    video_update_cursor_position();
                }
            }
            // ESC [ <row>;<col> H or f - Set cursor position.
            else if ((c == 'H') || (c == 'f')) {
                char *semicolon = strchr(&escape_buffer[1], ';');
                if (semicolon != NULL) {
                    *semicolon              = '\0';
                    // A zero or absent parameter underflows here and is then
                    // caught by the clamp below, which lands the cursor on the
                    // last cell rather than at home.
                    unsigned int target_row = atoi(&escape_buffer[1]) - 1;
                    unsigned int target_col = atoi(semicolon + 1) - 1;
                    if (target_col >= video_columns)
                        target_col = video_columns - 1;
                    if (target_row >= video_rows)
                        target_row = video_rows - 1;
                    __set_cursor((target_row * video_columns) + target_col);
                } else {
                    __set_cursor(0);
                }
                video_update_cursor_position();
            }
            // ESC [ <n> q - Set cursor shape.
            else if (c == 'q') {
                __parse_cursor_escape_code(atoi(&escape_buffer[1]));
            }
            // ESC [ <n> K - Erase in line.
            else if (c == 'K') {
                int mode = atoi(&escape_buffer[1]);
                // A parked cursor sits on no row, so there is nothing visible
                // to erase.
                if (!__cursor_is_parked()) {
                    unsigned row_start = __row_start();
                    unsigned row_end   = __row_end();
                    if (mode == 0) {
                        // Clear from cursor to end of line.
                        __erase(cursor, row_end - cursor);
                    } else if (mode == 1) {
                        // Clear from start of line to cursor.
                        __erase(row_start, (cursor - row_start) + 1);
                    } else if (mode == 2) {
                        // Clear entire line.
                        __erase(row_start, video_columns);
                    }
                }
            }
            // ESC [ s - Save cursor position.
            else if (c == 's') {
                saved_cursor = cursor;
            }
            // ESC [ u - Restore cursor position.
            else if (c == 'u') {
                // Validate saved position is within the visible screen.
                if (saved_cursor < screen_cells) {
                    __set_cursor(saved_cursor);
                }
                video_update_cursor_position();
            }
            // ESC [ <n> S - Custom: scroll down (show older lines).
            else if (c == 'S') {
                int lines_to_scroll = atoi(&escape_buffer[1]);
                if (lines_to_scroll < 0)
                    lines_to_scroll = 0;
                video_scroll_down(lines_to_scroll);
                escape_index = -1;
                return;
            }
            // ESC [ <n> T - Custom: scroll up (show newer lines).
            else if (c == 'T') {
                int lines_to_scroll = atoi(&escape_buffer[1]);
                if (lines_to_scroll < 0)
                    lines_to_scroll = 0;
                video_scroll_up(lines_to_scroll);
                escape_index = -1;
                return;
            }
            escape_index = -1;
        }
        return;
    }

    // Handle normal characters (not in escape sequence).
    if (c == '\n') {
        video_new_line();
    } else if (c == '\b') {
        __move_cursor_backward(true, 1);
    } else if (c == '\r') {
        video_cartridge_return();
    } else if (c == 127) {
        // DEL key - delete character at cursor position.
        __erase_to_space();

        // Update cursor position to reflect the deletion.
        if (!batch_cursor_updates) {
            video_update_cursor_position();
        }
        return;
    } else if ((c >= 0x20) && (c <= 0x7E)) {
        // Printable ASCII character.
        __draw_char(c);
    } else {
        // Ignore other control characters.
        return;
    }

    // Update cursor position unless we're batching updates.
    if (!batch_cursor_updates) {
        video_update_cursor_position();
    }
}

void video_puts(const char *str)
{
    // Validate input string.
    if (!str) {
        return;
    }
    // Batch cursor updates for efficiency.
    batch_cursor_updates = 1;
    // Output each character in the string.
    while ((*str) != 0) {
        video_putc((*str++));
    }
    // Re-enable cursor updates and sync position.
    batch_cursor_updates = 0;
    video_update_cursor_position();
}

void video_update_cursor_position(void)
{
    // Make sure there is something at the cursor for it to sit on: an empty
    // cell would leave a hardware cursor with nothing to render over.
    if (screen[cursor].character == 0) {
        screen[cursor].character = ' ';
        screen[cursor].attribute = color;
        __flush(cursor, 1);
    }

    unsigned column = cursor % video_columns;
    unsigned row    = cursor / video_columns;
    if (column >= video_columns) {
        column = video_columns - 1;
    }
    if (row >= video_rows) {
        row = video_rows - 1;
    }

    // Hand over the cell the cursor now sits on. Erasing whatever a backend drew
    // at the previous position is the backend's own business: it is the only
    // party that knows whether it drew anything, and scrolling moves its overlay
    // under it, so the generic layer cannot track where the pixels ended up.
    // Note the cell comes from the buffer, which never contains cursor pixels.
    video_active->set_cursor_position(column, row, screen[cursor]);
}

void video_cursor_blink_tick(void)
{
    // Backends with a hardware cursor leave this NULL: there is nothing for the
    // console to drive.
    if (video_active->cursor_blink != NULL) {
        video_active->cursor_blink();
    }
}

void video_move_cursor(unsigned int x, unsigned int y)
{
    // Clamp coordinates to screen bounds.
    if (x >= video_columns)
        x = video_columns - 1;
    if (y >= video_rows)
        y = video_rows - 1;
    __set_cursor((y * video_columns) + x);
    // Update hardware cursor to match.
    video_update_cursor_position();
}

void video_get_cursor_position(unsigned int *x, unsigned int *y)
{
    // Populate x coordinate if requested.
    if (x) {
        *x = __get_x();
    }
    // Populate y coordinate if requested.
    if (y) {
        *y = __get_y();
    }
}

void video_get_screen_size(unsigned int *width, unsigned int *height)
{
    // Return screen width if requested.
    if (width) {
        *width = video_columns;
    }
    // Return screen height if requested.
    if (height) {
        *height = video_rows;
    }
}

void video_clear(void)
{
    // Clear the scrollback buffer.
    memset(history, 0, history_cells * sizeof(video_cell_t));
    // Clear the visible screen.
    for (unsigned index = 0; index < screen_cells; ++index) {
        screen[index] = blank_cell;
    }
    // Reset cursor to top-left corner.
    __set_cursor(0);
    // Reset scrolling state.
    scrolled_lines = 0;
    __flush_all();
    video_update_cursor_position();
}

void video_new_line(void)
{
    // If we're viewing scrollback, unscroll first to show current content.
    if (scrolled_lines) {
        video_scroll_up(scrolled_lines);
    }

    // Move to the start of the next line.
    __set_cursor(__row_end());
    // Check if we've gone past the bottom of the screen.
    if (cursor >= screen_cells) {
        // Scroll up by one line and move content into scrollback.
        video_shift_one_line_up();
        // Position cursor at the beginning of the last line.
        __set_cursor(screen_cells - video_columns);
    }
    video_update_cursor_position();
}

void video_cartridge_return(void)
{
    // If we're viewing scrollback, unscroll first to show current content.
    if (scrolled_lines) {
        video_scroll_up(scrolled_lines);
    }

    // Move to the beginning of the current line.
    __set_cursor(__row_start());
    video_update_cursor_position();
}

/// @brief Shifts a cell buffer up or down by one line.
/// @param buffer Pointer to the buffer to shift.
/// @param rows Number of rows in the buffer.
/// @param direction 1 to shift up, -1 to shift down.
static inline void __shift_buffer(video_cell_t *buffer, unsigned rows, int direction)
{
    // Shift up: Move all lines in one operation.
    if (direction == 1) {
        // Move (rows-1) lines from row 1 to row 0
        memmove(buffer, buffer + video_columns, video_columns * (rows - 1) * sizeof(video_cell_t));
    }
    // Shift down: Move all lines in one operation.
    else if (direction == -1) {
        // Move (rows-1) lines from row 0 to row 1
        memmove(buffer + video_columns, buffer, video_columns * (rows - 1) * sizeof(video_cell_t));
    }
}

/// @brief Shifts the screen content up by one line. When not scrolled, moves
/// the top line into the `history` buffer.
static void __shift_screen_up(void)
{
    if (scrolled_lines == 0) {
        // Move the history buffer up by one line.
        __shift_buffer(history, history_rows, +1);
        // Copy the first line on the screen into the last line of the history.
        memcpy(&history[history_cells - video_columns], screen, video_columns * sizeof(video_cell_t));
    }
    // Move the screen up by one line.
    __shift_buffer(screen, video_rows, +1);
    // Clear the last line of the screen.
    for (unsigned index = screen_cells - video_columns; index < screen_cells; ++index) {
        screen[index] = blank_cell;
    }
    // Let the backend move the pixels it already has, then repaint the line
    // that was uncovered.
    video_active->scroll(1);
    __flush(screen_cells - video_columns, video_columns);
}

/// @brief Shifts the screen content down by one line. Restores the topmost line
/// from the `history` buffer.
static void __shift_screen_down(void)
{
    // Move the screen content down by one line.
    __shift_buffer(screen, video_rows, -1);
    // Restore from the history buffer.
    memcpy(screen, &history[history_cells - ((unsigned)scrolled_lines * video_columns)],
           video_columns * sizeof(video_cell_t));
    video_active->scroll(-1);
    __flush(0, video_columns);
}

void video_shift_one_line_up(void)
{
    // Handle case where cursor is beyond screen (during scrolling operations).
    if (cursor >= screen_cells) {
        // Shift the screen and scrollback buffer up.
        __shift_screen_up();
        // Adjust cursor to stay on the last line.
        __set_cursor(((cursor / video_columns) - 1) * video_columns);
    }
    // Handle case where we're viewing scrollback history and want to scroll to newer content.
    else if (scrolled_lines > 0) {
        // Shift screen up, moving top line into scrollback.
        __shift_screen_up();
        // Restore the bottom line from the original (unscrolled) screen content.
        memcpy(&screen[screen_cells - video_columns],
               &original_page[screen_cells - ((unsigned)scrolled_lines * video_columns)],
               video_columns * sizeof(video_cell_t));
        __flush(screen_cells - video_columns, video_columns);
        // We're now one line less scrolled back.
        --scrolled_lines;
    }
    // When scrolled_lines == 0, we're at the live view. Don't scroll further forward.
    // This prevents scrolling past the bottom of actual content.
}

void video_shift_one_line_down(void)
{
    // Check if we haven't scrolled beyond our scrollback buffer limit.
    if (scrolled_lines < (int)history_rows) {
        // Save the current visible screen before first scroll operation.
        if (scrolled_lines == 0) {
            memcpy(original_page, screen, screen_cells * sizeof(video_cell_t));
        }
        // We're now one line deeper into scrollback history.
        ++scrolled_lines;
        // Shift screen content down, making room at the top.
        __shift_screen_down();
        // Restore the top line from the scrollback buffer.
        memcpy(screen, &history[history_cells - ((unsigned)scrolled_lines * video_columns)],
               video_columns * sizeof(video_cell_t));
        __flush(0, video_columns);
    }
}

void video_shift_one_page_up(void)
{
    // Scroll up by one full page (video_rows lines).
    for (unsigned i = 0; i < video_rows; ++i) {
        video_shift_one_line_up();
    }
}

void video_shift_one_page_down(void)
{
    // Scroll down by one full page (video_rows lines).
    for (unsigned i = 0; i < video_rows; ++i) {
        video_shift_one_line_down();
    }
}

void video_scroll_up(int lines)
{
    // Validate input: clamp to reasonable bounds.
    if (lines < 0) {
        lines = 0;
    }
    if (lines > scrolled_lines) {
        lines = scrolled_lines;
    }
    // Scroll up by the specified number of lines.
    for (int i = 0; i < lines; ++i) {
        video_shift_one_line_up();
    }
}

void video_scroll_down(int lines)
{
    // Validate input: clamp to reasonable bounds.
    if (lines < 0) {
        lines = 0;
    }
    if (lines > (int)history_rows) {
        lines = (int)history_rows;
    }
    // Scroll down by the specified number of lines.
    for (int i = 0; i < lines; ++i) {
        video_shift_one_line_down();
    }
}
