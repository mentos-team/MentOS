/// @file video.h
/// @brief Video functions and costants.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "io/ansi_colors.h"
#include "stdint.h"

/// @brief Initialize the video.
void video_init(void);

/// @brief Updates the video.
void video_update(void);

/// @brief Print the given character on the screen.
/// @param c The character to print.
void video_putc(int c);

/// @brief Prints the given string on the screen.
/// @param str The string to print.
void video_puts(const char *str);

/// @brief When something is written in another position, update the cursor.
void video_update_cursor_position(void);

/// @brief Move the cursor at the position x, y on the screen.
/// @param x The x coordinate.
/// @param y The y coordinate.
void video_move_cursor(unsigned int x, unsigned int y);

/// @brief Returns cursor's position on the screen.
/// @param x The output x coordinate.
/// @param y The output y coordinate.
void video_get_cursor_position(unsigned int *x, unsigned int *y);

/// @brief Returns screen size.
/// @param width The screen width.
/// @param height The screen height.
void video_get_screen_size(unsigned int *width, unsigned int *height);

/// @brief Clears the screen.
void video_clear(void);

/// @brief Move to the following line (the effect of \n character).
void video_new_line(void);

/// @brief Move to the up line (the effect of \n character).
void video_cartridge_return(void);

/// @brief The whole screen is shifted up by one line. Used when the cursor
///        reaches the last position of the screen.
void video_shift_one_line_up(void);

/// @brief Shifts the screen content down by one line, loading the previous line
/// from the buffer if available.
void video_shift_one_line_down(void);

/// @brief The whole screen is shifted up by one page.
void video_shift_one_page_up(void);

/// @brief The whole screen is shifted down by one page.
void video_shift_one_page_down(void);

/// @brief Scrolls the screen up by a specified number of lines, showing
/// previous lines from the buffer.
/// @param lines The number of lines to scroll up.
void video_scroll_up(int lines);

/// @brief Scrolls the screen down by a specified number of lines, restoring
/// lines from the buffer or the original content.
/// @param lines The number of lines to scroll down.
void video_scroll_down(int lines);
