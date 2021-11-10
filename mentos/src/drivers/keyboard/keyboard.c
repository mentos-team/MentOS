///                MentOS, The Mentoring Operating system project
/// @file keyboard.c
/// @brief Keyboard handling.
/// @copyright (c) 2014-2021 This file is distributed under the MIT License.
/// See LICENSE.md for details.
/// @addtogroup keyboard
/// @{

#include "drivers/keyboard/keyboard.h"

#include "io/port_io.h"
#include "hardware/pic8259.h"
#include "drivers/keyboard/keymap.h"
#include "sys/bitops.h"
#include "io/video.h"
#include "io/debug.h"
#include "ctype.h"
#include "descriptor_tables/isr.h"
#include "process/scheduler.h"

/// The dimension of the circular buffer used to store video history.
#define BUFSIZE 256

/// A macro from Ivan to update buffer indexes.
#define STEP(x) (((x) == BUFSIZE - 1) ? 0 : ((x) + 1))

/// Circular Buffer where the pressed keys are stored.
static int32_t circular_buffer[BUFSIZE] = { 0 };
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

static inline void push_character(uint16_t c)
{
    // Update buffer.
    if (STEP(buf_w) == buf_r) {
        buf_r = STEP(buf_r);
    }

    circular_buffer[buf_w] = c;

    buf_w = STEP(buf_w);
}

static inline int read_character()
{
    char c = -1;
    if (buf_r != buf_w) {
        c     = circular_buffer[buf_r];
        buf_r = STEP(buf_r);
    }
    return c;
}

void keyboard_isr(pt_regs *f)
{
    (void)f;

    if (!(inportb(0x64U) & 1U)) {
        return;
    }

    // Take scancode from the port.
    uint32_t scancode = inportb(0x60);
    if (scancode == 0xE0) {
        scancode = (scancode << 8U) | inportb(0x60);
    }

    // If the key has just been released.
    if (scancode & 0x80U) {
        if (scancode == (KEY_LEFT_SHIFT | CODE_BREAK)) {
            bitmask_clear_assign(kflags, KBD_LEFT_SHIFT);
        } else if (scancode == (KEY_RIGHT_SHIFT | CODE_BREAK)) {
            bitmask_clear_assign(kflags, KBD_RIGHT_SHIFT);
        } else if (scancode == (KEY_LEFT_CONTROL | CODE_BREAK)) {
            bitmask_clear_assign(kflags, KBD_LEFT_CONTROL);
        } else if (scancode == (KEY_RIGHT_CONTROL | CODE_BREAK)) {
            bitmask_clear_assign(kflags, KBD_RIGHT_CONTROL);
        } else if (scancode == (KEY_LEFT_ALT | CODE_BREAK)) {
            bitmask_clear_assign(kflags, KBD_LEFT_ALT);
        } else if (scancode == (KEY_RIGHT_ALT | CODE_BREAK)) {
            bitmask_clear_assign(kflags, KBD_RIGHT_ALT);
        }
    } else {
        int32_t character = 0;

        // Parse the key.
        switch (scancode) {
        case KEY_LEFT_SHIFT:
            bitmask_set_assign(kflags, KBD_LEFT_SHIFT);
            break;
        case KEY_RIGHT_SHIFT:
            bitmask_set_assign(kflags, KBD_RIGHT_SHIFT);
            break;
        case KEY_LEFT_CONTROL:
            bitmask_set_assign(kflags, KBD_LEFT_CONTROL);
            break;
        case KEY_RIGHT_CONTROL:
            bitmask_set_assign(kflags, KBD_RIGHT_CONTROL);
            break;
        case KEY_CAPS_LOCK:
            bitmask_flip_assign(kflags, KBD_CAPS_LOCK);
            keyboard_update_leds();
            break;
        case KEY_NUM_LOCK:
            bitmask_flip_assign(kflags, KBD_NUM_LOCK);
            keyboard_update_leds();
            break;
        case KEY_SCROLL_LOCK:
            bitmask_flip_assign(kflags, KBD_SCROLL_LOCK);
            keyboard_update_leds();
            break;
        case KEY_PAGE_UP:
            video_shift_one_page_down();
            break;
        case KEY_PAGE_DOWN:
            video_shift_one_page_up();
            break;
        case KEY_ESCAPE:
            break;
        case KEY_LEFT_ALT:
            bitmask_set_assign(kflags, KBD_LEFT_ALT);
            push_character(scancode << 16u);
            break;
        case KEY_RIGHT_ALT:
            bitmask_set_assign(kflags, KBD_RIGHT_ALT);
            push_character(scancode << 16u);
            break;
        case KEY_BACKSPACE:
            push_character('\b');
            break;
        case KEY_DELETE:
            push_character(127);
            break;
        case KEY_KP_RETURN:
        case KEY_ENTER:
            push_character('\n');
            break;
        case KEY_UP_ARROW:
            push_character('\033');
            push_character('[');
            push_character('A');
            break;
        case KEY_DOWN_ARROW:
            push_character('\033');
            push_character('[');
            push_character('B');
            break;
        case KEY_RIGHT_ARROW:
            push_character('\033');
            push_character('[');
            push_character('C');
            break;
        case KEY_LEFT_ARROW:
            push_character('\033');
            push_character('[');
            push_character('D');
            break;
        case KEY_HOME:
            push_character('\033');
            push_character('[');
            push_character('H');
            break;
        case KEY_END:
            push_character('\033');
            push_character('[');
            push_character('F');
            break;
        default: {
            // Get the current keymap.
            const keymap_t *keymap = get_keymap();

            if (bitmask_check(kflags, KBD_NUM_LOCK)) {
                character = keymap->numlock[scancode];
            }
            if ((character <= 0) && bitmask_check(kflags, KBD_RIGHT_ALT) && (bitmask_check(kflags, KBD_LEFT_SHIFT | KBD_RIGHT_SHIFT))) {
                if (scancode == KEY_LEFT_BRAKET) {
                    character = '{';
                } else if (scancode == KEY_RIGHT_BRAKET) {
                    character = '}';
                }
            }
            if ((character <= 0) && bitmask_check(kflags, KBD_RIGHT_ALT)) {
                if (scancode == KEY_LEFT_BRAKET) {
                    character = '[';
                } else if (scancode == KEY_RIGHT_BRAKET) {
                    character = ']';
                } else if (scancode == 0x27) {
                    character = '@';
                } else if (scancode == 0x28) {
                    character = '#';
                }
            }
            if ((character <= 0) && (bitmask_check(kflags, KBD_LEFT_SHIFT | KBD_RIGHT_SHIFT))) {
                character = keymap->shift[scancode];
            }
            if (character <= 0) {
                character = keymap->base[scancode];
            }
            // We have failed to retrieve the character.
            if (character <= 0) {
                break;
            }
            // Apply caps lock modifier.
            if (bitmask_check(kflags, KBD_CAPS_LOCK)) {
                if (character >= 'a' && character <= 'z') {
                    character += 'A' - 'a';
                } else if (character >= 'A' && character <= 'Z') {
                    character += 'a' - 'A';
                }
            }
            if (bitmask_check(kflags, KBD_LEFT_CONTROL | KBD_RIGHT_CONTROL)) {
                push_character('\033');
                push_character('^');
                character = toupper(character);

                // Qui ci arrivi quando stai tenendo premuto ctrl+<carattere>.
                // Qui devi scatenare il SIGINT (ctrl+c).

                // signal();
            }
            // Update buffer.
            push_character(character);
        }
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