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
static uint32_t kflags  = 0;
/// Where we store the keypress.
rb_keybuffer_t scancodes;
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

/// Define variable to switch between normal mode sequences and xterm sequences.
#define USE_XTERM_SEQUENCES 0

#define SEQ_UP_ARROW         "\033[A"    ///< Escape sequence for the Up Arrow key.
#define SEQ_DOWN_ARROW       "\033[B"    ///< Escape sequence for the Down Arrow key.
#define SEQ_RIGHT_ARROW      "\033[C"    ///< Escape sequence for the Right Arrow key.
#define SEQ_LEFT_ARROW       "\033[D"    ///< Escape sequence for the Left Arrow key.
#define SEQ_CTRL_UP_ARROW    "\033[1;5A" ///< Escape sequence for Ctrl + Up Arrow.
#define SEQ_CTRL_DOWN_ARROW  "\033[1;5B" ///< Escape sequence for Ctrl + Down Arrow.
#define SEQ_CTRL_RIGHT_ARROW "\033[1;5C" ///< Escape sequence for Ctrl + Right Arrow.
#define SEQ_CTRL_LEFT_ARROW  "\033[1;5D" ///< Escape sequence for Ctrl + Left Arrow.
#define SEQ_INSERT           "\033[2~"   ///< Escape sequence for the Insert key.
#define SEQ_DELETE           "\033[3~"   ///< Escape sequence for the Delete key.
#define SEQ_PAGE_UP          "\033[5~"   ///< Escape sequence for the Page Up key.
#define SEQ_PAGE_DOWN        "\033[6~"   ///< Escape sequence for the Page Down key.
#define SEQ_F1               "\033OP"    ///< Escape sequence for the F1 key.
#define SEQ_F2               "\033OQ"    ///< Escape sequence for the F2 key.
#define SEQ_F3               "\033OR"    ///< Escape sequence for the F3 key.
#define SEQ_F4               "\033OS"    ///< Escape sequence for the F4 key.
#define SEQ_F5               "\033[15~"  ///< Escape sequence for the F5 key.
#define SEQ_F6               "\033[17~"  ///< Escape sequence for the F6 key.
#define SEQ_F7               "\033[18~"  ///< Escape sequence for the F7 key.
#define SEQ_F8               "\033[19~"  ///< Escape sequence for the F8 key.
#define SEQ_F9               "\033[20~"  ///< Escape sequence for the F9 key.
#define SEQ_F10              "\033[21~"  ///< Escape sequence for the F10 key.
#define SEQ_F11              "\033[23~"  ///< Escape sequence for the F11 key.
#define SEQ_F12              "\033[24~"  ///< Escape sequence for the F12 key.

#if USE_XTERM_SEQUENCES
#define SEQ_HOME "\033[1~" ///< Escape sequence for the Home key in xterm.
#define SEQ_END  "\033[4~" ///< Escape sequence for the End key in xterm.
#else
#define SEQ_HOME "\033[H" ///< Escape sequence for the Home key.
#define SEQ_END  "\033[F" ///< Escape sequence for the End key.
#endif

/// @brief Pushes a character into the scancode ring buffer.
/// @param c The character to push into the ring buffer.
static inline void keyboard_push_back(unsigned int c)
{
    // Lock the scancode buffer to ensure thread safety during push operation.
    spinlock_lock(&scancodes_lock);

    // Push the character into the front of the scancode buffer.
    rb_keybuffer_push_back(&scancodes, (int)c);

    // Unlock the buffer after the push operation is complete.
    spinlock_unlock(&scancodes_lock);
}

/// @brief Pushes a character into the scancode ring buffer.
/// @param c The character to push into the ring buffer.
static inline void keyboard_push_front(unsigned int c)
{
    // Lock the scancode buffer to ensure thread safety during push operation.
    spinlock_lock(&scancodes_lock);

    // Push the character into the front of the scancode buffer.
    rb_keybuffer_push_front(&scancodes, (int)c);

    // Unlock the buffer after the push operation is complete.
    spinlock_unlock(&scancodes_lock);
}

/// @brief Pushes a sequence of characters (scancodes) into the keyboard buffer.
/// @param sequence A null-terminated string representing the sequence to push.
static inline void keyboard_push_back_sequence(char *sequence)
{
    // Lock the scancodes ring buffer to ensure thread safety.
    spinlock_lock(&scancodes_lock);

    // Iterate through each character in the sequence and push it to the buffer.
    for (size_t i = 0; i < strlen(sequence); ++i) {
        rb_keybuffer_push_back(&scancodes, (int)sequence[i]);
    }

    // Unlock the buffer after the operation is complete.
    spinlock_unlock(&scancodes_lock);
}

/// @brief Pushes a sequence of characters (scancodes) into the keyboard buffer.
/// @param sequence A null-terminated string representing the sequence to push.
static inline void keyboard_push_front_sequence(char *sequence)
{
    // Lock the scancodes ring buffer to ensure thread safety.
    spinlock_lock(&scancodes_lock);

    // Iterate through each character in the sequence and push it to the buffer.
    for (size_t i = 0; i < strlen(sequence); ++i) {
        rb_keybuffer_push_front(&scancodes, (int)sequence[i]);
    }

    // Unlock the buffer after the operation is complete.
    spinlock_unlock(&scancodes_lock);
}

/// @brief Pops a value from the ring buffer.
/// @return the value we removed from the ring buffer.
int keyboard_pop_back(void)
{
    spinlock_lock(&scancodes_lock);
    int c = rb_keybuffer_pop_back(&scancodes);
    spinlock_unlock(&scancodes_lock);
    return c;
}

int keyboard_peek_back(void)
{
    spinlock_lock(&scancodes_lock);
    int c = rb_keybuffer_peek_back(&scancodes);
    spinlock_unlock(&scancodes_lock);
    return c;
}

int keyboard_peek_front(void)
{
    spinlock_lock(&scancodes_lock);
    int c = rb_keybuffer_peek_front(&scancodes);
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

    int ctrl_pressed      = (kflags & (KBD_LEFT_CONTROL | KBD_RIGHT_CONTROL)) != 0;
    int shift_pressed     = (kflags & (KBD_LEFT_SHIFT | KBD_RIGHT_SHIFT)) != 0;
    int right_alt_pressed = (kflags & KBD_RIGHT_ALT) != 0;
    int caps_lock_pressed = (kflags & KBD_CAPS_LOCK) != 0;

    // If the key has just been released.
    if (scancode == KEY_LEFT_SHIFT) {
        pr_debug("Press(KBD_LEFT_SHIFT)\n");
        bitmask_set_assign(kflags, KBD_LEFT_SHIFT);

    } else if (scancode == KEY_RIGHT_SHIFT) {
        pr_debug("Press(KBD_RIGHT_SHIFT)\n");
        bitmask_set_assign(kflags, KBD_RIGHT_SHIFT);

    } else if (scancode == KEY_LEFT_CONTROL) {
        pr_debug("Press(KBD_LEFT_CONTROL)\n");
        bitmask_set_assign(kflags, KBD_LEFT_CONTROL);

    } else if (scancode == KEY_RIGHT_CONTROL) {
        pr_debug("Press(KBD_RIGHT_CONTROL)\n");
        bitmask_set_assign(kflags, KBD_RIGHT_CONTROL);

    } else if (scancode == KEY_LEFT_ALT) {
        pr_debug("Press(KBD_LEFT_ALT)\n");
        bitmask_set_assign(kflags, KBD_LEFT_ALT);
        keyboard_push_front(scancode << 16U);

    } else if (scancode == KEY_RIGHT_ALT) {
        pr_debug("Press(KBD_RIGHT_ALT)\n");
        bitmask_set_assign(kflags, KBD_RIGHT_ALT);
        keyboard_push_front(scancode << 16U);

    } else if (scancode == (KEY_LEFT_SHIFT | CODE_BREAK)) {
        pr_debug("Release(KBD_LEFT_SHIFT)\n");
        bitmask_clear_assign(kflags, KBD_LEFT_SHIFT);

    } else if (scancode == (KEY_RIGHT_SHIFT | CODE_BREAK)) {
        pr_debug("Release(KBD_RIGHT_SHIFT)\n");
        bitmask_clear_assign(kflags, KBD_RIGHT_SHIFT);

    } else if (scancode == (KEY_LEFT_CONTROL | CODE_BREAK)) {
        pr_debug("Release(KBD_LEFT_CONTROL)\n");
        bitmask_clear_assign(kflags, KBD_LEFT_CONTROL);

    } else if (scancode == (KEY_RIGHT_CONTROL | CODE_BREAK)) {
        pr_debug("Release(KBD_RIGHT_CONTROL)\n");
        bitmask_clear_assign(kflags, KBD_RIGHT_CONTROL);

    } else if (scancode == (KEY_LEFT_ALT | CODE_BREAK)) {
        pr_debug("Release(KBD_LEFT_ALT)\n");
        bitmask_clear_assign(kflags, KBD_LEFT_ALT);

    } else if (scancode == (KEY_RIGHT_ALT | CODE_BREAK)) {
        pr_debug("Release(KBD_RIGHT_ALT)\n");
        bitmask_clear_assign(kflags, KBD_RIGHT_ALT);

    } else if (scancode == KEY_CAPS_LOCK) {
        pr_debug("Toggle(KBD_CAPS_LOCK)\n");
        bitmask_flip_assign(kflags, KBD_CAPS_LOCK);
        keyboard_update_leds();

    } else if (scancode == KEY_NUM_LOCK) {
        pr_debug("Toggle(KBD_NUM_LOCK)\n");
        bitmask_flip_assign(kflags, KBD_NUM_LOCK);
        keyboard_update_leds();

    } else if (scancode == KEY_SCROLL_LOCK) {
        pr_debug("Toggle(KBD_SCROLL_LOCK)\n");
        bitmask_flip_assign(kflags, KBD_SCROLL_LOCK);
        keyboard_update_leds();

    } else if (scancode == KEY_BACKSPACE) {
        pr_debug("Press(KEY_BACKSPACE)\n");
        keyboard_push_front('\b');

    } else if ((scancode == KEY_ENTER) || (scancode == KEY_KP_RETURN)) {
        pr_debug("Press(KEY_ENTER)\n");
        keyboard_push_front('\n');

    } else if (ctrl_pressed && ((scancode == KEY_UP_ARROW) || (keypad_code == KEY_KP8))) {
        pr_debug("Press(Ctrl + KEY_UP_ARROW)\n");
        keyboard_push_front_sequence(SEQ_CTRL_UP_ARROW);

    } else if (ctrl_pressed && ((scancode == KEY_DOWN_ARROW) || (keypad_code == KEY_KP2))) {
        pr_debug("Press(Ctrl + KEY_DOWN_ARROW)\n");
        keyboard_push_front_sequence(SEQ_CTRL_DOWN_ARROW);

    } else if (ctrl_pressed && ((scancode == KEY_RIGHT_ARROW) || (keypad_code == KEY_KP6))) {
        pr_debug("Press(Ctrl + KEY_RIGHT_ARROW)\n");
        keyboard_push_front_sequence(SEQ_CTRL_RIGHT_ARROW);

    } else if (ctrl_pressed && ((scancode == KEY_LEFT_ARROW) || (keypad_code == KEY_KP4))) {
        pr_debug("Press(Ctrl + KEY_LEFT_ARROW)\n");
        keyboard_push_front_sequence(SEQ_CTRL_LEFT_ARROW);

    } else if ((scancode == KEY_UP_ARROW) || (keypad_code == KEY_KP8)) {
        pr_debug("Press(KEY_UP_ARROW)\n");
        keyboard_push_front_sequence(SEQ_UP_ARROW);

    } else if ((scancode == KEY_DOWN_ARROW) || (keypad_code == KEY_KP2)) {
        pr_debug("Press(KEY_DOWN_ARROW)\n");
        keyboard_push_front_sequence(SEQ_DOWN_ARROW);

    } else if ((scancode == KEY_RIGHT_ARROW) || (keypad_code == KEY_KP6)) {
        pr_debug("Press(KEY_RIGHT_ARROW)\n");
        keyboard_push_front_sequence(SEQ_RIGHT_ARROW);

    } else if ((scancode == KEY_LEFT_ARROW) || (keypad_code == KEY_KP4)) {
        pr_debug("Press(KEY_LEFT_ARROW)\n");
        keyboard_push_front_sequence(SEQ_LEFT_ARROW);

    } else if (scancode == KEY_F1) {
        pr_debug("Press(KEY_F1)\n");
        keyboard_push_front_sequence(SEQ_F1);

    } else if (scancode == KEY_F2) {
        pr_debug("Press(KEY_F2)\n");
        keyboard_push_front_sequence(SEQ_F2);

    } else if (scancode == KEY_F3) {
        pr_debug("Press(KEY_F3)\n");
        keyboard_push_front_sequence(SEQ_F3);

    } else if (scancode == KEY_F4) {
        pr_debug("Press(KEY_F4)\n");
        keyboard_push_front_sequence(SEQ_F4);

    } else if (scancode == KEY_F5) {
        pr_debug("Press(KEY_F5)\n");
        keyboard_push_front_sequence(SEQ_F5);

    } else if (scancode == KEY_F6) {
        pr_debug("Press(KEY_F6)\n");
        keyboard_push_front_sequence(SEQ_F6);

    } else if (scancode == KEY_F7) {
        pr_debug("Press(KEY_F7)\n");
        keyboard_push_front_sequence(SEQ_F7);

    } else if (scancode == KEY_F8) {
        pr_debug("Press(KEY_F8)\n");
        keyboard_push_front_sequence(SEQ_F8);

    } else if (scancode == KEY_F9) {
        pr_debug("Press(KEY_F9)\n");
        keyboard_push_front_sequence(SEQ_F9);

    } else if (scancode == KEY_F10) {
        pr_debug("Press(KEY_F10)\n");
        keyboard_push_front_sequence(SEQ_F10);

    } else if (scancode == KEY_F11) {
        pr_debug("Press(KEY_F11)\n");
        keyboard_push_front_sequence(SEQ_F11);

    } else if (scancode == KEY_F12) {
        pr_debug("Press(KEY_F12)\n");
        keyboard_push_front_sequence(SEQ_F12);

    } else if ((scancode == KEY_INSERT) || (keypad_code == KEY_KP0)) {
        pr_debug("Press(KEY_INSERT)\n");
        keyboard_push_front_sequence(SEQ_INSERT);

    } else if ((scancode == KEY_DELETE) || (keypad_code == KEY_KP_DEC)) {
        pr_debug("Press(KEY_DELETE)\n");
        keyboard_push_front_sequence(SEQ_DELETE);

    } else if ((scancode == KEY_HOME) || (keypad_code == KEY_KP7)) {
        pr_debug("Press(KEY_HOME)\n");
        keyboard_push_front_sequence(SEQ_HOME);

    } else if ((scancode == KEY_END) || (keypad_code == KEY_KP1)) {
        pr_debug("Press(KEY_END)\n");
        keyboard_push_front_sequence(SEQ_END);

    } else if ((scancode == KEY_PAGE_UP) || (keypad_code == KEY_KP9)) {
        pr_debug("Press(KEY_PAGE_UP)\n");
        keyboard_push_front_sequence(SEQ_PAGE_UP);

    } else if ((scancode == KEY_PAGE_DOWN) || (keypad_code == KEY_KP3)) {
        pr_debug("Press(KEY_PAGE_DOWN)\n");
        keyboard_push_front_sequence(SEQ_PAGE_DOWN);

    } else if (scancode == KEY_ESCAPE) {
        // Nothing to do.

    } else if (!(scancode & CODE_BREAK)) {
        // Get the current keymap.
        const keymap_t *keymap = get_keymap(scancode);
        pr_debug("scancode : %04x (%c)\n", scancode, keymap->normal);
        // Get the specific keymap.
        if (!shift_pressed != !caps_lock_pressed) {
            keyboard_push_front(keymap->shift);
        } else if ((get_keymap_type() == KEYMAP_IT) && right_alt_pressed && shift_pressed) {
            keyboard_push_front(keymap->alt);
        } else if (right_alt_pressed) {
            keyboard_push_front(keymap->alt);
        } else if (ctrl_pressed) {
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

void keyboard_enable(void) { outportb(0x60, 0xF4); }

void keyboard_disable(void) { outportb(0x60, 0xF5); }

int keyboard_initialize(void)
{
    // Initialize the ring-buffer for the scancodes.
    rb_keybuffer_init(&scancodes);
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
