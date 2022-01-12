/// @file keyboard.c
/// @brief Keyboard handling.
/// @copyright (c) 2014-2021 This file is distributed under the MIT License.
/// See LICENSE.md for details.
/// @addtogroup keyboard
/// @{

// Include the kernel log levels.
#include "sys/kernel_levels.h"
/// Change the header.
#define __DEBUG_HEADER__ "[KEYBRD]"
/// Set the log level.
#define __DEBUG_LEVEL__ LOGLEVEL_DEBUG

#include "drivers/keyboard/keyboard.h"

#include "io/port_io.h"
#include "hardware/pic8259.h"
#include "drivers/keyboard/keymap.h"
#include "sys/bitops.h"
#include "io/video.h"
#include "io/debug.h"
#include "drivers/ps2.h"
#include "ctype.h"
#include "descriptor_tables/isr.h"
#include "process/scheduler.h"
#include "ring_buffer.h"
#include "string.h"

/// The dimension of the circular buffer used to store video history.
#define BUFSIZE 256

/// A macro from Ivan to update buffer indexes.
#define STEP(x) (((x) == BUFSIZE - 1) ? 0 : ((x) + 1))

/// Circular Buffer where the pressed keys are stored.
static int circular_buffer[BUFSIZE] = { 0 };
/// Index inside the buffer...
static long buf_r = 0;
/// Index inside the buffer...
static long buf_w = 0;
/// Tracks the state of the leds.
static uint8_t ledstate = 0;
/// The flags concerning the keyboard.
static uint32_t kflags = 0;

#define KBD_LEFT_SHIFT    (1 << 0) ///< Flag which identifies the left shift.
#define KBD_RIGHT_SHIFT   (1 << 1) ///< Flag which identifies the right shift.
#define KBD_CAPS_LOCK     (1 << 2) ///< Flag which identifies the caps lock.
#define KBD_NUM_LOCK      (1 << 3) ///< Flag which identifies the num lock.
#define KBD_SCROLL_LOCK   (1 << 4) ///< Flag which identifies the scroll lock.
#define KBD_LEFT_CONTROL  (1 << 5) ///< Flag which identifies the left control.
#define KBD_RIGHT_CONTROL (1 << 6) ///< Flag which identifies the right control.
#define KBD_LEFT_ALT      (1 << 7) ///< Flag which identifies the left alt.
#define KBD_RIGHT_ALT     (1 << 8) ///< Flag which identifies the right alt.

static inline void push_character(int c)
{
    // Update buffer.
    if (STEP(buf_w) == buf_r) {
        buf_r = STEP(buf_r);
    }

    circular_buffer[buf_w] = c & 0x00FF;

    buf_w = STEP(buf_w);
}

static inline int read_character()
{
    int c = -1;
    if (buf_r != buf_w) {
        c     = circular_buffer[buf_r];
        buf_r = STEP(buf_r);
    }
    return c;
}

static inline bool_t get_keypad_number(int scancode)
{
    if (scancode == KEY_KP0)
        return 0;
    if (scancode == KEY_KP1)
        return 1;
    if (scancode == KEY_KP2)
        return 2;
    if (scancode == KEY_KP3)
        return 3;
    if (scancode == KEY_KP4)
        return 4;
    if (scancode == KEY_KP5)
        return 5;
    if (scancode == KEY_KP6)
        return 6;
    if (scancode == KEY_KP7)
        return 7;
    if (scancode == KEY_KP8)
        return 8;
    if (scancode == KEY_KP9)
        return 9;
    return -1;
}

void keyboard_isr(pt_regs *f)
{
    unsigned int scancode;
    (void)f;

    if (!(inportb(0x64U) & 1U)) {
        return;
    }

    // Take scancode from the port.
    scancode = ps2_read();
    if (scancode == 0xE0) {
        scancode = (scancode << 8U) | ps2_read();
    }

    // Get the keypad number, of num-lock is disabled. Otherwise, initialize to -1;
    int keypad_fun_number = !bitmask_check(kflags, KBD_NUM_LOCK) ? get_keypad_number(scancode) : -1;

    // If the key has just been released.
    if (scancode == KEY_LEFT_SHIFT) {
        bitmask_set_assign(kflags, KBD_LEFT_SHIFT);
        pr_debug("Press(KBD_LEFT_SHIFT)\n");
    } else if (scancode == KEY_RIGHT_SHIFT) {
        bitmask_set_assign(kflags, KBD_RIGHT_SHIFT);
        pr_debug("Press(KBD_RIGHT_SHIFT)\n");
    } else if (scancode == KEY_LEFT_CONTROL) {
        bitmask_set_assign(kflags, KBD_LEFT_CONTROL);
        pr_debug("Press(KBD_LEFT_CONTROL)\n");
    } else if (scancode == KEY_RIGHT_CONTROL) {
        bitmask_set_assign(kflags, KBD_RIGHT_CONTROL);
        pr_debug("Press(KBD_RIGHT_CONTROL)\n");
    } else if (scancode == KEY_LEFT_ALT) {
        bitmask_set_assign(kflags, KBD_LEFT_ALT);
        push_character(scancode << 16u);
        pr_debug("Press(KBD_LEFT_ALT)\n");
    } else if (scancode == KEY_RIGHT_ALT) {
        bitmask_set_assign(kflags, KBD_RIGHT_ALT);
        push_character(scancode << 16u);
        pr_debug("Press(KBD_RIGHT_ALT)\n");
    } else if (scancode == (KEY_LEFT_SHIFT | CODE_BREAK)) {
        bitmask_clear_assign(kflags, KBD_LEFT_SHIFT);
        pr_debug("Release(KBD_LEFT_SHIFT)\n");
    } else if (scancode == (KEY_RIGHT_SHIFT | CODE_BREAK)) {
        bitmask_clear_assign(kflags, KBD_RIGHT_SHIFT);
        pr_debug("Release(KBD_RIGHT_SHIFT)\n");
    } else if (scancode == (KEY_LEFT_CONTROL | CODE_BREAK)) {
        bitmask_clear_assign(kflags, KBD_LEFT_CONTROL);
        pr_debug("Release(KBD_LEFT_CONTROL)\n");
    } else if (scancode == (KEY_RIGHT_CONTROL | CODE_BREAK)) {
        bitmask_clear_assign(kflags, KBD_RIGHT_CONTROL);
        pr_debug("Release(KBD_RIGHT_CONTROL)\n");
    } else if (scancode == (KEY_LEFT_ALT | CODE_BREAK)) {
        bitmask_clear_assign(kflags, KBD_LEFT_ALT);
        pr_debug("Release(KBD_LEFT_ALT)\n");
    } else if (scancode == (KEY_RIGHT_ALT | CODE_BREAK)) {
        bitmask_clear_assign(kflags, KBD_RIGHT_ALT);
        pr_debug("Release(KBD_RIGHT_ALT)\n");
    } else if (scancode == KEY_CAPS_LOCK) {
        bitmask_flip_assign(kflags, KBD_CAPS_LOCK);
        keyboard_update_leds();
        pr_debug("Toggle(KBD_CAPS_LOCK)\n");
    } else if (scancode == KEY_NUM_LOCK) {
        bitmask_flip_assign(kflags, KBD_NUM_LOCK);
        keyboard_update_leds();
        pr_debug("Toggle(KBD_NUM_LOCK)\n");
    } else if (scancode == KEY_SCROLL_LOCK) {
        bitmask_flip_assign(kflags, KBD_SCROLL_LOCK);
        keyboard_update_leds();
        pr_debug("Toggle(KBD_SCROLL_LOCK)\n");
    } else if (scancode == KEY_BACKSPACE) {
        push_character('\b');
        pr_debug("Press(KEY_BACKSPACE)\n");
    } else if (scancode == KEY_DELETE) {
        push_character(127);
        pr_debug("Press(KEY_DELETE)\n");
    } else if ((scancode == KEY_ENTER) || (scancode == KEY_KP_RETURN)) {
        push_character('\n');
        pr_debug("Press(KEY_ENTER)\n");
    } else if ((scancode == KEY_PAGE_UP) || (keypad_fun_number == 9)) {
        push_character(scancode);
        pr_debug("Press(KEY_PAGE_UP)\n");
    } else if ((scancode == KEY_PAGE_DOWN) || (keypad_fun_number == 2)) {
        push_character(scancode);
        pr_debug("Press(KEY_PAGE_DOWN)\n");
    } else if ((scancode == KEY_UP_ARROW) || (keypad_fun_number == 8)) {
        pr_debug("Press(KEY_UP_ARROW)\n");
        push_character('\033');
        push_character('[');
        push_character('A');
    } else if ((scancode == KEY_DOWN_ARROW) || (keypad_fun_number == 2)) {
        pr_debug("Press(KEY_DOWN_ARROW)\n");
        push_character('\033');
        push_character('[');
        push_character('B');
    } else if ((scancode == KEY_RIGHT_ARROW) || (keypad_fun_number == 6)) {
        pr_debug("Press(KEY_RIGHT_ARROW)\n");
        push_character('\033');
        push_character('[');
        push_character('C');
    } else if ((scancode == KEY_LEFT_ARROW) || (keypad_fun_number == 4)) {
        pr_debug("Press(KEY_LEFT_ARROW)\n");
        push_character('\033');
        push_character('[');
        push_character('D');
    } else if ((scancode == KEY_HOME) || (keypad_fun_number == 7)) {
        pr_debug("Press(KEY_HOME)\n");
        push_character('\033');
        push_character('[');
        push_character('H');
    } else if ((scancode == KEY_END) || (keypad_fun_number == 1)) {
        pr_debug("Press(KEY_END)\n");
        push_character('\033');
        push_character('[');
        push_character('F');
    } else if (scancode == KEY_ESCAPE) {
        // Nothing to do.
    } else if (!(scancode & CODE_BREAK)) {
        // Get the current keymap.
        const keymap_t *keymap = get_keymap(scancode);
        // Get the specific keymap.
#if 0
        pr_debug("%04x '%c' (%04x) '%c' (%04x) '%c' (%04x) '%c' (%04x))\n",
                 scancode,
                 ((0x00ff & keymap->normal) >= 32) ? keymap->normal : ' ', keymap->normal,
                 ((0x00ff & keymap->shift) >= 32) ? keymap->shift : ' ', keymap->shift,
                 ((0x00ff & keymap->ctrl) >= 32) ? keymap->ctrl : ' ', keymap->ctrl,
                 ((0x00ff & keymap->alt) >= 32) ? keymap->alt : ' ', keymap->alt);
#endif
        int character = 0;
        if (!bitmask_check(kflags, KBD_LEFT_SHIFT | KBD_RIGHT_SHIFT) != !bitmask_check(kflags, KBD_CAPS_LOCK)) {
            push_character(keymap->shift);
        } else if ((get_keymap_type() == KEYMAP_IT) && bitmask_check(kflags, KBD_RIGHT_ALT) && (bitmask_check(kflags, KBD_LEFT_SHIFT | KBD_RIGHT_SHIFT))) {
            push_character(keymap->alt);
        } else if (bitmask_check(kflags, KBD_RIGHT_ALT)) {
            push_character(keymap->alt);
        } else if (get_keypad_number(scancode) && bitmask_check(kflags, KBD_NUM_LOCK)) {
            push_character(keymap->normal);
        } else {
            push_character(keymap->normal);
        }
        // Parse the key.
        /*
            else if () {
                character = keymap->alt[scancode];
            }
            // We have failed to retrieve the character.
            if (character <= 0) {
                break;
            }
            if (bitmask_check(kflags, KBD_LEFT_CONTROL | KBD_RIGHT_CONTROL)) {
                push_character('\033');
                push_character('^');
                character = toupper(character);
            }
            // Update buffer.
            push_character(character);
            */
    }
    pic8259_send_eoi(IRQ_KEYBOARD);
}

void keyboard_update_leds()
{
    // Handle scroll_loc & num_loc & caps_loc.
    bitmask_check(kflags, KBD_SCROLL_LOCK) ? (ledstate |= 1) : (ledstate ^= 1);
    bitmask_check(kflags, KBD_NUM_LOCK) ? (ledstate |= 2) : (ledstate ^= 2);
    bitmask_check(kflags, KBD_CAPS_LOCK) ? (ledstate |= 4) : (ledstate ^= 4);

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

int keyboard_getc()
{
    return read_character();
}

int keyboard_initialize()
{
    // Initialize the keymaps.
    init_keymaps();
    // Install the IRQ.
    irq_install_handler(IRQ_KEYBOARD, keyboard_isr, "keyboard");
    // Enable the IRQ.
    pic8259_irq_enable(IRQ_KEYBOARD);
    return 0;
}

int keyboard_finalize()
{
    // Install the IRQ.
    irq_uninstall_handler(IRQ_KEYBOARD, keyboard_isr);
    // Enable the IRQ.
    pic8259_irq_disable(IRQ_KEYBOARD);
    return 0;
}

/// @}