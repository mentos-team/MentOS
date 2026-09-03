/// @file vbe_lfb_geometry.h
/// @brief Console geometry of the VBE linear-framebuffer backend.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.
///
/// Derived from the mode and the font, both of which divide exactly:
///
///     1024 pixels / 8 pixels per glyph  = 128 columns
///      768 lines  / 16 lines per glyph  =  48 rows
///
/// Neither division leaves a remainder, so no part of the display is wasted.
/// This is the whole point of the backend: the same 8x16 font on a 1024x768
/// mode shows 128x48 cells where 640x480 shows 80x30, so more of the terminal
/// is visible rather than the same terminal being scaled up.
///
/// The console needs at least 80 columns for the boot log, which writes its
/// status markers at column `width - 5`; 128 is comfortably past that.

#pragma once

/// Console width in cells: 1024 pixels at 8 pixels per glyph.
#define VIDEO_COLUMNS 128

/// Console height in cells: 768 scan lines at 16 lines per glyph.
#define VIDEO_ROWS    48
