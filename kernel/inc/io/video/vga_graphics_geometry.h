/// @file vga_graphics_geometry.h
/// @brief Console geometry of the graphical VGA backend.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.
///
/// Derived from the mode and the font, both of which divide exactly:
///
///     640 pixels / 8 pixels per glyph  = 80 columns
///     480 lines  / 16 lines per glyph  = 30 rows
///
/// Neither division leaves a remainder, so no part of the display is wasted.
/// 80 columns also matters for more than tidiness: the boot log writes its
/// status markers at column `width - 5`, so a narrower console would wrap every
/// line of it.

#pragma once

/// Console width in cells: 640 pixels at 8 pixels per glyph.
#define VIDEO_COLUMNS 80

/// Console height in cells: 480 scan lines at 16 lines per glyph.
#define VIDEO_ROWS    30
