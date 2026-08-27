/// @file video_font.h
/// @brief Bitmap fonts, shared by every backend that draws text as pixels.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.
///
/// A font is the one place where cells and pixels meet. The generic console
/// counts cells and knows nothing about glyphs; a graphical backend derives its
/// pixel geometry from the console's cell counts and the font it is drawing
/// with. Keeping that arithmetic behind one type is what lets a display resize
/// and a font change be the same operation from the console's point of view:
/// both arrive at the backend as a different pair of cell counts.
///
/// This header is for backends. The generic layer never includes it.

#pragma once

#include "stdint.h"

/// @name The default font's dimensions
/// @{
/// Available as macros because a backend with a fixed mode checks its
/// compile-time geometry against them, and that check has to happen before
/// there is any runtime state to ask.
#define VIDEO_FONT_DEFAULT_WIDTH  8U  ///< Pixels a default glyph occupies across.
#define VIDEO_FONT_DEFAULT_HEIGHT 16U ///< Scan lines a default glyph occupies.
/// @}

/// @brief Widest base asset this representation admits, in pixels.
///
/// One byte per scan line is what makes a glyph cheap to draw, and it is what
/// every asset in this project uses, so a base glyph cannot be wider than a
/// byte. A wider native font would need the row fetch to read more than one
/// byte, which is a change to this header rather than to any backend.
#define VIDEO_FONT_MAX_BASE_WIDTH 8U

/// @brief A bitmap font at a chosen magnification.
///
/// The asset is the bitmap as it was drawn; `scale` magnifies it by replicating
/// pixels, which is how sizes larger than any available asset are reached. A
/// glyph is therefore `base_width * scale` by `base_height * scale` pixels, and
/// nothing outside this header needs to know which part came from which.
typedef struct {
    const char *name;       ///< Effective size as text, for diagnostics.
    const uint8_t *glyphs;  ///< 256 glyphs of base_height bytes, one per scan line.
    uint8_t base_width;     ///< Glyph width in the asset, at most VIDEO_FONT_MAX_BASE_WIDTH.
    uint8_t base_height;    ///< Glyph height in the asset, in scan lines.
    uint8_t scale;          ///< Integer magnification, at least 1.
} video_font_t;

/// @brief Width of a glyph as drawn.
/// @param font The font.
/// @return The width in pixels.
static inline unsigned video_font_width(const video_font_t *font)
{
    return (unsigned)font->base_width * (unsigned)font->scale;
}

/// @brief Height of a glyph as drawn.
/// @param font The font.
/// @return The height in scan lines.
static inline unsigned video_font_height(const video_font_t *font)
{
    return (unsigned)font->base_height * (unsigned)font->scale;
}

/// @brief Unmagnified bitmap of a character.
/// @param font The font.
/// @param character The character code.
/// @return base_height bytes, one per scan line, bit (base_width - 1) leftmost.
///
/// For a backend drawing at `scale == 1`, which is every backend with a fixed
/// mode. Those backends turn a glyph byte straight into device words -- a byte
/// per plane for the planar adapter, a nibble-expanded pair of 32-bit stores for
/// the linear one -- and both tricks exist only because a glyph is exactly eight
/// pixels wide. A backend that magnifies wants video_font_scanline() instead.
static inline const uint8_t *video_font_glyph(const video_font_t *font, uint8_t character)
{
    return &font->glyphs[(unsigned)character * (unsigned)font->base_height];
}

/// @brief One drawn scan line of a character, magnification included.
/// @param font The font.
/// @param character The character code.
/// @param line The scan line, from 0 to video_font_height() - 1.
/// @return A mask in which bit (video_font_width() - 1) is the leftmost pixel.
///
/// For a backend that magnifies, and the only accessor that copes with a scale
/// above 1: the asset's scan line is chosen by dividing, and each of its pixels
/// is then repeated across. A mask rather than bytes because a magnified glyph
/// is wider than a byte, and 32 bits is enough for any size this
/// representation can produce.
static inline uint32_t video_font_scanline(const video_font_t *font, uint8_t character, unsigned line)
{
    unsigned scale = font->scale;
    uint8_t bits   = font->glyphs[((unsigned)character * (unsigned)font->base_height) + (line / scale)];

    // Unmagnified, the asset's byte already is the mask, with the same bit
    // holding the leftmost pixel. This is the overwhelmingly common case.
    if (scale == 1U) {
        return bits;
    }

    unsigned width = video_font_width(font);
    uint32_t out   = 0;
    for (unsigned x = 0; x < font->base_width; ++x) {
        if ((bits & (1U << ((font->base_width - 1U) - x))) == 0U) {
            continue;
        }
        for (unsigned repeat = 0; repeat < scale; ++repeat) {
            out |= 1U << ((width - 1U) - ((x * scale) + repeat));
        }
    }
    return out;
}

/// @name The available fonts
/// @{
/// The default, and the only one any backend draws with today. 8x16 is what
/// every mode in this project is dimensioned for.
extern const video_font_t video_font_8x16;

/// An alternate aspect ratio, not a smaller version of the default: it is the
/// same eight pixels wide, so it buys rows and no columns at all. Kept because
/// it is the classic compact console font and belongs to a separate "compact
/// font" choice rather than to font zoom, which is proportional by definition.
extern const video_font_t video_font_8x8;
/// @}

/// @brief The font a console starts with.
/// @return The default font, never NULL.
const video_font_t *video_font_default(void);
