/// @file vga_palette.h
/// @brief VGA color palettes.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

/// Structure that simplifies defining a palette.
typedef struct {
    unsigned char red;   ///< Red value.
    unsigned char green; ///< Green value.
    unsigned char blue;  ///< Blue value.
} palette_entry_t;

/// 16 color palette.
palette_entry_t ansi_16_palette[17] = {
    {0x00, 0x00, 0x00}, //  0 Black
    {0xAA, 0x00, 0x00}, //  1 Red
    {0x00, 0xAA, 0x00}, //  2 Green
    {0xAA, 0xAA, 0x00}, //  3 Yellow
    {0x00, 0x00, 0xAA}, //  4 Blue
    {0xAA, 0x00, 0xAA}, //  5 Magenta
    {0x00, 0xAA, 0xAA}, //  6 Cyan
    {0xAA, 0xAA, 0xAA}, //  7 White
    {0x55, 0x55, 0x55}, //  8 Black
    {0xFF, 0x55, 0x55}, //  9 Red
    {0x55, 0xFF, 0x55}, // 10 Green
    {0xFF, 0xFF, 0x55}, // 11 Yellow
    {0x55, 0x55, 0xFF}, // 12 Blue
    {0xFF, 0x55, 0xFF}, // 13 Magenta
    {0x55, 0xFF, 0xFF}, // 14 Cyan
    {0xFF, 0xFF, 0xFF}, // 15 White
};

/// 256 color palette.
palette_entry_t ansi_256_palette[256] = {
    {0, 0, 0},       // Black
    {128, 0, 0},     // Maroon
    {0, 128, 0},     // Green
    {128, 128, 0},   // Olive
    {0, 0, 128},     // Navy
    {128, 0, 128},   // Purple
    {0, 128, 128},   // Teal
    {192, 192, 192}, // Silver
    {128, 128, 128}, // Grey
    {255, 0, 0},     // Red
    {0, 255, 0},     // Lime
    {255, 255, 0},   // Yellow
    {0, 0, 255},     // Blue
    {255, 0, 255},   // Fuchsia
    {0, 255, 255},   // Aqua
    {255, 255, 255}, // White
    {0, 0, 55},      // ExtremelyDarkBlue
    {0, 0, 95},      // NavyBlue
    {0, 0, 135},     // DarkBlue
    {0, 0, 175},     // Blue3
    {0, 0, 215},     // Blue3
    {0, 0, 255},     // Blue1
    {0, 95, 0},      // DarkGreen
    {0, 95, 95},     // DeepSkyBlue4
    {0, 95, 135},    // DeepSkyBlue4
    {0, 95, 175},    // DeepSkyBlue4
    {0, 95, 215},    // DodgerBlue3
    {0, 95, 255},    // DodgerBlue2
    {0, 135, 0},     // Green4
    {0, 135, 95},    // SpringGreen4
    {0, 135, 135},   // Turquoise4
    {0, 135, 175},   // DeepSkyBlue3
    {0, 135, 215},   // DeepSkyBlue3
    {0, 135, 255},   // DodgerBlue1
    {0, 175, 0},     // Green3
    {0, 175, 95},    // SpringGreen3
    {0, 175, 135},   // DarkCyan
    {0, 175, 175},   // LightSeaGreen
    {0, 175, 215},   // DeepSkyBlue2
    {0, 175, 255},   // DeepSkyBlue1
    {0, 215, 0},     // Green3
    {0, 215, 95},    // SpringGreen3
    {0, 215, 135},   // SpringGreen2
    {0, 215, 175},   // Cyan3
    {0, 215, 215},   // DarkTurquoise
    {0, 215, 255},   // Turquoise2
    {0, 255, 0},     // Green1
    {0, 255, 95},    // SpringGreen2
    {0, 255, 135},   // SpringGreen1
    {0, 255, 175},   // MediumSpringGreen
    {0, 255, 215},   // Cyan2
    {0, 255, 255},   // Cyan1
    {95, 0, 0},      // DarkRed
    {95, 0, 95},     // DeepPink4
    {95, 0, 135},    // Purple4
    {95, 0, 175},    // Purple4
    {95, 0, 215},    // Purple3
    {95, 0, 255},    // BlueViolet
    {95, 95, 0},     // Orange4
    {95, 95, 95},    // Grey37
    {95, 95, 135},   // MediumPurple4
    {95, 95, 175},   // SlateBlue3
    {95, 95, 215},   // SlateBlue3
    {95, 95, 255},   // RoyalBlue1
    {95, 135, 0},    // Chartreuse4
    {95, 135, 95},   // DarkSeaGreen4
    {95, 135, 135},  // PaleTurquoise4
    {95, 135, 175},  // SteelBlue
    {95, 135, 215},  // SteelBlue3
    {95, 135, 255},  // CornflowerBlue
    {95, 175, 0},    // Chartreuse3
    {95, 175, 95},   // DarkSeaGreen4
    {95, 175, 135},  // CadetBlue
    {95, 175, 175},  // CadetBlue
    {95, 175, 215},  // SkyBlue3
    {95, 175, 255},  // SteelBlue1
    {95, 215, 0},    // Chartreuse3
    {95, 215, 95},   // PaleGreen3
    {95, 215, 135},  // SeaGreen3
    {95, 215, 175},  // Aquamarine3
    {95, 215, 215},  // MediumTurquoise
    {95, 215, 255},  // SteelBlue1
    {95, 255, 0},    // Chartreuse2
    {95, 255, 95},   // SeaGreen2
    {95, 255, 135},  // SeaGreen1
    {95, 255, 175},  // SeaGreen1
    {95, 255, 215},  // Aquamarine1
    {95, 255, 255},  // DarkSlateGray2
    {135, 0, 0},     // DarkRed
    {135, 0, 95},    // DeepPink4
    {135, 0, 135},   // DarkMagenta
    {135, 0, 175},   // DarkMagenta
    {135, 0, 215},   // DarkViolet
    {135, 0, 255},   // Purple
    {135, 95, 0},    // Orange4
    {135, 95, 95},   // LightPink4
    {135, 95, 135},  // Plum4
    {135, 95, 175},  // MediumPurple3
    {135, 95, 215},  // MediumPurple3
    {135, 95, 255},  // SlateBlue1
    {135, 135, 0},   // Yellow4
    {135, 135, 95},  // Wheat4
    {135, 135, 135}, // Grey53
    {135, 135, 175}, // LightSlateGrey
    {135, 135, 215}, // MediumPurple
    {135, 135, 255}, // LightSlateBlue
    {135, 175, 0},   // Yellow4
    {135, 175, 95},  // DarkOliveGreen3
    {135, 175, 135}, // DarkSeaGreen
    {135, 175, 175}, // LightSkyBlue3
    {135, 175, 215}, // LightSkyBlue3
    {135, 175, 255}, // SkyBlue2
    {135, 215, 0},   // Chartreuse2
    {135, 215, 95},  // DarkOliveGreen3
    {135, 215, 135}, // PaleGreen3
    {135, 215, 175}, // DarkSeaGreen3
    {135, 215, 215}, // DarkSlateGray3
    {135, 215, 255}, // SkyBlue1
    {135, 255, 0},   // Chartreuse1
    {135, 255, 95},  // LightGreen
    {135, 255, 135}, // LightGreen
    {135, 255, 175}, // PaleGreen1
    {135, 255, 215}, // Aquamarine1
    {135, 255, 255}, // DarkSlateGray1
    {175, 0, 0},     // Red3
    {175, 0, 95},    // DeepPink4
    {175, 0, 135},   // MediumVioletRed
    {175, 0, 175},   // Magenta3
    {175, 0, 215},   // DarkViolet
    {175, 0, 255},   // Purple
    {175, 95, 0},    // DarkOrange3
    {175, 95, 95},   // IndianRed
    {175, 95, 135},  // HotPink3
    {175, 95, 175},  // MediumOrchid3
    {175, 95, 215},  // MediumOrchid
    {175, 95, 255},  // MediumPurple2
    {175, 135, 0},   // DarkGoldenrod
    {175, 135, 95},  // LightSalmon3
    {175, 135, 135}, // RosyBrown
    {175, 135, 175}, // Grey63
    {175, 135, 215}, // MediumPurple2
    {175, 135, 255}, // MediumPurple1
    {175, 175, 0},   // Gold3
    {175, 175, 95},  // DarkKhaki
    {175, 175, 135}, // NavajoWhite3
    {175, 175, 175}, // Grey69
    {175, 175, 215}, // LightSteelBlue3
    {175, 175, 255}, // LightSteelBlue
    {175, 215, 0},   // Yellow3
    {175, 215, 95},  // DarkOliveGreen3
    {175, 215, 135}, // DarkSeaGreen3
    {175, 215, 175}, // DarkSeaGreen2
    {175, 215, 215}, // LightCyan3
    {175, 215, 255}, // LightSkyBlue1
    {175, 255, 0},   // GreenYellow
    {175, 255, 95},  // DarkOliveGreen2
    {175, 255, 135}, // PaleGreen1
    {175, 255, 175}, // DarkSeaGreen2
    {175, 255, 215}, // DarkSeaGreen1
    {175, 255, 255}, // PaleTurquoise1
    {215, 0, 0},     // Red3
    {215, 0, 95},    // DeepPink3
    {215, 0, 135},   // DeepPink3
    {215, 0, 175},   // Magenta3
    {215, 0, 215},   // Magenta3
    {215, 0, 255},   // Magenta2
    {215, 95, 0},    // DarkOrange3
    {215, 95, 95},   // IndianRed
    {215, 95, 135},  // HotPink3
    {215, 95, 175},  // HotPink2
    {215, 95, 215},  // Orchid
    {215, 95, 255},  // MediumOrchid1
    {215, 135, 0},   // Orange3
    {215, 135, 95},  // LightSalmon3
    {215, 135, 135}, // LightPink3
    {215, 135, 175}, // Pink3
    {215, 135, 215}, // Plum3
    {215, 135, 255}, // Violet
    {215, 175, 0},   // Gold3
    {215, 175, 95},  // LightGoldenrod3
    {215, 175, 135}, // Tan
    {215, 175, 175}, // MistyRose3
    {215, 175, 215}, // Thistle3
    {215, 175, 255}, // Plum2
    {215, 215, 0},   // Yellow3
    {215, 215, 95},  // Khaki3
    {215, 215, 135}, // LightGoldenrod2
    {215, 215, 175}, // LightYellow3
    {215, 215, 215}, // Grey84
    {215, 215, 255}, // LightSteelBlue1
    {215, 255, 0},   // Yellow2
    {215, 255, 95},  // DarkOliveGreen1
    {215, 255, 135}, // DarkOliveGreen1
    {215, 255, 175}, // DarkSeaGreen1
    {215, 255, 215}, // Honeydew2
    {215, 255, 255}, // LightCyan1
    {255, 0, 0},     // Red1
    {255, 0, 95},    // DeepPink2
    {255, 0, 135},   // DeepPink1
    {255, 0, 175},   // DeepPink1
    {255, 0, 215},   // Magenta2
    {255, 0, 255},   // Magenta1
    {255, 95, 0},    // OrangeRed1
    {255, 95, 95},   // IndianRed1
    {255, 95, 135},  // IndianRed1
    {255, 95, 175},  // HotPink
    {255, 95, 215},  // HotPink
    {255, 95, 255},  // MediumOrchid1
    {255, 135, 0},   // DarkOrange
    {255, 135, 95},  // Salmon1
    {255, 135, 135}, // LightCoral
    {255, 135, 175}, // PaleVioletRed1
    {255, 135, 215}, // Orchid2
    {255, 135, 255}, // Orchid1
    {255, 175, 0},   // Orange1
    {255, 175, 95},  // SandyBrown
    {255, 175, 135}, // LightSalmon1
    {255, 175, 175}, // LightPink1
    {255, 175, 215}, // Pink1
    {255, 175, 255}, // Plum1
    {255, 215, 0},   // Gold1
    {255, 215, 95},  // LightGoldenrod2
    {255, 215, 135}, // LightGoldenrod2
    {255, 215, 175}, // NavajoWhite1
    {255, 215, 215}, // MistyRose1
    {255, 215, 255}, // Thistle1
    {255, 255, 0},   // Yellow1
    {255, 255, 95},  // LightGoldenrod1
    {255, 255, 135}, // Khaki1
    {255, 255, 175}, // Wheat1
    {255, 255, 215}, // Cornsilk1
    {255, 255, 255}, // Grey100
    {8, 8, 8},       // Grey3
    {18, 18, 18},    // Grey7
    {28, 28, 28},    // Grey11
    {38, 38, 38},    // Grey15
    {48, 48, 48},    // Grey19
    {58, 58, 58},    // Grey23
    {68, 68, 68},    // Grey27
    {78, 78, 78},    // Grey30
    {88, 88, 88},    // Grey35
    {98, 98, 98},    // Grey39
    {108, 108, 108}, // Grey42
    {118, 118, 118}, // Grey46
    {128, 128, 128}, // Grey50
    {138, 138, 138}, // Grey54
    {148, 148, 148}, // Grey58
    {158, 158, 158}, // Grey62
    {168, 168, 168}, // Grey66
    {178, 178, 178}, // Grey70
    {188, 188, 188}, // Grey74
    {198, 198, 198}, // Grey78
    {208, 208, 208}, // Grey82
    {218, 218, 218}, // Grey85
    {228, 228, 228}, // Grey89
    {238, 238, 238}, // Grey93
};
