/// @file keyboard.c
/// @brief Keyboard handling.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.
/// @addtogroup keyboard
/// @{

// Setup the logging for this file (do this before any other include).
#include "sys/kernel_levels.h"           // Include kernel log levels.
#define __DEBUG_HEADER__ "[KEYBRD]"      ///< Change header.
#define __DEBUG_LEVEL__  LOGLEVEL_NOTICE ///< Set log level.
#include "io/debug.h"                    // Include debugging functions.

#include "ctype.h"
#include "descriptor_tables/isr.h"
#include "drivers/keyboard/keyboard.h"
#include "drivers/keyboard/keymap.h"
#include "drivers/ps2.h"
#include "hardware/pic8259.h"
#include "io/port_io.h"
#include "io/video.h"
#include "process/scheduler.h"
#include "ring_buffer.h"
#include "string.h"
#include "sys/bitops.h"

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

/// @brief Pushes a character into the scancode ring buffer.
/// @param c The character to push into the ring buffer.
static inline void keyboard_push_front(unsigned int c)
{
    // Lock the scancode buffer to ensure thread safety during push operation.
    spinlock_lock(&scancodes_lock);

    // Push the character into the front of the scancode buffer.
    fs_rb_scancode_push_front(&scancodes, (int)c);

    // Unlock the buffer after the push operation is complete.
    spinlock_unlock(&scancodes_lock);
}

/// @brief Pushes a sequence of characters (scancodes) into the keyboard buffer.
/// @param sequence A null-terminated string representing the sequence to push.
static inline void keyboard_push_sequence(char *sequence)
{
    // Lock the scancodes ring buffer to ensure thread safety.
    spinlock_lock(&scancodes_lock);

    // Iterate through each character in the sequence and push it to the buffer.
    for (size_t i = 0; i < strlen(sequence); ++i) {
        fs_rb_scancode_push_front(&scancodes, (int)sequence[i]);
    }

    // Unlock the buffer after the operation is complete.
    spinlock_unlock(&scancodes_lock);
}

/// @brief Pops a value from the ring buffer.
/// @return the value we removed from the ring buffer.
int keyboard_pop_back(void)
{
    int c;
    spinlock_lock(&scancodes_lock);
    if (!fs_rb_scancode_empty(&scancodes)) {
        c = fs_rb_scancode_pop_back(&scancodes);
    } else {
        c = -1;
    }
    spinlock_unlock(&scancodes_lock);
    return c;
}

int keyboard_back(void)
{
    int c;
    spinlock_lock(&scancodes_lock);
    if (!fs_rb_scancode_empty(&scancodes)) {
        c = fs_rb_scancode_back(&scancodes);
    } else {
        c = -1;
    }
    spinlock_unlock(&scancodes_lock);
    return c;
}

int keyboard_front(void)
{
    int c = -1;
    spinlock_lock(&scancodes_lock);
    if (!fs_rb_scancode_empty(&scancodes)) {
        c = fs_rb_scancode_front(&scancodes);
    } else {
        c = -1;
    }
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
    scancode = ps2_read_data();
    if (scancode == 0xE0) {
        scancode = (scancode << 8U) | ps2_read_data();
    }

    // Get the keypad number, of num-lock is disabled.
    int keypad_code = !bitmask_check(kflags, KBD_NUM_LOCK) ? scancode : 0;

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
    } else if ((scancode == KEY_ENTER) || (scancode == KEY_KP_RETURN)) {
        keyboard_push_front('\n');
        pr_debug("Press(KEY_ENTER)\n");
    } else if ((scancode == KEY_UP_ARROW) || (keypad_code == KEY_KP8)) {
        pr_debug("Press(KEY_UP_ARROW)\n");
        keyboard_push_sequence("\033[A");
    } else if ((scancode == KEY_DOWN_ARROW) || (keypad_code == KEY_KP2)) {
        pr_debug("Press(KEY_DOWN_ARROW)\n");
        keyboard_push_sequence("\033[B");
    } else if ((scancode == KEY_RIGHT_ARROW) || (keypad_code == KEY_KP6)) {
        pr_debug("Press(KEY_RIGHT_ARROW)\n");
        keyboard_push_sequence("\033[C");
    } else if ((scancode == KEY_LEFT_ARROW) || (keypad_code == KEY_KP4)) {
        pr_debug("Press(KEY_LEFT_ARROW)\n");
        keyboard_push_sequence("\033[D");
    } else if (scancode == KEY_F1) {
        pr_debug("Press(KEY_F1)\n");
        keyboard_push_sequence("\033[11~");
    } else if (scancode == KEY_F2) {
        pr_debug("Press(KEY_F2)\n");
        keyboard_push_sequence("\033[12~");
    } else if (scancode == KEY_F3) {
        pr_debug("Press(KEY_F3)\n");
        keyboard_push_sequence("\033[13~");
    } else if (scancode == KEY_F4) {
        pr_debug("Press(KEY_F4)\n");
        keyboard_push_sequence("\033[14~");
    } else if (scancode == KEY_F5) {
        pr_debug("Press(KEY_F5)\n");
        keyboard_push_sequence("\033[15~");
    } else if (scancode == KEY_F6) {
        pr_debug("Press(KEY_F6)\n");
        keyboard_push_sequence("\033[17~");
    } else if (scancode == KEY_F7) {
        pr_debug("Press(KEY_F7)\n");
        keyboard_push_sequence("\033[18~");
    } else if (scancode == KEY_F8) {
        pr_debug("Press(KEY_F8)\n");
        keyboard_push_sequence("\033[19~");
    } else if (scancode == KEY_F9) {
        pr_debug("Press(KEY_F9)\n");
        keyboard_push_sequence("\033[20~");
    } else if (scancode == KEY_F10) {
        pr_debug("Press(KEY_F10)\n");
        keyboard_push_sequence("\033[21~");
    } else if (scancode == KEY_F11) {
        pr_debug("Press(KEY_F11)\n");
        keyboard_push_sequence("\033[23~");
    } else if (scancode == KEY_F12) {
        pr_debug("Press(KEY_F12)\n");
        keyboard_push_sequence("\033[24~");
    } else if ((scancode == KEY_INSERT) || (keypad_code == KEY_KP0)) {
        pr_debug("Press(KEY_INSERT)\n");
        keyboard_push_sequence("\033[2~");
    } else if ((scancode == KEY_DELETE) || (keypad_code == KEY_KP_DEC)) {
        pr_debug("Press(KEY_DELETE)\n");
        keyboard_push_sequence("\033[3~");
    } else if ((scancode == KEY_HOME) || (keypad_code == KEY_KP7)) {
        pr_debug("Press(KEY_HOME)\n");
        keyboard_push_sequence("\033[1~");
    } else if ((scancode == KEY_END) || (keypad_code == KEY_KP1)) {
        pr_debug("Press(KEY_END)\n");
        keyboard_push_sequence("\033[4~");
    } else if ((scancode == KEY_PAGE_UP) || (keypad_code == KEY_KP9)) {
        pr_debug("Press(KEY_PAGE_UP)\n");
        keyboard_push_sequence("\033[5~");
    } else if ((scancode == KEY_PAGE_DOWN) || (keypad_code == KEY_KP3)) {
        pr_debug("Press(KEY_PAGE_DOWN)\n");
        keyboard_push_sequence("\033[6~");
    } else if (scancode == KEY_ESCAPE) {
        // Nothing to do.
    } else if (keypad_code == KEY_5) {
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

void keyboard_update_leds(void)
{
    // Handle scroll_loc & num_loc & caps_loc.
    bitmask_check(kflags, KBD_SCROLL_LOCK) ? (ledstate |= 1) : (ledstate ^= 1);
    bitmask_check(kflags, KBD_NUM_LOCK) ? (ledstate |= 2) : (ledstate ^= 2);
    bitmask_check(kflags, KBD_CAPS_LOCK) ? (ledstate |= 4) : (ledstate ^= 4);

    // Write on the port.
    outportb(0x60, 0xED);
    outportb(0x60, ledstate);
}

void keyboard_enable(void)
{
    outportb(0x60, 0xF4);
}

void keyboard_disable(void)
{
    outportb(0x60, 0xF5);
}

int keyboard_initialize(void)
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

int keyboard_finalize(void)
{
    // Install the IRQ.
    irq_uninstall_handler(IRQ_KEYBOARD, keyboard_isr);
    // Enable the IRQ.
    pic8259_irq_disable(IRQ_KEYBOARD);
    return 0;
}

/// @}
