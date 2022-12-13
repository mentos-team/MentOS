/// @file keyboard.c
/// @brief Keyboard handling.
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.
/// @addtogroup keyboard
/// @{

// Include the kernel log levels.
#include "sys/kernel_levels.h"
/// Change the header.
#define __DEBUG_HEADER__ "[KEYBRD]"
/// Set the log level.
#define __DEBUG_LEVEL__ LOGLEVEL_NOTICE

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

/// Tracks the state of the leds.
static uint8_t ledstate = 0;
/// The flags concerning the keyboard.
static uint32_t kflags = 0;
/// Where we store the keypress.
fs_rb_scancode_t scancodes;
/// Spinlock to protect access to the scancode buffer.
spinlock_t scancodes_lock;

#define KBD_LEFT_SHIFT    (1 << 0) ///< Flag which identifies the left shift.
#define KBD_RIGHT_SHIFT   (1 << 1) ///< Flag which identifies the right shift.
#define KBD_CAPS_LOCK     (1 << 2) ///< Flag which identifies the caps lock.
#define KBD_NUM_LOCK      (1 << 3) ///< Flag which identifies the num lock.
#define KBD_SCROLL_LOCK   (1 << 4) ///< Flag which identifies the scroll lock.
#define KBD_LEFT_CONTROL  (1 << 5) ///< Flag which identifies the left control.
#define KBD_RIGHT_CONTROL (1 << 6) ///< Flag which identifies the right control.
#define KBD_LEFT_ALT      (1 << 7) ///< Flag which identifies the left alt.
#define KBD_RIGHT_ALT     (1 << 8) ///< Flag which identifies the right alt.

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

static inline void keyboard_push_front(int c)
{
    if (c >= 0) {
        spinlock_lock(&scancodes_lock);
        fs_rb_scancode_push_front(&scancodes, c);
        spinlock_unlock(&scancodes_lock);
    }
}

int keyboard_pop_back()
{
    int c;
    spinlock_lock(&scancodes_lock);
    if (!fs_rb_scancode_empty(&scancodes))
        c = fs_rb_scancode_pop_back(&scancodes);
    else
        c = -1;
    spinlock_unlock(&scancodes_lock);
    return c;
}

int keyboard_back()
{
    int c;
    spinlock_lock(&scancodes_lock);
    if (!fs_rb_scancode_empty(&scancodes))
        c = fs_rb_scancode_back(&scancodes);
    else
        c = -1;
    spinlock_unlock(&scancodes_lock);
    return c;
}

int keyboard_front()
{
    int c = -1;
    spinlock_lock(&scancodes_lock);
    if (!fs_rb_scancode_empty(&scancodes))
        c = fs_rb_scancode_front(&scancodes);
    else
        c = -1;
    spinlock_unlock(&scancodes_lock);
    return c;
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
        keyboard_push_front(scancode << 16u);
        pr_debug("Press(KBD_LEFT_ALT)\n");
    } else if (scancode == KEY_RIGHT_ALT) {
        bitmask_set_assign(kflags, KBD_RIGHT_ALT);
        keyboard_push_front(scancode << 16u);
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
        keyboard_push_front('\b');
        pr_debug("Press(KEY_BACKSPACE)\n");
    } else if (scancode == KEY_DELETE) {
        keyboard_push_front(0x7F);
        pr_debug("Press(KEY_DELETE)\n");
    } else if ((scancode == KEY_ENTER) || (scancode == KEY_KP_RETURN)) {
        keyboard_push_front('\n');
        pr_debug("Press(KEY_ENTER)\n");
    } else if ((scancode == KEY_PAGE_UP) || (keypad_fun_number == 9)) {
        keyboard_push_front(scancode);
        pr_debug("Press(KEY_PAGE_UP)\n");
    } else if ((scancode == KEY_PAGE_DOWN) || (keypad_fun_number == 2)) {
        keyboard_push_front(scancode);
        pr_debug("Press(KEY_PAGE_DOWN)\n");
    } else if ((scancode == KEY_UP_ARROW) || (keypad_fun_number == 8)) {
        pr_debug("Press(KEY_UP_ARROW)\n");
        keyboard_push_front('\033');
        keyboard_push_front('[');
        keyboard_push_front('A');
    } else if ((scancode == KEY_DOWN_ARROW) || (keypad_fun_number == 2)) {
        pr_debug("Press(KEY_DOWN_ARROW)\n");
        keyboard_push_front('\033');
        keyboard_push_front('[');
        keyboard_push_front('B');
    } else if ((scancode == KEY_RIGHT_ARROW) || (keypad_fun_number == 6)) {
        pr_debug("Press(KEY_RIGHT_ARROW)\n");
        keyboard_push_front('\033');
        keyboard_push_front('[');
        keyboard_push_front('C');
    } else if ((scancode == KEY_LEFT_ARROW) || (keypad_fun_number == 4)) {
        pr_debug("Press(KEY_LEFT_ARROW)\n");
        keyboard_push_front('\033');
        keyboard_push_front('[');
        keyboard_push_front('D');
    } else if ((scancode == KEY_HOME) || (keypad_fun_number == 7)) {
        pr_debug("Press(KEY_HOME)\n");
        keyboard_push_front('\033');
        keyboard_push_front('[');
        keyboard_push_front('H');
    } else if ((scancode == KEY_END) || (keypad_fun_number == 1)) {
        pr_debug("Press(KEY_END)\n");
        keyboard_push_front('\033');
        keyboard_push_front('[');
        keyboard_push_front('F');
    } else if (scancode == KEY_ESCAPE) {
        // Nothing to do.
    } else if (keypad_fun_number == 5) {
        // Nothing to do.
    } else if (!(scancode & CODE_BREAK)) {
        pr_debug("scancode : %04x\n", scancode);
        // Get the current keymap.
        const keymap_t *keymap = get_keymap(scancode);
        // Get the specific keymap.
        int character = 0;
        if (!bitmask_check(kflags, KBD_LEFT_SHIFT | KBD_RIGHT_SHIFT) != !bitmask_check(kflags, KBD_CAPS_LOCK)) {
            keyboard_push_front(keymap->shift);
        } else if ((get_keymap_type() == KEYMAP_IT) && bitmask_check(kflags, KBD_RIGHT_ALT) && (bitmask_check(kflags, KBD_LEFT_SHIFT | KBD_RIGHT_SHIFT))) {
            keyboard_push_front(keymap->alt);
        } else if (bitmask_check(kflags, KBD_RIGHT_ALT)) {
            keyboard_push_front(keymap->alt);
        } else if (bitmask_check(kflags, KBD_LEFT_CONTROL | KBD_RIGHT_CONTROL)) {
            keyboard_push_front(keymap->ctrl);
        } else {
            keyboard_push_front(keymap->normal);
        }
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

int keyboard_initialize()
{
    // Initialize the ring-buffer for the scancodes.
    fs_rb_scancode_init(&scancodes);
    // Initialize the spinlock.
    spinlock_init(&scancodes_lock);
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
