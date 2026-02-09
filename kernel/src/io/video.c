/// @file video.c
/// @brief Video functions and constants.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

// Setup the logging for this file (do this before any other include).
#include "sys/kernel_levels.h"
#define __DEBUG_HEADER__ "[VIDEO ]"      ///< Change header.
#define __DEBUG_LEVEL__  LOGLEVEL_NOTICE ///< Set log level.
#include "io/debug.h"

#include "ctype.h"
#include "io/port_io.h"
#include "io/video.h"
#include "stdbool.h"
#include "stdio.h"
#include "string.h"

#define HEIGHT                   25                   ///< The height of the screen (rows).
#define WIDTH                    80                   ///< The width of the screen (columns).
#define W2                       (WIDTH * 2)          ///< The width of the screen in bytes.
#define TOTAL_SIZE               (HEIGHT * WIDTH * 2) ///< The total size of the screen in bytes.
#define ADDR                     (char *)0xB8000U     ///< The address of the video memory.
#define STORED_PAGES             10                   ///< The number of stored pages for scrolling.
#define VGA_CRTC_INDEX           0x3D4                ///< VGA CRTC index register port.
#define VGA_CRTC_DATA            0x3D5                ///< VGA CRTC data register port.
#define VGA_CURSOR_START         0x0A                 ///< VGA cursor start register index.
#define VGA_CURSOR_END           0x0B                 ///< VGA cursor end register index.
#define VGA_CURSOR_LOCATION_LOW  0x0F                 ///< VGA cursor location low register index.
#define VGA_CURSOR_LOCATION_HIGH 0x0E                 ///< VGA cursor location high register index.

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

/// @brief Pointer to the current position of the screen writer.
char *pointer = ADDR;

/// @brief The current color attribute (foreground and background).
unsigned char color = 7;

/// @brief Index for writing to escape_buffer. If -1, we are not parsing an escape sequence.
int escape_index = -1;

/// @brief Buffer used to store an escape sequence as it's being parsed.
char escape_buffer[256];

/// @brief Buffer where we store the upper scroll history.
char upper_buffer[STORED_PAGES * TOTAL_SIZE] = {0};

/// @brief Buffer where we store the original page content during scrolling.
char original_page[TOTAL_SIZE] = {0};

/// @brief Indicates if the screen is currently scrolled, and by how many lines.
int scrolled_lines = 0;

/// @brief Flag to batch cursor updates in video_puts to improve performance.
static int batch_cursor_updates = 0;

/// @brief Saved cursor position for ESC [ s and ESC [ u commands.
static char *saved_pointer = ADDR;

/// @brief Get the current column number.
/// @return The column number.
static inline unsigned __get_x(void)
{
    // Validate pointer is within screen bounds.
    if (pointer < ADDR || pointer >= ADDR + TOTAL_SIZE) {
        return 0;
    }
    return ((pointer - ADDR) % (WIDTH * 2)) / 2;
}

/// @brief Get the current row number.
/// @return The row number.
static inline unsigned __get_y(void)
{
    // Validate pointer is within screen bounds.
    if (pointer < ADDR || pointer >= ADDR + TOTAL_SIZE) {
        return 0;
    }
    return (pointer - ADDR) / (WIDTH * 2);
}

/// @brief Draws the given character at the current cursor position.
/// @param c The character to draw.
static inline void __draw_char(char c)
{
    // If we are scrolled, unscroll first to show current content.
    if (scrolled_lines) {
        video_scroll_up(scrolled_lines);
    }

    // Calculate the end of the current line.
    unsigned int current_row = (pointer - ADDR) / W2;
    char *line_end           = ADDR + ((current_row + 1) * W2);

    // Shift characters within the current line to make room for insertion.
    if (pointer < line_end - 2) {
        size_t bytes_to_shift = (line_end - 2) - pointer;
        if (bytes_to_shift > 0) {
            memmove(pointer + 2, pointer, bytes_to_shift);
        }
    }

    // Write the character and its color attribute.
    *pointer       = c;
    *(pointer + 1) = color;

    // Advance the pointer to the next character position.
    pointer += 2;

    // If we've reached the end of the line, wrap to the next line.
    if (pointer >= line_end) {
        pointer = line_end;
    }

    // If pointer goes past the end of the screen, scroll up and reset.
    if (pointer >= ADDR + TOTAL_SIZE) {
        video_shift_one_line_up();
        pointer = ADDR + TOTAL_SIZE - W2;
    }
}

/// @brief Hides the VGA cursor.
void __video_hide_cursor(void)
{
    outportb(VGA_CRTC_INDEX, VGA_CURSOR_START);
    unsigned char cursor_start = inportb(VGA_CRTC_DATA);
    // Set the most significant bit to disable the cursor.
    outportb(VGA_CRTC_DATA, cursor_start | 0x20);
}

/// @brief Shows the VGA cursor by clearing the disable bit.
void __video_show_cursor(void)
{
    // Read current cursor start register
    outportb(VGA_CRTC_INDEX, VGA_CURSOR_START);
    unsigned char cursor_start = inportb(VGA_CRTC_DATA);
    // Clear bit 5 to enable cursor, keep other bits
    outportb(VGA_CRTC_INDEX, VGA_CURSOR_START);
    outportb(VGA_CRTC_DATA, cursor_start & ~0x20);
}

/// @brief Sets the VGA cursor shape by specifying the start and end scan lines.
/// @param start The starting scan line of the cursor (0-15).
/// @param end The ending scan line of the cursor (0-15).
void __video_set_cursor_shape(unsigned char start, unsigned char end)
{
    // Ensure start is less than or equal to end for visible cursor
    if (start > end) {
        start = 0;
        end   = 15;
    }

    // Set the cursor's start scan line with cursor enabled (bit 5 = 0)
    outportb(VGA_CRTC_INDEX, VGA_CURSOR_START);
    outportb(VGA_CRTC_DATA, start & 0x1F);

    // Set the cursor's end scan line
    outportb(VGA_CRTC_INDEX, VGA_CURSOR_END);
    outportb(VGA_CRTC_DATA, end & 0x1F);

    // Explicitly ensure cursor is visible
    __video_show_cursor();
}

/// @brief Issues a command to move the cursor to the given position.
/// @param x The x coordinate.
/// @param y The y coordinate.
static inline void __video_set_cursor_position(unsigned int x, unsigned int y)
{
    // Clamp coordinates to valid screen bounds.
    if (x >= WIDTH)
        x = WIDTH - 1;
    if (y >= HEIGHT)
        y = HEIGHT - 1;

    // Calculate linear position from x,y coordinates.
    uint32_t position = (y * WIDTH) + x;

    // Write low byte of cursor position.
    outportb(VGA_CRTC_INDEX, VGA_CURSOR_LOCATION_LOW);
    outportb(VGA_CRTC_DATA, (uint8_t)(position & 0xFFU));

    // Write high byte of cursor position.
    outportb(VGA_CRTC_INDEX, VGA_CURSOR_LOCATION_HIGH);
    outportb(VGA_CRTC_DATA, (uint8_t)((position >> 8U) & 0xFFU));
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
        if (pointer - 2 >= ADDR) {
            // Move pointer back one character position.
            pointer -= 2;
            if (erase) {
                // Calculate the end of the current line.
                unsigned int current_row = (pointer - ADDR) / W2;
                char *line_end           = ADDR + ((current_row + 1) * W2);

                // Shift characters left on the current line only.
                size_t bytes_to_move = line_end - (pointer + 2);
                if (bytes_to_move > 0) {
                    memmove(pointer, pointer + 2, bytes_to_move);
                }
                // Clear the last character position of the line.
                *(line_end - 2) = ' ';
                *(line_end - 1) = color;
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
        if (pointer + 2 <= ADDR + TOTAL_SIZE) {
            if (erase) {
                // Overwrite with space without shifting other characters.
                *pointer       = ' ';
                *(pointer + 1) = color;
            }
            // Move pointer forward one character position.
            pointer += 2;
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
    switch (shape) {
    case 0: // Default blinking block cursor
    case 2: // Blinking block cursor
        __video_set_cursor_shape(0, 15);
        break;
    case 1: // Steady block cursor
        __video_set_cursor_shape(0, 15);
        break;
    case 3: // Blinking underline cursor
        __video_set_cursor_shape(13, 15);
        break;
    case 4: // Steady underline cursor
        __video_set_cursor_shape(13, 15);
        break;
    case 5: // Blinking vertical bar cursor
        __video_set_cursor_shape(0, 1);
        break;
    case 6: // Steady vertical bar cursor
        __video_set_cursor_shape(0, 1);
        break;
    default:
        // Handle any other cases if needed
        break;
    }
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
    // Clear the screen and set default cursor shape.
    video_clear();

    // Set up cursor with known good values
    // Use default blinking block cursor (scan lines 0-15)
    outportb(VGA_CRTC_INDEX, VGA_CURSOR_START);
    outportb(VGA_CRTC_DATA, 0x00); // Start at line 0, bit 5 clear (enabled)

    outportb(VGA_CRTC_INDEX, VGA_CURSOR_END);
    outportb(VGA_CRTC_DATA, 0x0F); // End at line 15

    // Explicitly show cursor
    __video_show_cursor();
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
                    unsigned int y = __get_y();
                    if (y > 0) {
                        pointer -= W2;
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
                    unsigned int y = __get_y();
                    if (y < HEIGHT - 1) {
                        pointer += W2;
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
                    // Clear from cursor to end of screen.
                    memset(pointer, 0, (ADDR + TOTAL_SIZE) - pointer);
                } else if (mode == 1) {
                    // Clear from start of screen to cursor (inclusive).
                    memset(ADDR, 0, pointer - ADDR + 2);
                } else if (mode == 3) {
                    // Clear entire screen AND scrollback buffer.
                    video_clear();
                } else {
                    // Mode 2 or default: Clear entire screen (but preserve scrollback).
                    memset(ADDR, 0, TOTAL_SIZE);
                    pointer        = ADDR;
                    scrolled_lines = 0;
                    video_update_cursor_position();
                }
            }
            // ESC [ <row>;<col> H or f - Set cursor position.
            else if ((c == 'H') || (c == 'f')) {
                char *semicolon = strchr(&escape_buffer[1], ';');
                if (semicolon != NULL) {
                    *semicolon     = '\0';
                    unsigned int y = atoi(&escape_buffer[1]) - 1;
                    unsigned int x = atoi(semicolon + 1) - 1;
                    if (x >= WIDTH)
                        x = WIDTH - 1;
                    if (y >= HEIGHT)
                        y = HEIGHT - 1;
                    pointer = ADDR + (y * WIDTH * 2 + x * 2);
                } else {
                    pointer = ADDR;
                }
                video_update_cursor_position();
            }
            // ESC [ <n> q - Set cursor shape.
            else if (c == 'q') {
                __parse_cursor_escape_code(atoi(&escape_buffer[1]));
            }
            // ESC [ <n> K - Erase in line.
            else if (c == 'K') {
                int mode                 = atoi(&escape_buffer[1]);
                unsigned int current_row = (pointer - ADDR) / W2;
                char *line_start         = ADDR + (current_row * W2);
                char *line_end           = line_start + W2;

                if (mode == 0) {
                    // Clear from cursor to end of line.
                    memset(pointer, 0, line_end - pointer);
                } else if (mode == 1) {
                    // Clear from start of line to cursor.
                    memset(line_start, 0, pointer - line_start + 2);
                } else if (mode == 2) {
                    // Clear entire line.
                    memset(line_start, 0, W2);
                }
            }
            // ESC [ s - Save cursor position.
            else if (c == 's') {
                saved_pointer = pointer;
            }
            // ESC [ u - Restore cursor position.
            else if (c == 'u') {
                // Validate saved pointer is within screen bounds.
                if (saved_pointer >= ADDR && saved_pointer < ADDR + TOTAL_SIZE) {
                    pointer = saved_pointer;
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
        unsigned int current_row = (pointer - ADDR) / W2;
        char *line_end           = ADDR + ((current_row + 1) * W2);

        // Shift characters left on the current line only.
        size_t bytes_to_move = line_end - (pointer + 2);
        if (bytes_to_move > 0) {
            memmove(pointer, pointer + 2, bytes_to_move);
        }
        // Clear the last character position of the line.
        *(line_end - 2) = ' ';
        *(line_end - 1) = color;

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
    // Ensure there's a character at the cursor position for VGA hardware cursor visibility.
    // VGA cursor needs a non-null character cell to display over.
    if (pointer[0] == 0) {
        pointer[0] = ' ';   // Character
        pointer[1] = color; // Attribute
    }
    // Convert byte pointer to character coordinates (divide by 2 since each char uses 2 bytes).
    __video_set_cursor_position(((pointer - ADDR) / 2U) % WIDTH, ((pointer - ADDR) / 2U) / WIDTH);
}

void video_move_cursor(unsigned int x, unsigned int y)
{
    // Clamp coordinates to screen bounds.
    if (x >= WIDTH)
        x = WIDTH - 1;
    if (y >= HEIGHT)
        y = HEIGHT - 1;
    // Calculate pointer position (each character is 2 bytes).
    pointer = ADDR + ((y * WIDTH * 2) + (x * 2));
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
        *width = WIDTH;
    }
    // Return screen height if requested.
    if (height) {
        *height = HEIGHT;
    }
}

void video_clear(void)
{
    // Clear the scrollback buffer.
    memset(upper_buffer, 0, STORED_PAGES * TOTAL_SIZE);
    // Clear the visible screen.
    memset(ADDR, 0, TOTAL_SIZE);
    // Reset cursor to top-left corner.
    pointer        = ADDR;
    // Reset scrolling state.
    scrolled_lines = 0;
    video_update_cursor_position();
}

void video_new_line(void)
{
    // If we're viewing scrollback, unscroll first to show current content.
    if (scrolled_lines) {
        video_scroll_up(scrolled_lines);
    }

    // Move pointer to the start of the next line.
    pointer = ADDR + ((pointer - ADDR) / W2 + 1) * W2;
    // Check if we've gone past the bottom of the screen.
    if (pointer >= ADDR + TOTAL_SIZE) {
        // Scroll up by one line and move content into scrollback.
        video_shift_one_line_up();
        // Position cursor at the beginning of the last line.
        pointer = ADDR + TOTAL_SIZE - W2;
    }
    video_update_cursor_position();
}

void video_cartridge_return(void)
{
    // If we're viewing scrollback, unscroll first to show current content.
    if (scrolled_lines) {
        video_scroll_up(scrolled_lines);
    }

    // Calculate which row we're on.
    unsigned int current_row = (pointer - ADDR) / W2;
    // Move pointer to the beginning of the current line.
    pointer                  = ADDR + (current_row * W2);
    video_update_cursor_position();
}

/// @brief Shifts the buffer up or down by one line.
/// @param buffer Pointer to the buffer to shift.
/// @param lines Number of lines we want to shift.
/// @param direction 1 to shift up, -1 to shift down.
static inline void __shift_buffer(char *buffer, int lines, int direction)
{
    // Shift up: Move all lines in one operation.
    if (direction == 1) {
        // Move (lines-1) lines from row 1 to row 0
        memmove(buffer, buffer + W2, W2 * (lines - 1));
    }
    // Shift down: Move all lines in one operation.
    else if (direction == -1) {
        // Move (lines-1) lines from row 0 to row 1
        memmove(buffer + W2, buffer, W2 * (lines - 1));
    }
}

/// @brief Shifts the screen content up by one line. When not scrolled, moves
/// the top line into the `upper_buffer`.
static void __shift_screen_up(void)
{
    if (scrolled_lines == 0) {
        // Move the upper buffer up by one line.
        __shift_buffer(upper_buffer, STORED_PAGES * HEIGHT, +1);
        // Copy the first line on the screen inside the last line of the upper buffer.
        memcpy(upper_buffer + (TOTAL_SIZE * STORED_PAGES - W2), ADDR, W2);
    }
    // Move the screen up by one line.
    __shift_buffer(ADDR, HEIGHT, +1);
    // Clear the last line of the screen.
    memset(ADDR + (W2 * (HEIGHT - 1)), 0, W2);
}

/// @brief Shifts the screen content down by one line. Restores the topmost line
/// from the `upper_buffer`.
static void __shift_screen_down(void)
{
    // Move the screen content down by one line.
    __shift_buffer(ADDR, HEIGHT, -1);
    // Restore from the `upper_buffer`.
    memcpy(ADDR, upper_buffer + (W2 * (STORED_PAGES * HEIGHT - scrolled_lines)), W2);
}

void video_shift_one_line_up(void)
{
    // Handle case where cursor is beyond screen (during scrolling operations).
    if (pointer >= ADDR + TOTAL_SIZE) {
        // Shift the screen and scrollback buffer up.
        __shift_screen_up();
        // Adjust pointer to stay on the last line.
        pointer = ADDR + ((pointer - ADDR) / W2 - 1) * W2;
    }
    // Handle case where we're viewing scrollback history and want to scroll to newer content.
    else if (scrolled_lines > 0) {
        // Shift screen up, moving top line into scrollback.
        __shift_screen_up();
        // Restore the bottom line from the original (unscrolled) screen content.
        memcpy(ADDR + (W2 * (HEIGHT - 1)), original_page + (W2 * (TOTAL_SIZE / W2 - scrolled_lines)), W2);
        // We're now one line less scrolled back.
        --scrolled_lines;
    }
    // When scrolled_lines == 0, we're at the live view. Don't scroll further forward.
    // This prevents scrolling past the bottom of actual content.
}

void video_shift_one_line_down(void)
{
    // Check if we haven't scrolled beyond our scrollback buffer limit.
    if (scrolled_lines < (STORED_PAGES * HEIGHT)) {
        // Save the current visible screen before first scroll operation.
        if (scrolled_lines == 0) {
            memcpy(original_page, ADDR, TOTAL_SIZE);
        }
        // We're now one line deeper into scrollback history.
        ++scrolled_lines;
        // Shift screen content down, making room at the top.
        __shift_screen_down();
        // Restore the top line from the scrollback buffer.
        memcpy(ADDR, upper_buffer + (W2 * (STORED_PAGES * HEIGHT - scrolled_lines)), W2);
    }
}

void video_shift_one_page_up(void)
{
    // Scroll up by one full page (HEIGHT lines).
    for (int i = 0; i < HEIGHT; ++i) {
        video_shift_one_line_up();
    }
}

void video_shift_one_page_down(void)
{
    // Scroll down by one full page (HEIGHT lines).
    for (int i = 0; i < HEIGHT; ++i) {
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
    if (lines > STORED_PAGES * HEIGHT) {
        lines = STORED_PAGES * HEIGHT;
    }
    // Scroll down by the specified number of lines.
    for (int i = 0; i < lines; ++i) {
        video_shift_one_line_down();
    }
}
