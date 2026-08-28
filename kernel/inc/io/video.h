/// @file video.h
/// @brief Video functions and costants.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "io/ansi_colors.h"
#include "stdint.h"

/// @brief Initialize the video.
void video_init(void);

/// @brief Finishes video initialization once memory management is up.
///
/// Called from kmain() after paging_init() has succeeded. Most backends have
/// nothing to do here and this returns immediately.
///
/// It exists for a backend whose hardware is out of reach at video_init() time
/// -- a linear framebuffer in a high PCI BAR, which nothing has mapped yet and
/// which there is no allocator to map. Such a backend stays inert until this
/// runs; the console keeps recording into its cell buffer meanwhile, and this
/// repaints all of it once the backend reports itself ready, so nothing printed
/// in between is lost.
void video_late_init(void);

/// @brief Records that the console should change shape.
/// @param columns The wanted width in cells.
/// @param rows The wanted height in cells.
///
/// Safe to call from an interrupt handler: it validates the request cheaply and
/// stores it, and does nothing else. No allocation, no device traffic, no
/// migration. Repeated requests coalesce -- only the most recent one survives,
/// which is what makes a burst of display-change events cost one resize.
///
/// The work happens in video_service_pending().
void video_request_resize(unsigned columns, unsigned rows);

/// @brief A font-size change, as the user asks for it.
///
/// A direction and nothing more. Which sizes exist, and what the glyphs look
/// like, belong to the display side; the console only ever learns the cell
/// counts that come out of the choice.
typedef enum {
    VIDEO_FONT_DEFAULT, ///< Whatever size the console started at.
    VIDEO_FONT_SMALLER, ///< One step down, if there is one.
    VIDEO_FONT_LARGER,  ///< One step up, if there is one.
} video_font_request_t;

/// @brief Records that the console should change font size.
/// @param request The direction to move in.
///
/// Safe to call from anywhere the console can be written to, which includes the
/// escape-sequence parser: it accumulates the request and does nothing else. A
/// font change allocates and talks to the display, and video_putc() is reachable
/// from contexts where neither is safe.
///
/// Relative requests **accumulate** rather than replacing one another, so two
/// "larger" arriving before the next service move two steps, not one. A request
/// for the default clears what has accumulated, being absolute rather than
/// relative. Requests are dropped immediately, with nothing left pending, when
/// the active backend cannot change font at all.
///
/// The work happens in video_service_pending().
void video_request_font(video_font_request_t request);

/// @brief Applies a pending resize, if there is one.
///
/// **Process context only.** It allocates, talks to the backend and copies the
/// console, none of which may happen in an interrupt handler.
///
/// Called from the console's own syscall paths, so a resize is serviced the next
/// time anything reads from or writes to the console. A shell blocked on input
/// is woken when the request is recorded, which makes that next time
/// immediate in practice; a guest touching the console not at all leaves the
/// resize pending, which is the honest consequence of a kernel with no worker
/// threads.
///
/// Also applies a pending font change, and in that order: the backend is given
/// its process context first so that it can refresh the display size it knows
/// about, and the font change then derives its cell counts from that. A font
/// change supersedes a pending resize rather than following it, because both
/// would compute their cell counts from the same scanout and only the font
/// change knows the new cell size.
void video_service_pending(void);

/// @brief Begins a run of output that may be shown all at once.
///
/// A hint, not a mode: the cell buffer changes exactly as it would without it,
/// and every change is visible by the time the matching video_end_batch()
/// returns. What it buys is on backends where *presenting* costs a device round
/// trip, which without this happens once per character.
///
/// Nesting is counted, so it is safe to bracket a caller that brackets its own
/// callees. Every begin must have an end; there is no timer that will end one.
void video_begin_batch(void);

/// @brief Ends a run of output and shows everything it changed.
void video_end_batch(void);

/// @brief Print the given character on the screen.
/// @param c The character to print.
void video_putc(int c);

/// @brief Prints the given string on the screen.
/// @param str The string to print.
void video_puts(const char *str);

/// @brief When something is written in another position, update the cursor.
void video_update_cursor_position(void);

/// @brief Drives a software cursor's blink; called once per timer tick.
///
/// The console has no periodic source of its own, and a cursor drawn in software
/// only blinks if something toggles it. This is that something: the timer
/// interrupt calls it, and it forwards to the backend when the backend has a
/// cursor to blink. Backends whose cursor blinks in hardware make it a no-op.
void video_cursor_blink_tick(void);

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
