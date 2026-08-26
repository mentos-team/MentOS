/// @file vga_graphics_mode.h
/// @brief Register values for the 640x480 16-colour planar VGA mode.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.
///
/// The values are grouped exactly as the hardware is addressed, so each array
/// index is the register index. Geometry is a property of these numbers, not of
/// any comment, and can be read back out of them:
///
///   - CRTC 0x13 (offset) = 0x28 = 40, and the offset counts in words, so a
///     scan line is 40 * 2 = 80 bytes per plane, which is 640 pixels at one
///     bit per pixel per plane.
///   - CRTC 0x12 (vertical display end) = 0xDF = 223, extended by bits 1 and 6
///     of CRTC 0x07 (overflow) = 0x3E, giving 479: a 480-line display.
///   - GC 0x06 (miscellaneous) = 0x05 selects the 64 KB window at 0xA0000 and
///     graphics mode. Four planes of 480 * 80 = 38400 bytes fit inside it.

#pragma once

#include "stdint.h"

/// @brief Number of sequencer registers written when setting the mode.
#define VGA_MODE_SEQUENCER_REGISTERS 5
/// @brief Number of CRT controller registers written when setting the mode.
#define VGA_MODE_CRTC_REGISTERS      25
/// @brief Number of graphics controller registers written when setting the mode.
#define VGA_MODE_GRAPHICS_REGISTERS  9
/// @brief Number of attribute controller registers written when setting the mode.
#define VGA_MODE_ATTRIBUTE_REGISTERS 21

/// @brief Miscellaneous Output Register (port 0x3C2).
///
/// 0xE3: colour emulation with the CRTC at 0x3Dx, CPU access to video memory
/// enabled, the 25 MHz dot clock used by 640-pixel-wide modes, and both sync
/// polarities negative, which is the 480-line combination.
static const uint8_t vga_mode_misc = 0xE3;

/// @brief Sequencer registers 0x00-0x04.
static const uint8_t vga_mode_sequencer[VGA_MODE_SEQUENCER_REGISTERS] = {
    0x03, // 0x00 reset: both reset bits high, so the sequencer runs.
    0x01, // 0x01 clocking mode: 8 dots per character.
    0x0F, // 0x02 map mask: writes reach all four planes.
    0x00, // 0x03 character map select: unused outside text modes.
    0x06, // 0x04 memory mode: extended memory, odd/even off, chain-4 off.
};

/// @brief CRT controller registers 0x00-0x18.
///
/// Index 0x03 has bit 7 set and index 0x11 has bit 7 clear, which is what keeps
/// the timing registers writable: with CRTC 0x11 bit 7 set, registers 0x00-0x07
/// are write-protected.
static const uint8_t vga_mode_crtc[VGA_MODE_CRTC_REGISTERS] = {
    0x5F, // 0x00 horizontal total.
    0x4F, // 0x01 end horizontal display: 79, i.e. 80 character clocks.
    0x50, // 0x02 start horizontal blanking.
    0x82, // 0x03 end horizontal blanking (bit 7 set: registers stay writable).
    0x54, // 0x04 start horizontal retrace.
    0x80, // 0x05 end horizontal retrace.
    0x0B, // 0x06 vertical total.
    0x3E, // 0x07 overflow: supplies the high bits of the vertical timings.
    0x00, // 0x08 preset row scan.
    0x40, // 0x09 maximum scan line: one scan line per row, no doubling.
    0x00, // 0x0A cursor start: the hardware text cursor is unused here.
    0x00, // 0x0B cursor end.
    0x00, // 0x0C start address high.
    0x00, // 0x0D start address low.
    0x00, // 0x0E cursor location high.
    0x00, // 0x0F cursor location low.
    0xEA, // 0x10 vertical retrace start.
    0x0C, // 0x11 vertical retrace end (bit 7 clear: no write protection).
    0xDF, // 0x12 vertical display end: 479 once extended by the overflow.
    0x28, // 0x13 offset: 40 words, i.e. 80 bytes per scan line per plane.
    0x00, // 0x14 underline location.
    0xE7, // 0x15 start vertical blanking.
    0x04, // 0x16 end vertical blanking.
    0xE3, // 0x17 mode control: byte mode, normal addressing.
    0xFF, // 0x18 line compare: no split screen.
};

/// @brief Graphics controller registers 0x00-0x08.
static const uint8_t vga_mode_graphics[VGA_MODE_GRAPHICS_REGISTERS] = {
    0x00, // 0x00 set/reset.
    0x00, // 0x01 enable set/reset: off, so writes carry the CPU's own data.
    0x00, // 0x02 colour compare.
    0x00, // 0x03 data rotate: write mode 0, no rotation, no logical op.
    0x03, // 0x04 read map select.
    0x00, // 0x05 graphics mode: write mode 0, planar (not 256-colour shift).
    0x05, // 0x06 miscellaneous: graphics mode, 64 KB window at 0xA0000.
    0x0F, // 0x07 colour don't care.
    0xFF, // 0x08 bit mask: every bit of a written byte takes effect.
};

/// @brief Attribute controller registers 0x00-0x14.
///
/// The palette registers are the identity mapping, so attribute index N selects
/// DAC entry N and a 16-entry DAC load is sufficient. The EGA-compatible
/// mapping that the recovered reference code used here instead sends indices 6
/// and 8-15 to DAC entries 0x14 and 0x38-0x3F, which such a load never
/// initialises.
static const uint8_t vga_mode_attribute[VGA_MODE_ATTRIBUTE_REGISTERS] = {
    0x00, 0x01, 0x02, 0x03, // 0x00-0x03 palette.
    0x04, 0x05, 0x06, 0x07, // 0x04-0x07 palette.
    0x08, 0x09, 0x0A, 0x0B, // 0x08-0x0B palette.
    0x0C, 0x0D, 0x0E, 0x0F, // 0x0C-0x0F palette.
    0x01,                   // 0x10 mode control: graphics, 16 colours.
    0x00,                   // 0x11 overscan colour.
    0x0F,                   // 0x12 colour plane enable: all four planes.
    0x00,                   // 0x13 horizontal pixel panning.
    0x00,                   // 0x14 colour select.
};
