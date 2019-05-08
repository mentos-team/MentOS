///                MentOS, The Mentoring Operating system project
/// @file keyboard.c
/// @brief Keyboard handling.
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "keyboard.h"
#include "isr.h"
#include "video.h"
#include "stdio.h"
#include "keymap.h"
#include "bitops.h"
#include "port_io.h"
#include "pic8259.h"

/// A macro from Ivan to update buffer indexes.
#define STEP(x) (((x) == BUFSIZE - 1) ? 0 : (x + 1))

/// Circular Buffer where the pressed keys are stored.
static int circular_buffer[BUFSIZE];

/// Index inside the buffer...
static long buf_r = 0;

/// Index inside the buffer...
static long buf_w = 0;

/// Tracks the state of the leds.
static uint8_t ledstate = 0;

/// The shadow option is active.
static bool_t shadow = false;

/// The shadow character.
static char shadow_character = 0;

/// The flags concerning the keyboard.
static uint32_t keyboard_flags = 0;

/// Flag which identifies the left shift.
#define KBD_LEFT_SHIFT 1

/// Flag which identifies the right shift.
#define KBD_RIGHT_SHIFT 2

/// Flag which identifies the caps lock.
#define KBD_CAPS_LOCK 4

/// Flag which identifies the num lock.
#define KBD_NUM_LOCK 8

/// Flag which identifies the scroll lock.
#define KBD_SCROLL_LOCK 16

/// Flag which identifies the left control.
#define KBD_LEFT_CONTROL 32

/// Flag which identifies the right control.
#define KBD_RIGHT_CONTROL 64

static inline void push_character(char c)
{
	// Update buffer.
	if (STEP(buf_w) == buf_r) {
		buf_r = STEP(buf_r);
	}
	circular_buffer[buf_w] = c;
	buf_w = STEP(buf_w);
}

void keyboard_install()
{
	// Install the IRQ.
	irq_install_handler(IRQ_KEYBOARD, keyboard_isr, "keyboard");
	// Enable the IRQ.
	pic8259_irq_enable(IRQ_KEYBOARD);
}

void keyboard_isr(pt_regs *r)
{
	(void)r;

	if (!(inportb(0x64) & 1)) {
		return;
	}

	// Take scancode from the port.
	uint32_t scancode = inportb(0x60);
	if (scancode == 0xE0) {
		scancode = (scancode << 8) | inportb(0x60);
	}

	// If the key has just been released.
	if (scancode & 0x80) {
		if (scancode == (KEY_LEFT_SHIFT | CODE_BREAK)) {
			clear_flag(&keyboard_flags, KBD_LEFT_SHIFT);
		} else if (scancode == (KEY_RIGHT_SHIFT | CODE_BREAK)) {
			clear_flag(&keyboard_flags, KBD_RIGHT_SHIFT);
		} else if (scancode == (KEY_LEFT_CONTROL | CODE_BREAK)) {
			clear_flag(&keyboard_flags, KBD_LEFT_CONTROL);
		} else if (scancode == (KEY_RIGHT_CONTROL | CODE_BREAK)) {
			clear_flag(&keyboard_flags, KBD_RIGHT_CONTROL);
		}
	} else {
		int32_t character = 0;

		// Parse the key.
		switch (scancode) {
		case KEY_LEFT_SHIFT:
			set_flag(&keyboard_flags, KBD_LEFT_SHIFT);
			break;
		case KEY_RIGHT_SHIFT:
			set_flag(&keyboard_flags, KBD_RIGHT_SHIFT);
			break;
		case KEY_LEFT_CONTROL:
			set_flag(&keyboard_flags, KBD_LEFT_CONTROL);
			break;
		case KEY_RIGHT_CONTROL:
			set_flag(&keyboard_flags, KBD_RIGHT_CONTROL);
			break;
		case KEY_CAPS_LOCK:
			keyboard_flags ^= KBD_CAPS_LOCK;
			keyboard_update_leds();
			break;
		case KEY_NUM_LOCK:
			keyboard_flags ^= KBD_NUM_LOCK;
			keyboard_update_leds();
			break;
		case KEY_SCROLL_LOCK:
			keyboard_flags ^= KBD_SCROLL_LOCK;
			keyboard_update_leds();
			break;
		case KEY_PAGE_UP:
			video_scroll_up();
			break;
		case KEY_PAGE_DOWN:
			video_scroll_down();
			break;
		case KEY_ESCAPE:
			break;
		case KEY_LEFT_ALT:
			break;
		case KEY_RIGHT_ALT:
			break;
		case KEY_BACKSPACE:
			push_character('\b');
			video_delete_last_character();
			video_set_cursor_auto();
			break;
		case KEY_KP_RETURN:
		case KEY_ENTER:
			push_character('\n');
			video_new_line();
			video_set_cursor_auto();
			pic8259_send_eoi(IRQ_KEYBOARD);
			break;
		case KEY_UP_ARROW:
			push_character('\033');
			push_character('[');
			push_character(72);
			break;
		case KEY_DOWN_ARROW:
			push_character('\033');
			push_character('[');
			push_character(80);
			break;
		case KEY_LEFT_ARROW:
			push_character('\033');
			push_character('[');
			push_character(75);
			break;
		case KEY_RIGHT_ARROW:
			push_character('\033');
			push_character('[');
			push_character(77);
			break;
		default:
			if (has_flag(keyboard_flags, KBD_NUM_LOCK)) {
				character = keymap_it.numlock[scancode];
			}
			if (character <= 0) {
				// Apply shift modifier.
				if (has_flag(keyboard_flags,
							 (KBD_LEFT_SHIFT | KBD_RIGHT_SHIFT))) {
					character = keymap_it.shift[scancode];
				} else {
					character = keymap_it.base[scancode];
				}
			}
			if (character > 0) {
				// Apply caps lock modifier.
				if (has_flag(keyboard_flags, KBD_CAPS_LOCK)) {
					if (character >= 'a' && character <= 'z') {
						character += 'A' - 'a';
					} else if (character >= 'A' && character <= 'Z') {
						character += 'a' - 'A';
					}
				}
				if (!shadow) {
					video_putc(character);
				} else if (shadow_character != 0) {
					video_putc(shadow_character);
				}
				// Update buffer.
				push_character(character);
			}
			break;
		}
	}
	pic8259_send_eoi(IRQ_KEYBOARD);
}

void keyboard_update_leds()
{
	// Handle scroll_loc & num_loc & caps_loc.
	(keyboard_flags & KBD_SCROLL_LOCK) ? (ledstate |= 1) : (ledstate ^= 1);
	(keyboard_flags & KBD_NUM_LOCK) ? (ledstate |= 2) : (ledstate ^= 2);
	(keyboard_flags & KBD_CAPS_LOCK) ? (ledstate |= 4) : (ledstate ^= 4);

	// Write on the port.
	outportb(0x60, 0xED);
	outportb(0x60, ledstate);
}

void keyboard_enable()
{
	outportb(0x60, 0xF4);
}

void keyboard_disable()
{
	outportb(0x60, 0xF5);
}

int keyboard_getc(void)
{
	int c = -1;
	if (buf_r != buf_w) {
		c = circular_buffer[buf_r];
		buf_r = STEP(buf_r);
	}
	return c;
}

void keyboard_set_shadow(const bool_t value)
{
	shadow = value;
	if (shadow == false) {
		shadow_character = 0;
	}
}

void keyboard_set_shadow_character(const char _shadow_character)
{
	shadow_character = _shadow_character;
}

bool_t keyboard_get_shadow()
{
	return shadow;
}

bool_t keyboard_is_ctrl_pressed()
{
	return (bool_t)(keyboard_flags & (KBD_LEFT_CONTROL));
}

bool_t keyboard_is_shifted()
{
	return (bool_t)(keyboard_flags & (KBD_LEFT_SHIFT | KBD_RIGHT_SHIFT));
}
