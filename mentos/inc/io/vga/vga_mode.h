/// @file vga_mode.h
/// @brief VGA models.
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

/// Structure that holds the information about a VGA mode.
typedef struct {
    unsigned char misc; ///< 00h --
    /// @brief The Sequencer Registers.
    struct {
        unsigned char reset;                 ///< 00h --
        unsigned char clocking_mode;         ///< 01h --
        unsigned char map_mask;              ///< 02h --
        unsigned char character_map_select;  ///< 03h --
        unsigned char sequencer_memory_mode; ///< 04h --
    } sc;
    /// @brief CRT Controller (CRTC) Registers
    struct {
        unsigned char horizontal_total;          ///< 00h --
        unsigned char end_horizontal_display;    ///< 01h --
        unsigned char start_horizontal_blanking; ///< 02h --
        unsigned char end_horizontal_blanking;   ///< 03h --
        unsigned char start_horizontal_retrace;  ///< 04h --
        unsigned char end_horizontal_retrace;    ///< 05h --
        unsigned char vertical_total;            ///< 06h --
        unsigned char overflow;                  ///< 07h --
        unsigned char preset_row_scan;           ///< 08h --
        unsigned char maximum_scan_line;         ///< 09h --
        unsigned char cursor_start;              ///< 0Ah --
        unsigned char cursor_end;                ///< 0Bh --
        unsigned char start_address_high;        ///< 0Ch --
        unsigned char start_address_low;         ///< 0Dh --
        unsigned char cursor_location_high;      ///< 0Eh --
        unsigned char cursor_location_low;       ///< 0Fh --
        unsigned char vertical_retrace_start;    ///< 10h --
        unsigned char vertical_retrace_end;      ///< 11h --
        unsigned char vertical_display_end;      ///< 12h --
        unsigned char offset;                    ///< 13h --
        unsigned char underline_location;        ///< 14h --
        unsigned char start_vertical_blanking;   ///< 15h --
        unsigned char end_vertical_blanking;     ///< 16h --
        unsigned char crtc_mode_control;         ///< 17h --
        unsigned char line_compare;              ///< 18h --
    } crtc;
    /// @brief The Graphics Registers.
    struct {
        unsigned char set_reset;        ///< 00h --
        unsigned char enable_set_reset; ///< 01h --
        unsigned char color_compare;    ///< 02h --
        unsigned char data_rotate;      ///< 03h --
        unsigned char read_map;         ///< 04h --
        unsigned char graphics_mode;    ///< 05h --
        unsigned char misc_graphics;    ///< 06h --
        unsigned char color_dont_care;  ///< 07h --
        unsigned char bit_mask;         ///< 08h --
    } gc;
    /// @brief The Attribute Controller Registers.
    struct {
        unsigned char internal_palette_registers[16]; ///< 00h-0Fh --
        unsigned char attribute_mode_control;         ///< 10h --
        unsigned char overscan_color;                 ///< 11h --
        unsigned char color_plane_enable;             ///< 12h --
        unsigned char horizontal_pixel_panning;       ///< 13h --
        unsigned char color_select;                   ///< 14h --
    } ac;
} vga_mode_t;

/// @brief Size 80x25, 16 colors.
vga_mode_t _mode_80_25_text = {
    // 3C2h (W):  Miscellaneous Output Register
    //     bit   0  If set Color Emulation:
    //                 Base Address = 3Dxh else Mono Emulation.
    //                 Base Address = 3Bxh.
    //           1  Enable CPU Access to video memory if set
    //         2-3  Clock Select:
    //                00: 25MHz (used for 320/640 pixel wide modes),
    //                01: 28MHz (used for 360/720 pixel wide modes)
    //           5  When in Odd/Even modes Select High 64k bank if set
    //           6  Horizontal Sync Polarity. Negative if set
    //           7  Vertical Sync Polarity. Negative if set
    //              Bit 6-7 indicates the number of lines on the display:
    //                 1:  400, 2: 350, 3: 480
    // Note: Set to all zero on a hardware reset.
    // Note: This register can be read from port 3CCh.
    .misc = 0x67, // 0110 0011
    .sc   = {
        .reset                 = 0x03, // 0000 0011 - Bits 1 and 0 must be 1 to allow the sequencer to operate.
        .clocking_mode         = 0x00, // 0000 0001 - Selects 8 dots per character.
        .map_mask              = 0x03, // 0000 1111 - Write operations affect all the planes.
        .character_map_select  = 0x00, // 0000 0000 - No Character Map Select
        .sequencer_memory_mode = 0x02, // 0000 0110 - System addresses sequentially access data within a bit map.
                                       //             Enables the video memory from 64KB to 256KB
    },
    .crtc = {
        .horizontal_total          = 0x5F, // 95 (+5) - Number of character clocks per scan line.
        .end_horizontal_display    = 0x4F, //
        .start_horizontal_blanking = 0x50,
        .end_horizontal_blanking   = 0x82,
        .start_horizontal_retrace  = 0x55,
        .end_horizontal_retrace    = 0x81,
        .vertical_total            = 0xBF,
        .overflow                  = 0x1F,
        .preset_row_scan           = 0x00,
        .maximum_scan_line         = 0x4F,
        .cursor_start              = 0x0D,
        .cursor_end                = 0x0E,
        .start_address_high        = 0x00,
        .start_address_low         = 0x00,
        .cursor_location_high      = 0x00,
        .cursor_location_low       = 0x50,
        .vertical_retrace_start    = 0x9C,
        .vertical_retrace_end      = 0x0E,
        .vertical_display_end      = 0x8F,
        .offset                    = 0x28,
        .underline_location        = 0x1F,
        .start_vertical_blanking   = 0x96,
        .end_vertical_blanking     = 0xB9,
        .crtc_mode_control         = 0xA3, // 0xA3 1010 0011
        .line_compare              = 0xFF,
    },
    .gc = {
        .set_reset        = 0x00,
        .enable_set_reset = 0x00,
        .color_compare    = 0x00,
        .data_rotate      = 0x00,
        .read_map         = 0x00,
        .graphics_mode    = 0x10,
        .misc_graphics    = 0x0E,
        .color_dont_care  = 0x00,
        .bit_mask         = 0xFF,
    },
    .ac = {
        .internal_palette_registers = {
            0x00,
            0x01,
            0x02,
            0x03,
            0x04,
            0x05,
            0x14,
            0x07,
            0x38,
            0x39,
            0x3A,
            0x3B,
            0x3C,
            0x3D,
            0x3E,
            0x3F,
        },
        .attribute_mode_control   = 0x0C,
        .overscan_color           = 0x00,
        .color_plane_enable       = 0x0F,
        .horizontal_pixel_panning = 0x08,
        .color_select             = 0x00,
    }
};

/// @brief Size 320x200, 256 colors.
vga_mode_t _mode_320_200_256 = {
    // 3C2h (W):  Miscellaneous Output Register
    //     bit   0  If set Color Emulation:
    //                 Base Address = 3Dxh else Mono Emulation.
    //                 Base Address = 3Bxh.
    //           1  Enable CPU Access to video memory if set
    //         2-3  Clock Select:
    //                00: 25MHz (used for 320/640 pixel wide modes),
    //                01: 28MHz (used for 360/720 pixel wide modes)
    //           5  When in Odd/Even modes Select High 64k bank if set
    //           6  Horizontal Sync Polarity. Negative if set
    //           7  Vertical Sync Polarity. Negative if set
    //              Bit 6-7 indicates the number of lines on the display:
    //                 1:  400, 2: 350, 3: 480
    // Note: Set to all zero on a hardware reset.
    // Note: This register can be read from port 3CCh.
    .misc = 0x63, // 0110 0011
    .sc   = {
        .reset                 = 0x03, // 0000 0011 - Bits 1 and 0 must be 1 to allow the sequencer to operate.
        .clocking_mode         = 0x01, // 0000 0001 - Selects 8 dots per character.
        .map_mask              = 0x0F, // 0000 1111 - Write operations affect all the planes.
        .character_map_select  = 0x00, // 0000 0000 - No Character Map Select
        .sequencer_memory_mode = 0x06, // 0000 0110 - System addresses sequentially access data within a bit map.
                                       //             Enables the video memory from 64KB to 256KB
    },
    .crtc = {
        .horizontal_total          = 0x5F, // 95 (+5) - Number of character clocks per scan line.
        .end_horizontal_display    = 0x4F, //
        .start_horizontal_blanking = 0x50,
        .end_horizontal_blanking   = 0x82,
        .start_horizontal_retrace  = 0x54,
        .end_horizontal_retrace    = 0x80,
        .vertical_total            = 0xBF,
        .overflow                  = 0x1F,
        .preset_row_scan           = 0x00,
        .maximum_scan_line         = 0x41,
        .cursor_start              = 0x00,
        .cursor_end                = 0x00,
        .start_address_high        = 0x00,
        .start_address_low         = 0x00,
        .cursor_location_high      = 0x00,
        .cursor_location_low       = 0x00,
        .vertical_retrace_start    = 0x9C,
        .vertical_retrace_end      = 0xE3,
        .vertical_display_end      = 0x8F,
        .offset                    = 0x28,
        .underline_location        = 0x40,
        .start_vertical_blanking   = 0x96,
        .end_vertical_blanking     = 0xB9,
        .crtc_mode_control         = 0xA3, // 0xA3 1010 0011
        .line_compare              = 0xFF,
    },
    .gc = {
        .set_reset        = 0x00,
        .enable_set_reset = 0x00,
        .color_compare    = 0x00,
        .data_rotate      = 0x00,
        .read_map         = 0x00,
        .graphics_mode    = 0x40,
        .misc_graphics    = 0x05,
        .color_dont_care  = 0x0F,
        .bit_mask         = 0xFF,
    },
    .ac = {
        .internal_palette_registers = {
            0x00,
            0x01,
            0x02,
            0x03,
            0x04,
            0x05,
            0x06,
            0x07,
            0x08,
            0x09,
            0x0A,
            0x0B,
            0x0C,
            0x0D,
            0x0E,
            0x0F,
        },
        .attribute_mode_control   = 0x41,
        .overscan_color           = 0x00,
        .color_plane_enable       = 0x0F,
        .horizontal_pixel_panning = 0x00,
        .color_select             = 0x00,
    }
};

/// @brief Size 640x480, 16 colors.
vga_mode_t _mode_640_480_16 = {
    // 3C2h (W):  Miscellaneous Output Register
    //     bit   0  If set Color Emulation:
    //                 Base Address = 3Dxh else Mono Emulation.
    //                 Base Address = 3Bxh.
    //           1  Enable CPU Access to video memory if set
    //         2-3  Clock Select:
    //                00: 25MHz (used for 320/640 pixel wide modes),
    //                01: 28MHz (used for 360/720 pixel wide modes)
    //           5  When in Odd/Even modes Select High 64k bank if set
    //           6  Horizontal Sync Polarity. Negative if set
    //           7  Vertical Sync Polarity. Negative if set
    //              Bit 6-7 indicates the number of lines on the display:
    //                 1:  400, 2: 350, 3: 480
    // Note: Set to all zero on a hardware reset.
    // Note: This register can be read from port 3CCh.
    .misc = 0xE3, // 1110 0011
    .sc   = {
        .reset                 = 0x03, // 0000 0011 - Bits 1 and 0 must be 1 to allow the sequencer to operate.
        .clocking_mode         = 0x01, // 0000 0001 - Selects 8 dots per character.
        .map_mask              = 0x0F, // 0000 1111 - Write operations affect all the planes.
        .character_map_select  = 0x00, // 0000 0000 - No Character Map Select
        .sequencer_memory_mode = 0x06, // 0000 0110 - System addresses sequentially access data within a bit map.
                                       //             Enables the video memory from 64KB to 256KB
    },
    .crtc = {
        .horizontal_total          = 0x5F, // 95 (+5) - Number of character clocks per scan line.
        .end_horizontal_display    = 0x4F, //
        .start_horizontal_blanking = 0x50,
        .end_horizontal_blanking   = 0x82,
        .start_horizontal_retrace  = 0x54,
        .end_horizontal_retrace    = 0x80,
        .vertical_total            = 0x0B,
        .overflow                  = 0x3E,
        .preset_row_scan           = 0x00,
        .maximum_scan_line         = 0x40,
        .cursor_start              = 0x00,
        .cursor_end                = 0x00,
        .start_address_high        = 0x00,
        .start_address_low         = 0x00,
        .cursor_location_high      = 0x00,
        .cursor_location_low       = 0x00,
        .vertical_retrace_start    = 0xEA,
        .vertical_retrace_end      = 0x0C,
        .vertical_display_end      = 0xDF,
        .offset                    = 0x28,
        .underline_location        = 0x00,
        .start_vertical_blanking   = 0xE7,
        .end_vertical_blanking     = 0x04,
        .crtc_mode_control         = 0xE3,
        .line_compare              = 0xFF,
    },
    .gc = {
        .set_reset        = 0x00,
        .enable_set_reset = 0x00,
        .color_compare    = 0x00,
        .data_rotate      = 0x00,
        .read_map         = 0x03,
        .graphics_mode    = 0x00,
        .misc_graphics    = 0x05,
        .color_dont_care  = 0x0F,
        .bit_mask         = 0xFF,
    },
    .ac = {
        .internal_palette_registers = {
            0x00,
            0x01,
            0x02,
            0x03,
            0x04,
            0x05,
            0x14,
            0x07,
            0x38,
            0x39,
            0x3A,
            0x3B,
            0x3C,
            0x3D,
            0x3E,
            0x3F,
        },
        .attribute_mode_control   = 0x01, // 0b01000001, // 0x41,
        .overscan_color           = 0x00,
        .color_plane_enable       = 0x0F,
        .horizontal_pixel_panning = 0x00,
        .color_select             = 0x00,
    }
};

/// @brief Size 720x480, 16 colors.
vga_mode_t _mode_720_480_16 = {
    // 3C2h (W):  Miscellaneous Output Register
    //     bit   0  If set Color Emulation:
    //                 Base Address = 3Dxh else Mono Emulation.
    //                 Base Address = 3Bxh.
    //           1  Enable CPU Access to video memory if set
    //         2-3  Clock Select:
    //                00: 25MHz (used for 320/640 pixel wide modes),
    //                01: 28MHz (used for 360/720 pixel wide modes)
    //           5  When in Odd/Even modes Select High 64k bank if set
    //           6  Horizontal Sync Polarity. Negative if set
    //           7  Vertical Sync Polarity. Negative if set
    //              Bit 6-7 indicates the number of lines on the display:
    //                 1:  400, 2: 350, 3: 480
    // Note: Set to all zero on a hardware reset.
    // Note: This register can be read from port 3CCh.
    .misc = 0xE7, // 1110 0011
    .sc   = {
        .reset                 = 0x03, // 0000 0011 - Bits 1 and 0 must be 1 to allow the sequencer to operate.
        .clocking_mode         = 0x01, // 0000 0001 - Selects 8 dots per character.
        .map_mask              = 0x08, // 0000 1111 - Write operations affect all the planes.
        .character_map_select  = 0x00, // 0000 0000 - No Character Map Select
        .sequencer_memory_mode = 0x06, // 0000 0110 - System addresses sequentially access data within a bit map.
                                       //             Enables the video memory from 64KB to 256KB
    },
    .crtc = {
        .horizontal_total          = 0x6B, // 95 (+5) - Number of character clocks per scan line.
        .end_horizontal_display    = 0x59, //
        .start_horizontal_blanking = 0x5A,
        .end_horizontal_blanking   = 0x82,
        .start_horizontal_retrace  = 0x60,
        .end_horizontal_retrace    = 0x8D,
        .vertical_total            = 0x0B,
        .overflow                  = 0x3E,
        .preset_row_scan           = 0x00,
        .maximum_scan_line         = 0x40,
        .cursor_start              = 0x06,
        .cursor_end                = 0x07,
        .start_address_high        = 0x00,
        .start_address_low         = 0x00,
        .cursor_location_high      = 0x00,
        .cursor_location_low       = 0x00,
        .vertical_retrace_start    = 0xEA,
        .vertical_retrace_end      = 0x0C,
        .vertical_display_end      = 0xDF,
        .offset                    = 0x2D,
        .underline_location        = 0x08,
        .start_vertical_blanking   = 0xE8,
        .end_vertical_blanking     = 0x05,
        .crtc_mode_control         = 0xE3,
        .line_compare              = 0xFF,
    },
    .gc = {
        .set_reset        = 0x00,
        .enable_set_reset = 0x00,
        .color_compare    = 0x00,
        .data_rotate      = 0x00,
        .read_map         = 0x03,
        .graphics_mode    = 0x00,
        .misc_graphics    = 0x05,
        .color_dont_care  = 0x0F,
        .bit_mask         = 0xFF,
    },
    .ac = {
        .internal_palette_registers = {
            0x00,
            0x01,
            0x02,
            0x03,
            0x04,
            0x05,
            0x06,
            0x07,
            0x08,
            0x09,
            0x0A,
            0x0B,
            0x0C,
            0x0D,
            0x0E,
            0x0F,
        },
        .attribute_mode_control   = 0x01, // 0b01000001, // 0x41,
        .overscan_color           = 0x00,
        .color_plane_enable       = 0x0F,
        .horizontal_pixel_panning = 0x00,
        .color_select             = 0x00,
    }
};
