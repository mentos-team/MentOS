/// @file video_palette_16.h
/// @brief The console's 16 colours, shared by every backend that owns a palette.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.
///
/// The standard IBM VGA 16-colour palette, in the order the console's attribute
/// nibbles index it. Both graphical backends load exactly these entries into
/// the DAC, which is what makes them render the same console in the same
/// colours. Components are given as 8-bit values for readability; the
/// DAC only takes 6 bits, so the loader shifts them right by two. Writing 8-bit
/// values straight to the DAC silently drops the top two bits and turns, for
/// example, a 0x80 component into 0x00.

#pragma once

#include "stdint.h"

/// @brief One palette entry, as 8-bit RGB.
typedef struct {
    uint8_t red;   ///< Red component.
    uint8_t green; ///< Green component.
    uint8_t blue;  ///< Blue component.
} video_palette_entry_t;

/// @brief The 16 console colours, indexed by attribute nibble.
static const video_palette_entry_t video_palette_16[16] = {
    {0x00, 0x00, 0x00}, //  0 Black
    {0x00, 0x00, 0xAA}, //  1 Blue
    {0x00, 0xAA, 0x00}, //  2 Green
    {0x00, 0xAA, 0xAA}, //  3 Cyan
    {0xAA, 0x00, 0x00}, //  4 Red
    {0xAA, 0x00, 0xAA}, //  5 Magenta
    {0xAA, 0x55, 0x00}, //  6 Brown
    {0xAA, 0xAA, 0xAA}, //  7 Light grey
    {0x55, 0x55, 0x55}, //  8 Dark grey
    {0x55, 0x55, 0xFF}, //  9 Bright blue
    {0x55, 0xFF, 0x55}, // 10 Bright green
    {0x55, 0xFF, 0xFF}, // 11 Bright cyan
    {0xFF, 0x55, 0x55}, // 12 Bright red
    {0xFF, 0x55, 0xFF}, // 13 Bright magenta
    {0xFF, 0xFF, 0x55}, // 14 Yellow
    {0xFF, 0xFF, 0xFF}, // 15 White
};
