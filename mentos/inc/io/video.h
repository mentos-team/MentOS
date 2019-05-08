///                MentOS, The Mentoring Operating system project
/// @file video.h
/// @brief Video functions and costants.
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "stdint.h"

/// @brief A set of colors.
typedef enum video_color_t {
	///0 : Black
	BLACK,
	/// 1 : Blue
	BLUE,
	/// 2 : Green
	GREEN,
	/// 3 : Cyan
	CYAN,
	/// 4 : Red
	RED,
	/// 5 : Magenta
	MAGENTA,
	/// 6 : Brown
	BROWN,
	/// 7 : Grey
	GREY,
	/// 8 : Dark Grey
	DARK_GREY,
	/// 9 : Bright Blue
	BRIGHT_BLUE,
	/// 10 : Bright Green
	BRIGHT_GREEN,
	/// 11 : Bright Cyan
	BRIGHT_CYAN,
	/// 12 : Bright Red
	BRIGHT_RED,
	/// 13 : Bright Magenta
	BRIGHT_MAGENTA,
	/// 14 : Yellow
	YELLOW,
	/// 15 : White
	WHITE,
} video_color_t;

/// @brief Initialize the video.
void video_init();

/// @brief Print the given character on the screen.
void video_putc(int);

/// @brief Prints the given string on the screen.
void video_puts(const char *str);

/// @brief Change foreground colour.
void video_set_color(const video_color_t foreground);

/// @brief Change background colour.
void video_set_background(const video_color_t background);

/// @brief Deletes the last inserted character.
void video_delete_last_character();

/// @brief Move the cursor to the given position.
void video_set_cursor(const unsigned int x, const unsigned int y);

/// @brief When something is written in another position, update the cursor.
void video_set_cursor_auto();

/// @brief Move the cursor at the position x, y on the screen.
void video_move_cursor(int, int);

/// @brief Prints a tab on the screen.
void video_put_tab();

/// @brief Clears the screen.
void video_clear();

/// @brief Move to the following line (the effect of \n character).
void video_new_line();

/// @brief Move to the up line (the effect of \n character).
void video_cartridge_return();

/// @brief Get the current column number.
uint32_t video_get_column();

/// @brief Get the current row number.
uint32_t video_get_line();

/// @brief The whole screen is shifted up by one line. Used when the cursor
///        reaches the last position of the screen.
void video_shift_one_line();

/// @brief The scrolling buffer is updated to contain the screen up the
///        current one. The oldest line is lost to make space for the new one.
void video_rotate_scroll_buffer();

/// @brief Called by the pression of the PAGEUP key.
///        The screen aboce the current one is printed and the current one is
///        saved in downbuffer, ready to be restored in future.
void video_scroll_up();

/// @brief Called by the pression of the PAGEDOWN key.
///        The content of downbuffer (that is, the screen present when you
///        pressed PAGEUP) is printed again.
void video_scroll_down();

/// Determines the lower-bound on the x axis for the video.
uint32_t lower_bound_x;

/// Determines the lower-bound on the y axis for the video.
uint32_t lower_bound_y;

/// Determines the current position of the shell cursor on the x axis.
uint32_t shell_current_x;

/// Determines the current position of the shell cursor on the y axis.
uint32_t shell_current_y;

/// Determines the lower-bound on the x axis for the shell.
uint32_t shell_lower_bound_x;

/// Determines the lower-bound on the y axis for the shell.
uint32_t shell_lower_bound_y;

/// @brief Prints [OK] at the current row and column 60.
void video_print_ok();

/// @brief Prints [FAIL] at the current row and column 60.
void video_print_fail();
