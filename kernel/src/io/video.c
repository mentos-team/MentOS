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
#include "stdbool.h"
#include "stddef.h"
#include "stdio.h"
#include "string.h"

/// The number of screen-sized pages of scrollback history we keep.
#define STORED_PAGES  10

/// Total number of cells on the visible screen.
#define SCREEN_CELLS  (VIDEO_COLUMNS * VIDEO_ROWS)
/// Total number of rows in the scrollback history.
#define HISTORY_ROWS  (STORED_PAGES * VIDEO_ROWS)
/// Total number of cells in the scrollback history.
#define HISTORY_CELLS (HISTORY_ROWS * VIDEO_COLUMNS)

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

/// @brief The visible screen, plus one guard cell.
///
/// The guard cell exists because the cursor is allowed to come to rest one cell
/// past the end of the screen; see __cursor_is_parked().
static video_cell_t screen[SCREEN_CELLS + 1];

/// @brief Scrollback history. The most recently scrolled-off line is last.
static video_cell_t history[HISTORY_CELLS];

/// @brief The live screen, saved while the user is scrolled back into history.
static video_cell_t original_page[SCREEN_CELLS];

/// @brief Cursor position, as an index into `screen`.
///
/// Valid range is [0, SCREEN_CELLS] inclusive: the upper end is the guard cell.
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
static inline bool_t __cursor_is_parked(void) { return cursor >= SCREEN_CELLS; }

/// @brief Moves the cursor, keeping it within the buffer.
/// @param index The desired cell index.
static inline void __set_cursor(unsigned index) { cursor = (index > SCREEN_CELLS) ? SCREEN_CELLS : index; }

/// @brief Get the current column number.
/// @return The column number.
static inline unsigned __get_x(void)
{
    if (__cursor_is_parked()) {
        return 0;
    }
    return cursor % VIDEO_COLUMNS;
}

/// @brief Get the current row number.
/// @return The row number.
static inline unsigned __get_y(void)
{
    if (__cursor_is_parked()) {
        return 0;
    }
    return cursor / VIDEO_COLUMNS;
}

/// @brief First cell of the row the cursor is on.
/// @return The cell index of the start of the row.
static inline unsigned __row_start(void) { return (cursor / VIDEO_COLUMNS) * VIDEO_COLUMNS; }

/// @brief One past the last cell of the row the cursor is on.
/// @return The cell index just past the end of the row.
static inline unsigned __row_end(void) { return __row_start() + VIDEO_COLUMNS; }

/// @brief Pushes a range of cells to the backend.
/// @param first Index of the first cell.
/// @param count How many cells to push.
///
/// Ranges are clipped to the visible screen, so the guard cell is never sent.
static void __flush(unsigned first, unsigned count)
{
    if (first >= SCREEN_CELLS) {
        return;
    }
    if (count > (SCREEN_CELLS - first)) {
        count = SCREEN_CELLS - first;
    }
    if (count == 0) {
        return;
    }
    video_backend.put_cells(first % VIDEO_COLUMNS, first / VIDEO_COLUMNS, &screen[first], count);
}

/// @brief Pushes the whole screen to the backend.
static void __flush_all(void) { __flush(0, SCREEN_CELLS); }

/// @brief Erases a range of cells.
/// @param first Index of the first cell.
/// @param count How many cells to erase.
static void __erase(unsigned first, unsigned count)
{
    if (first >= SCREEN_CELLS) {
        return;
    }
    if (count > (SCREEN_CELLS - first)) {
        count = SCREEN_CELLS - first;
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
    if (cursor >= SCREEN_CELLS) {
        video_shift_one_line_up();
        __set_cursor(SCREEN_CELLS - VIDEO_COLUMNS);
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
        if ((cursor + 1) <= SCREEN_CELLS) {
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
    video_backend.set_cursor_style(style);
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

    // The geometry the backend reports and the geometry the console compiled
    // against come from the same macros, so a mismatch means the build is
    // inconsistent and every offset computed here would be wrong.
    if ((video_backend.columns != VIDEO_COLUMNS) || (video_backend.rows != VIDEO_ROWS)) {
        pr_emerg(
            "Video backend '%s' reports %ux%u but the console was built for %ux%u.\n", video_backend.name,
            video_backend.columns, video_backend.rows, (unsigned)VIDEO_COLUMNS, (unsigned)VIDEO_ROWS);
    }

    // Clearing the screen is what publishes the initialized console. Anything
    // printed before this point is discarded, which is what has always
    // happened.
    video_clear();

    // Use the default blinking block cursor.
    __parse_cursor_escape_code(0);
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
                        __set_cursor(cursor - VIDEO_COLUMNS);
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
                    if (__get_y() < (VIDEO_ROWS - 1)) {
                        __set_cursor(cursor + VIDEO_COLUMNS);
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
                    __erase(cursor, SCREEN_CELLS - cursor);
                } else if (mode == 1) {
                    // Clear from start of screen to cursor (inclusive).
                    __erase(0, cursor + 1);
                } else if (mode == 3) {
                    // Clear entire screen AND scrollback buffer.
                    video_clear();
                } else {
                    // Mode 2 or default: Clear entire screen (but preserve scrollback).
                    for (unsigned index = 0; index < SCREEN_CELLS; ++index) {
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
                    if (target_col >= VIDEO_COLUMNS)
                        target_col = VIDEO_COLUMNS - 1;
                    if (target_row >= VIDEO_ROWS)
                        target_row = VIDEO_ROWS - 1;
                    __set_cursor((target_row * VIDEO_COLUMNS) + target_col);
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
                        __erase(row_start, VIDEO_COLUMNS);
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
                if (saved_cursor < SCREEN_CELLS) {
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

    unsigned column = cursor % VIDEO_COLUMNS;
    unsigned row    = cursor / VIDEO_COLUMNS;
    if (column >= VIDEO_COLUMNS) {
        column = VIDEO_COLUMNS - 1;
    }
    if (row >= VIDEO_ROWS) {
        row = VIDEO_ROWS - 1;
    }

    // Hand over the cell the cursor now sits on. Erasing whatever a backend drew
    // at the previous position is the backend's own business: it is the only
    // party that knows whether it drew anything, and scrolling moves its overlay
    // under it, so the generic layer cannot track where the pixels ended up.
    // Note the cell comes from the buffer, which never contains cursor pixels.
    video_backend.set_cursor_position(column, row, screen[cursor]);
}

void video_cursor_blink_tick(void)
{
    // Backends with a hardware cursor leave this NULL: there is nothing for the
    // console to drive.
    if (video_backend.cursor_blink != NULL) {
        video_backend.cursor_blink();
    }
}

void video_move_cursor(unsigned int x, unsigned int y)
{
    // Clamp coordinates to screen bounds.
    if (x >= VIDEO_COLUMNS)
        x = VIDEO_COLUMNS - 1;
    if (y >= VIDEO_ROWS)
        y = VIDEO_ROWS - 1;
    __set_cursor((y * VIDEO_COLUMNS) + x);
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
        *width = VIDEO_COLUMNS;
    }
    // Return screen height if requested.
    if (height) {
        *height = VIDEO_ROWS;
    }
}

void video_clear(void)
{
    // Clear the scrollback buffer.
    memset(history, 0, sizeof(history));
    // Clear the visible screen.
    for (unsigned index = 0; index < SCREEN_CELLS; ++index) {
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
    if (cursor >= SCREEN_CELLS) {
        // Scroll up by one line and move content into scrollback.
        video_shift_one_line_up();
        // Position cursor at the beginning of the last line.
        __set_cursor(SCREEN_CELLS - VIDEO_COLUMNS);
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
        memmove(buffer, buffer + VIDEO_COLUMNS, VIDEO_COLUMNS * (rows - 1) * sizeof(video_cell_t));
    }
    // Shift down: Move all lines in one operation.
    else if (direction == -1) {
        // Move (rows-1) lines from row 0 to row 1
        memmove(buffer + VIDEO_COLUMNS, buffer, VIDEO_COLUMNS * (rows - 1) * sizeof(video_cell_t));
    }
}

/// @brief Shifts the screen content up by one line. When not scrolled, moves
/// the top line into the `history` buffer.
static void __shift_screen_up(void)
{
    if (scrolled_lines == 0) {
        // Move the history buffer up by one line.
        __shift_buffer(history, HISTORY_ROWS, +1);
        // Copy the first line on the screen into the last line of the history.
        memcpy(&history[HISTORY_CELLS - VIDEO_COLUMNS], screen, VIDEO_COLUMNS * sizeof(video_cell_t));
    }
    // Move the screen up by one line.
    __shift_buffer(screen, VIDEO_ROWS, +1);
    // Clear the last line of the screen.
    for (unsigned index = SCREEN_CELLS - VIDEO_COLUMNS; index < SCREEN_CELLS; ++index) {
        screen[index] = blank_cell;
    }
    // Let the backend move the pixels it already has, then repaint the line
    // that was uncovered.
    video_backend.scroll(1);
    __flush(SCREEN_CELLS - VIDEO_COLUMNS, VIDEO_COLUMNS);
}

/// @brief Shifts the screen content down by one line. Restores the topmost line
/// from the `history` buffer.
static void __shift_screen_down(void)
{
    // Move the screen content down by one line.
    __shift_buffer(screen, VIDEO_ROWS, -1);
    // Restore from the history buffer.
    memcpy(screen, &history[HISTORY_CELLS - ((unsigned)scrolled_lines * VIDEO_COLUMNS)],
           VIDEO_COLUMNS * sizeof(video_cell_t));
    video_backend.scroll(-1);
    __flush(0, VIDEO_COLUMNS);
}

void video_shift_one_line_up(void)
{
    // Handle case where cursor is beyond screen (during scrolling operations).
    if (cursor >= SCREEN_CELLS) {
        // Shift the screen and scrollback buffer up.
        __shift_screen_up();
        // Adjust cursor to stay on the last line.
        __set_cursor(((cursor / VIDEO_COLUMNS) - 1) * VIDEO_COLUMNS);
    }
    // Handle case where we're viewing scrollback history and want to scroll to newer content.
    else if (scrolled_lines > 0) {
        // Shift screen up, moving top line into scrollback.
        __shift_screen_up();
        // Restore the bottom line from the original (unscrolled) screen content.
        memcpy(&screen[SCREEN_CELLS - VIDEO_COLUMNS],
               &original_page[SCREEN_CELLS - ((unsigned)scrolled_lines * VIDEO_COLUMNS)],
               VIDEO_COLUMNS * sizeof(video_cell_t));
        __flush(SCREEN_CELLS - VIDEO_COLUMNS, VIDEO_COLUMNS);
        // We're now one line less scrolled back.
        --scrolled_lines;
    }
    // When scrolled_lines == 0, we're at the live view. Don't scroll further forward.
    // This prevents scrolling past the bottom of actual content.
}

void video_shift_one_line_down(void)
{
    // Check if we haven't scrolled beyond our scrollback buffer limit.
    if (scrolled_lines < (int)HISTORY_ROWS) {
        // Save the current visible screen before first scroll operation.
        if (scrolled_lines == 0) {
            memcpy(original_page, screen, SCREEN_CELLS * sizeof(video_cell_t));
        }
        // We're now one line deeper into scrollback history.
        ++scrolled_lines;
        // Shift screen content down, making room at the top.
        __shift_screen_down();
        // Restore the top line from the scrollback buffer.
        memcpy(screen, &history[HISTORY_CELLS - ((unsigned)scrolled_lines * VIDEO_COLUMNS)],
               VIDEO_COLUMNS * sizeof(video_cell_t));
        __flush(0, VIDEO_COLUMNS);
    }
}

void video_shift_one_page_up(void)
{
    // Scroll up by one full page (VIDEO_ROWS lines).
    for (unsigned i = 0; i < VIDEO_ROWS; ++i) {
        video_shift_one_line_up();
    }
}

void video_shift_one_page_down(void)
{
    // Scroll down by one full page (VIDEO_ROWS lines).
    for (unsigned i = 0; i < VIDEO_ROWS; ++i) {
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
    if (lines > (int)HISTORY_ROWS) {
        lines = (int)HISTORY_ROWS;
    }
    // Scroll down by the specified number of lines.
    for (int i = 0; i < lines; ++i) {
        video_shift_one_line_down();
    }
}
