///                MentOS, The Mentoring Operating system project
/// @file video.c
/// @brief Video functions and costants.
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "video.h"
#include "debug.h"
#include "stdbool.h"

/// The height of the screen.
#define SCREEN_HEIGHT 25

/// The width of the screen.
#define SCREEN_WIDTH 80

/// @brief Structure used to hold information about the screen.
typedef struct screen_t {
	/// The width of the screen.
	uint32_t width;
	/// The height of the screen.
	uint32_t height;
	/// Pointer to the memory of the screen.
	char *memory;
	/// Pointer to a position of the screen.
	char *pointer;
	/// The current foreground color.
	char foreground_color;
	/// The current background color.
	char background_color;
	/// The buffer used when scrolling upward.
	char upbuffer[SCREEN_HEIGHT][SCREEN_WIDTH * 2];
	/// The buffer used when scrolling downward.
	char downbuffer[SCREEN_HEIGHT][SCREEN_WIDTH * 2];
	/// The output has been scrolled.
	bool_t is_scrolled;
	/// If the output has been shifted at least once.
	bool_t is_shifted_once;
	/// Used to store the last x coordiantes (used when scrolling).
	uint32_t stored_x;
	/// Used to store the last y coordiantes (used when scrolling).
	uint32_t stored_y;
} screen_t;

/// The information concerning the screen.
screen_t screen;

void video_init()
{
	// ===================================
	screen.height = 25;
	screen.width = 80;
	screen.memory = (char *)0xb8000;
	screen.pointer = (char *)0xb8000;
	screen.foreground_color = GREY;
	screen.background_color = BLACK;
	screen.is_scrolled = false;
	screen.is_shifted_once = false;
	// ===================================
	video_clear();
}

static inline void _draw_char(char c)
{
	*(screen.pointer++) = c;
	*(screen.pointer++) =
		(screen.foreground_color + 16 * screen.background_color);
}

void video_putc(int c)
{
	// If we have stored the last coordinates, this means that we are
	// currently scrolling upwards. Thus, we need to scroll down first.
	if (screen.stored_x && screen.stored_y) {
		video_scroll_down();
	}
	// If the character is '\n' go the new line.
	if (c == '\n') {
		video_new_line();
	} else if (c == '\033') {
		__asm__("nop");
	} else if (c == '\t') {
		video_put_tab();
	} else if (c == '\b') {
		video_delete_last_character();
	} else if (c == '\r') {
		video_cartridge_return();
	} else {
		_draw_char(c);
	}
	video_shift_one_line();
	video_set_cursor_auto();
}

void video_puts(const char *str)
{
	while ((*str) != 0) {
		if ((*str) == '\n') {
			video_new_line();
		} else if ((*str) == '\033') {
			video_set_color(*(++str));
		} else if ((*str) == '\t') {
			video_put_tab();
		} else if ((*str) == '\b') {
			video_delete_last_character();
		} else if ((*str) == '\r') {
			video_cartridge_return();
		} else {
			video_putc((*str));
		}
		++str;
	}
}

void video_set_color(const video_color_t foreground)
{
	screen.foreground_color = foreground;
}

void video_set_background(const video_color_t background)
{
	screen.background_color = background;
}

void video_delete_last_character()
{
	if (lower_bound_y != video_get_line() ||
		lower_bound_x < video_get_column()) {
		video_set_color(WHITE);
		video_set_background(BLACK);
		// Delete any tabular.
		int x = video_get_column();
		if ((*(screen.pointer - 2)) == '\t') {
			for (int i = 0; (i < (4 - x % 4)); ++i) {
				if ((*(screen.pointer - 2)) != '\t')
					break;
				// Bring back the pointer.
				screen.pointer -= 2;
				// Delete the character.
				_draw_char(' ');
				// Bring back the pointer.
				screen.pointer -= 2;
			}
		} else {
			// Bring back the pointer.
			screen.pointer -= 2;
			// Delete the character.
			_draw_char(' ');
			// Bring back the pointer.
			screen.pointer -= 2;
		}
	}
}

void video_set_cursor(const unsigned int x, const unsigned int y)
{
	uint32_t position = (x * 80) + y;

	// Cursor LOW port to vga INDEX register.
	outportb(0x3D4, 0x0F);
	outportb(0x3D5, (uint8_t)(position & 0xFF));

	// Cursor HIGH port to vga INDEX register.
	outportb(0x3D4, 0x0E);
	outportb(0x3D5, (uint8_t)((position >> 8) & 0xFF));
}

void video_set_cursor_auto()
{
	long x = ((screen.pointer - screen.memory) / 2) / screen.width;
	long y = ((screen.pointer - screen.memory) / 2) % screen.width;

	if (x < 0) {
		dbg_print("Negative x while setting auto-cursor.\n");
		x = 0;
	}

	if (y < 0) {
		dbg_print("Negative x while setting auto-cursor.\n");
		y = 0;
	}

	video_set_cursor((uint32_t)x, (uint32_t)y);
}

void video_move_cursor(int x, int y)
{
	screen.pointer = screen.memory + ((y * screen.width * 2) + (x * 2));
	video_set_cursor_auto();
}

void video_put_tab()
{
	int foreground = screen.foreground_color;
	video_set_color(screen.background_color);
	int x = video_get_column();
	for (int i = 0; i < (4 - x % 4); ++i)
		_draw_char('\t');
	video_set_color(foreground);
}

void video_clear()
{
	screen.pointer = screen.memory;

	for (uint32_t y = 0; y < screen.height; y++) {
		for (uint32_t x = 0; x < screen.width; x++) {
			*(screen.pointer++) = ' ';
			*(screen.pointer++) = 0x7;
		}
	}

	screen.pointer = screen.memory;
}

void video_new_line()
{
	screen.pointer =
		screen.memory +
		((((screen.pointer - screen.memory) / (screen.width * 2)) + 1) *
		 (screen.width * 2));

	video_shift_one_line();
	video_set_cursor_auto();
}

void video_cartridge_return()
{
	screen.pointer =
		screen.memory +
		((((screen.pointer - screen.memory) / (screen.width * 2)) - 1) *
		 (screen.width * 2));

	video_new_line();

	video_shift_one_line();

	video_set_cursor_auto();
}

uint32_t video_get_column()
{
	if (screen.pointer < screen.memory) {
		dbg_print("Get negative value while getting video column.\n");

		return 0;
	}

	return ((screen.pointer - screen.memory) % (screen.width * 2)) / 2;
}

uint32_t video_get_line()
{
	if (screen.pointer < screen.memory) {
		dbg_print("Get negative value while getting video line.\n");

		return 0;
	}

	return ((screen.pointer - screen.memory) / (screen.width * 2));
}

void video_shift_one_line(void)
{
	if (screen.pointer >=
		screen.memory + ((screen.height) * screen.width * 2)) {
		/* We save the line to be lost in a buffer, this will be useful for
         * scrolling.
         */
		video_rotate_scroll_buffer();
		uint32_t index;

		for (index = 0; index < screen.width * 2; index++) {
			screen.upbuffer[screen.height - 1][index] =
				*(screen.memory + index);
		}

		for (char *i = screen.memory;
			 i <= (screen.memory + ((screen.height) * screen.width * 2) +
				   (screen.width * 2));
			 ++i) {
			*i = i[screen.width * 2];
		}
		screen.pointer =
			screen.memory +
			((((screen.pointer - screen.memory) / (screen.width * 2)) - 1) *
			 (screen.width * 2));
		// Set that the scroll has been shifted at least once.
		screen.is_shifted_once = true;
	}
}

void video_rotate_scroll_buffer()
{
	for (uint32_t y = 1; y < screen.height; y++) {
		for (uint32_t x = 0; x < screen.width * 2; x++) {
			screen.upbuffer[y - 1][x] = screen.upbuffer[y][x];
		}
	}
}

void video_scroll_up()
{
	char *ptr = screen.memory;

	if (screen.is_scrolled || !screen.is_shifted_once) {
		return;
	}

	for (uint32_t y = 0; y < screen.height; y++) {
		for (uint32_t x = 0; x < screen.width * 2; x++) {
			screen.downbuffer[y][x] = *ptr;
			*ptr++ = screen.upbuffer[y][x];
		}
	}

	screen.is_scrolled = true;

	screen.stored_x = video_get_column();

	screen.stored_y = video_get_line();

	video_move_cursor(screen.width, screen.height);
}

void video_scroll_down()
{
	char *ptr = screen.memory;

	// If PAGEUP hasn't been pressed, it's useless to go down, there is nothing.
	if (!screen.is_scrolled) {
		return;
	}
	for (uint32_t y = 0; y < screen.height; y++) {
		for (uint32_t x = 0; x < screen.width * 2; x++) {
			*ptr++ = screen.downbuffer[y][x];
		}
	}

	screen.is_scrolled = false;

	video_move_cursor(screen.stored_x, screen.stored_y);

	screen.stored_x = 0;

	screen.stored_y = 0;
}

void video_print_ok()
{
	video_move_cursor(60, video_get_line());

	video_set_color(WHITE);

	video_putc('[');

	video_set_color(BRIGHT_GREEN);

	video_puts("OK");

	video_set_color(WHITE);

	video_puts("]\n");
}

void video_print_fail()
{
	video_move_cursor(60, video_get_line());

	video_set_color(WHITE);

	video_putc('[');

	video_set_color(BRIGHT_RED);

	video_puts("FAIL");

	video_set_color(WHITE);

	video_puts("]\n");
}
