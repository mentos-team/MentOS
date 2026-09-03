/// @file vga_text_geometry.h
/// @brief Console geometry of the VGA text-mode backend.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.
///
/// The generic console sizes its static state from these macros, and the
/// backend initializes its reported geometry from the very same ones, so the
/// two cannot drift apart.

#pragma once

/// Console width in cells: the standard VGA text mode is 80 columns.
#define VIDEO_COLUMNS 80

/// Console height in cells: the standard VGA text mode is 25 rows.
#define VIDEO_ROWS    25
