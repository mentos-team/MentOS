/// @file test_video.c
/// @brief Characterization tests for the generic console layer.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.
///
/// These tests pin down the *observable* behaviour of the console as it exists
/// today, so that a refactor of the video layer can be proven not to change it.
/// They deliberately use nothing but the public `video_*` API, which makes them
/// independent of the underlying backend: the same assertions must hold for the
/// VGA text backend and for any graphical backend.
///
/// The public API exposes no way to read screen *content*, so what these tests
/// can pin is the cursor/geometry contract. Rendered content is covered
/// separately, by comparing a QEMU screendump against a stored pixel baseline.
///
/// Several assertions below encode long-standing quirks rather than desirable
/// behaviour. They are marked QUIRK and must keep passing unchanged: fixing
/// them is a separate decision, and a silent change would be a regression.

// Setup the logging for this file (do this before any other include).
#include "sys/kernel_levels.h"           // Include kernel log levels.
#define __DEBUG_HEADER__ "[TUNIT ]"      ///< Change header.
#define __DEBUG_LEVEL__  LOGLEVEL_NOTICE ///< Set log level.
#include "io/debug.h"                    // Include debugging functions.

#include "io/video.h"
#include "tests/test.h"
#include "tests/test_utils.h"

/// @brief Asserts that the cursor sits exactly at the expected position.
/// @param expected_column The expected column.
/// @param expected_row The expected row.
/// @param what Description of the operation under test.
static inline void __assert_cursor(unsigned expected_column, unsigned expected_row, const char *what)
{
    unsigned column = (unsigned)-1;
    unsigned row    = (unsigned)-1;
    video_get_cursor_position(&column, &row);
    if ((column != expected_column) || (row != expected_row)) {
        pr_emerg(
            "Cursor mismatch after %s: expected (%u,%u), got (%u,%u)\n", what, expected_column, expected_row, column,
            row);
        kernel_panic("Test failure");
    }
}

/// @brief The screen geometry must be reported consistently.
TEST(video_geometry)
{
    TEST_SECTION_START("console geometry is reported");

    unsigned width  = 0;
    unsigned height = 0;
    video_get_screen_size(&width, &height);
    ASSERT_MSG(width > 0, "Screen width must be non-zero");
    ASSERT_MSG(height > 0, "Screen height must be non-zero");

    // Partial queries must be honoured without touching the other output.
    unsigned only_width          = 0;
    unsigned untouched           = 0xABCDU;
    video_get_screen_size(&only_width, NULL);
    ASSERT_MSG(only_width == width, "Querying width alone must agree");
    ASSERT_MSG(untouched == 0xABCDU, "Querying width alone must not write height");

    unsigned only_height = 0;
    video_get_screen_size(NULL, &only_height);
    ASSERT_MSG(only_height == height, "Querying height alone must agree");

    TEST_SECTION_END();
}

/// @brief Absolute cursor placement, including out-of-range clamping.
TEST(video_move_cursor)
{
    TEST_SECTION_START("absolute cursor placement and clamping");

    unsigned width  = 0;
    unsigned height = 0;
    video_get_screen_size(&width, &height);

    video_move_cursor(0, 0);
    __assert_cursor(0, 0, "video_move_cursor(0,0)");

    video_move_cursor(10, 5);
    __assert_cursor(10, 5, "video_move_cursor(10,5)");

    // Out-of-range coordinates clamp to the last cell, they do not wrap.
    video_move_cursor(width + 100, height + 100);
    __assert_cursor(width - 1, height - 1, "video_move_cursor(overflow)");

    // Partial queries must be honoured.
    video_move_cursor(3, 7);
    unsigned row_only = (unsigned)-1;
    video_get_cursor_position(NULL, &row_only);
    ASSERT_MSG(row_only == 7, "Querying the row alone must agree");

    TEST_SECTION_END();
}

/// @brief Printable characters advance the cursor and wrap at the line end.
TEST(video_putc_advance)
{
    TEST_SECTION_START("printable characters advance and wrap");

    unsigned width = 0;
    video_get_screen_size(&width, NULL);

    video_move_cursor(0, 0);
    video_putc('A');
    __assert_cursor(1, 0, "putc at the line start");

    // Writing in the last column moves to the start of the following line.
    video_move_cursor(width - 1, 0);
    video_putc('B');
    __assert_cursor(0, 1, "putc in the last column");

    // A string advances by exactly its printable length.
    video_move_cursor(0, 2);
    video_puts("hello");
    __assert_cursor(5, 2, "video_puts(\"hello\")");

    TEST_SECTION_END();
}

/// @brief Newline, carriage return, backspace and delete.
TEST(video_control_characters)
{
    TEST_SECTION_START("control character handling");

    unsigned width  = 0;
    unsigned height = 0;
    video_get_screen_size(&width, &height);

    video_move_cursor(5, 3);
    video_putc('\n');
    __assert_cursor(0, 4, "putc('\\n')");

    video_move_cursor(5, 3);
    video_putc('\r');
    __assert_cursor(0, 3, "putc('\\r')");

    video_move_cursor(5, 3);
    video_putc('\b');
    __assert_cursor(4, 3, "putc('\\b')");

    // Backspace at the very start of the screen is a no-op, not a wrap.
    video_move_cursor(0, 0);
    video_putc('\b');
    __assert_cursor(0, 0, "putc('\\b') at home");

    // DEL deletes in place: it shifts the line left but leaves the cursor put.
    video_move_cursor(5, 3);
    video_putc(127);
    __assert_cursor(5, 3, "putc(127)");

    // A newline on the last row scrolls and keeps the cursor on the last row.
    video_move_cursor(5, height - 1);
    video_putc('\n');
    __assert_cursor(0, height - 1, "putc('\\n') on the last row");

    TEST_SECTION_END();
}

/// @brief Control characters the console deliberately ignores.
///
/// Tab expansion and control-character echoing live in the terminal layer
/// (proc_video.c), not here. The console drops them silently and, notably,
/// without moving the cursor.
TEST(video_ignored_characters)
{
    TEST_SECTION_START("ignored control characters");

    video_move_cursor(5, 5);
    video_putc('\t');
    __assert_cursor(5, 5, "putc('\\t')");

    video_move_cursor(5, 5);
    video_putc(0x01);
    __assert_cursor(5, 5, "putc(0x01)");

    video_move_cursor(5, 5);
    video_putc(0x7F + 1);
    __assert_cursor(5, 5, "putc(0x80)");

    TEST_SECTION_END();
}

/// @brief Relative cursor movement escape sequences.
TEST(video_escape_relative_moves)
{
    TEST_SECTION_START("ESC [ A/B/C/D relative moves");

    unsigned width  = 0;
    unsigned height = 0;
    video_get_screen_size(&width, &height);

    video_move_cursor(0, 0);
    video_puts("\033[5C");
    __assert_cursor(5, 0, "ESC [ 5 C");

    // A missing or zero parameter counts as one for relative moves.
    video_move_cursor(0, 0);
    video_puts("\033[C");
    __assert_cursor(1, 0, "ESC [ C");

    video_move_cursor(0, 0);
    video_puts("\033[0C");
    __assert_cursor(1, 0, "ESC [ 0 C");

    video_move_cursor(10, 0);
    video_puts("\033[3D");
    __assert_cursor(7, 0, "ESC [ 3 D");

    video_move_cursor(0, 5);
    video_puts("\033[2A");
    __assert_cursor(0, 3, "ESC [ 2 A");

    // Vertical moves saturate at the screen edges.
    video_move_cursor(0, 0);
    video_puts("\033[5A");
    __assert_cursor(0, 0, "ESC [ 5 A at the top");

    video_move_cursor(0, 5);
    video_puts("\033[2B");
    __assert_cursor(0, 7, "ESC [ 2 B");

    video_move_cursor(0, height - 1);
    video_puts("\033[5B");
    __assert_cursor(0, height - 1, "ESC [ 5 B at the bottom");

    TEST_SECTION_END();
}

/// @brief Absolute positioning escape sequences.
TEST(video_escape_absolute_moves)
{
    TEST_SECTION_START("ESC [ H / f absolute moves");

    unsigned width  = 0;
    unsigned height = 0;
    video_get_screen_size(&width, &height);

    // Parameters are one-based, so row 10 column 20 is (19, 9).
    video_puts("\033[10;20H");
    __assert_cursor(19, 9, "ESC [ 10 ; 20 H");

    video_puts("\033[4;2f");
    __assert_cursor(1, 3, "ESC [ 4 ; 2 f");

    // Without parameters the cursor goes home.
    video_move_cursor(5, 5);
    video_puts("\033[H");
    __assert_cursor(0, 0, "ESC [ H");

    // QUIRK 7: a zero parameter underflows the one-based conversion and is then
    // caught by the upper clamp, so ESC [ 0 ; 0 H lands on the LAST cell rather
    // than at home. Preserved deliberately.
    video_puts("\033[0;0H");
    __assert_cursor(width - 1, height - 1, "ESC [ 0 ; 0 H (QUIRK 7)");

    // Out-of-range parameters clamp.
    video_puts("\033[999;999H");
    __assert_cursor(width - 1, height - 1, "ESC [ 999 ; 999 H");

    TEST_SECTION_END();
}

/// @brief QUIRK 5: the cursor may come to rest one cell past the screen.
///
/// Moving forward off the bottom-right corner leaves the cursor in a position
/// that is out of range, and the position query then reports (0,0) rather than
/// the last cell. Preserved deliberately.
TEST(video_cursor_past_end)
{
    TEST_SECTION_START("cursor one past the end (QUIRK 5)");

    unsigned width  = 0;
    unsigned height = 0;
    video_get_screen_size(&width, &height);

    video_move_cursor(width - 1, height - 1);
    video_puts("\033[5C");
    __assert_cursor(0, 0, "ESC [ 5 C off the last cell (QUIRK 5)");

    // The console must recover normally from that state.
    video_move_cursor(2, 2);
    __assert_cursor(2, 2, "recovery after QUIRK 5");

    TEST_SECTION_END();
}

/// @brief Erase sequences and their (non-)effect on the cursor.
TEST(video_escape_erase)
{
    TEST_SECTION_START("ESC [ J / K erase sequences");

    // Erase-in-display mode 2 is the only erase that homes the cursor.
    video_move_cursor(5, 5);
    video_puts("\033[2J");
    __assert_cursor(0, 0, "ESC [ 2 J");

    // Modes 0 and 1 erase relative to the cursor and leave it in place.
    video_move_cursor(5, 5);
    video_puts("\033[0J");
    __assert_cursor(5, 5, "ESC [ 0 J");

    video_move_cursor(5, 5);
    video_puts("\033[1J");
    __assert_cursor(5, 5, "ESC [ 1 J");

    // Erase-in-line never moves the cursor, in any mode.
    video_move_cursor(5, 5);
    video_puts("\033[K");
    __assert_cursor(5, 5, "ESC [ K");

    video_move_cursor(5, 5);
    video_puts("\033[1K");
    __assert_cursor(5, 5, "ESC [ 1 K");

    video_move_cursor(5, 5);
    video_puts("\033[2K");
    __assert_cursor(5, 5, "ESC [ 2 K");

    TEST_SECTION_END();
}

/// @brief Save/restore, colour and cursor-shape sequences.
TEST(video_escape_state)
{
    TEST_SECTION_START("ESC [ s/u, ESC [ m, ESC [ q");

    // Save and restore the cursor position.
    video_move_cursor(7, 3);
    video_puts("\033[s");
    video_move_cursor(1, 1);
    video_puts("\033[u");
    __assert_cursor(7, 3, "ESC [ s then ESC [ u");

    // Selecting attributes must never move the cursor.
    video_move_cursor(5, 5);
    video_puts("\033[31m");
    __assert_cursor(5, 5, "ESC [ 31 m");

    video_move_cursor(5, 5);
    video_puts("\033[1;33;44m");
    __assert_cursor(5, 5, "ESC [ 1 ; 33 ; 44 m");

    video_move_cursor(5, 5);
    video_puts("\033[m");
    __assert_cursor(5, 5, "ESC [ m");

    // Cursor shape selection must never move the cursor.
    for (int shape = 0; shape <= 6; ++shape) {
        char sequence[8] = {'\033', '[', (char)('0' + shape), 'q', 0};
        video_move_cursor(5, 5);
        video_puts(sequence);
        __assert_cursor(5, 5, "ESC [ n q");
    }

    // Reset attributes so later output is not tinted by this test.
    video_puts("\033[0m");

    TEST_SECTION_END();
}

/// @brief The escape parser recovers from unknown and malformed sequences.
TEST(video_escape_parser_recovery)
{
    TEST_SECTION_START("escape parser recovery");

    // An unrecognised single-character escape is dropped, and the parser must
    // return to normal so that the following character prints.
    video_move_cursor(5, 5);
    video_puts("\033Z");
    __assert_cursor(5, 5, "ESC Z");
    video_putc('A');
    __assert_cursor(6, 5, "printable after ESC Z");

    // An unrecognised CSI final byte is dropped without moving the cursor.
    video_move_cursor(5, 5);
    video_puts("\033[9Z");
    __assert_cursor(5, 5, "ESC [ 9 Z");
    video_putc('A');
    __assert_cursor(6, 5, "printable after ESC [ 9 Z");

    // A parameter far beyond the screen must not derail the parser: wherever it
    // leaves the cursor, the console has to stay usable afterwards.
    video_move_cursor(5, 5);
    video_puts("\033[111111111111111111111111C");
    video_move_cursor(3, 4);
    video_putc('A');
    __assert_cursor(4, 4, "printable after an overlong parameter");

    TEST_SECTION_END();
}

/// @brief Scroll sequences, and the unscroll-before-draw behaviour.
TEST(video_escape_scroll)
{
    TEST_SECTION_START("ESC [ S / T scroll sequences");

    // QUIRK 14: ESC [ S and ESC [ T are inverted with respect to their letters,
    // and a missing parameter means zero lines rather than one. A bare ESC [ S
    // is therefore a no-op. Preserved deliberately.
    video_move_cursor(5, 5);
    video_puts("\033[S");
    __assert_cursor(5, 5, "ESC [ S (QUIRK 14)");

    video_move_cursor(5, 5);
    video_puts("\033[T");
    __assert_cursor(5, 5, "ESC [ T (QUIRK 14)");

    // Scrolling never moves the cursor by itself.
    video_move_cursor(0, 10);
    video_puts("\033[1S");
    __assert_cursor(0, 10, "ESC [ 1 S");

    // Drawing while scrolled back returns to the live view first, and the
    // character still lands under the cursor.
    video_putc('X');
    __assert_cursor(1, 10, "printable while scrolled back");

    TEST_SECTION_END();
}

/// @brief Full reset returns the cursor home.
TEST(video_reset)
{
    TEST_SECTION_START("ESC c full reset");

    video_move_cursor(5, 5);
    video_puts("\033c");
    __assert_cursor(0, 0, "ESC c");

    TEST_SECTION_END();
}

/// @brief Main test function for the console subsystem.
/// This function runs all console characterization tests in sequence.
void test_video(void)
{
    test_video_geometry();
    test_video_move_cursor();
    test_video_putc_advance();
    test_video_control_characters();
    test_video_ignored_characters();
    test_video_escape_relative_moves();
    test_video_escape_absolute_moves();
    test_video_cursor_past_end();
    test_video_escape_erase();
    test_video_escape_state();
    test_video_escape_parser_recovery();
    test_video_escape_scroll();
    test_video_reset();

    // Leave a clean screen behind: the tests above scribbled all over it.
    video_clear();
    video_move_cursor(0, 0);
}
