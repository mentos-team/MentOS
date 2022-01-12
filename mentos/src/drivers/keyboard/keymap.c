/// @file keymap.c
/// @brief Keymap for keyboard.
/// @copyright (c) 2014-2021 This file is distributed under the MIT License.
/// See LICENSE.md for details.
/// @addtogroup keyboard
/// @{

#include "drivers/keyboard/keymap.h"
#include "string.h"

/// Identifies the current keymap type.
keymap_type_t keymap_type = KEYMAP_IT;
/// Contains the different keymaps.
keymap_t keymaps[KEYMAP_TYPE_MAX][65536];

keymap_type_t get_keymap_type()
{
    return keymap_type;
}

void set_keymap_type(keymap_type_t type)
{
    if (type != keymap_type)
        keymap_type = type;
}

const keymap_t *get_keymap(int scancode)
{
    return &keymaps[keymap_type][scancode];
}

void init_keymaps()
{
    for (int i = 0; i < KEYMAP_TYPE_MAX; ++i)
        memset(&keymaps[i], -1, sizeof(keymap_t));

    // == ITALIAN KEY MAPPING =================================================
    //                 Keys                Normal  Shifted Ctrl    Alt
    keymaps[KEYMAP_IT][KEY_A] = (keymap_t){ 0x1E61, 0x1E41, 0x1E01, 0x1E00 }; // 0x001E  A
    keymaps[KEYMAP_IT][KEY_B] = (keymap_t){ 0x3062, 0x3042, 0x3002, 0x3000 }; // 0x0030  B
    keymaps[KEYMAP_IT][KEY_C] = (keymap_t){ 0x2E63, 0x2E43, 0x2E03, 0x2E00 }; // 0x002E  C
    keymaps[KEYMAP_IT][KEY_D] = (keymap_t){ 0x2064, 0x2044, 0x2004, 0x2000 }; // 0x0020  D
    keymaps[KEYMAP_IT][KEY_E] = (keymap_t){ 0x1265, 0x1245, 0x1205, 0x1200 }; // 0x0012  E
    keymaps[KEYMAP_IT][KEY_F] = (keymap_t){ 0x2166, 0x2146, 0x2106, 0x2100 }; // 0x0021  F
    keymaps[KEYMAP_IT][KEY_G] = (keymap_t){ 0x2267, 0x2247, 0x2207, 0x2200 }; // 0x0022  G
    keymaps[KEYMAP_IT][KEY_H] = (keymap_t){ 0x2368, 0x2348, 0x2308, 0x2300 }; // 0x0023  H
    keymaps[KEYMAP_IT][KEY_I] = (keymap_t){ 0x1769, 0x1749, 0x1709, 0x1700 }; // 0x0017  I
    keymaps[KEYMAP_IT][KEY_J] = (keymap_t){ 0x246A, 0x244A, 0x240A, 0x2400 }; // 0x0024  J
    keymaps[KEYMAP_IT][KEY_K] = (keymap_t){ 0x256B, 0x254B, 0x250B, 0x2500 }; // 0x0025  K
    keymaps[KEYMAP_IT][KEY_L] = (keymap_t){ 0x266C, 0x264C, 0x260C, 0x2600 }; // 0x0026  L
    keymaps[KEYMAP_IT][KEY_M] = (keymap_t){ 0x326D, 0x324D, 0x320D, 0x3200 }; // 0x0032  M
    keymaps[KEYMAP_IT][KEY_N] = (keymap_t){ 0x316E, 0x314E, 0x310E, 0x3100 }; // 0x0031  N
    keymaps[KEYMAP_IT][KEY_O] = (keymap_t){ 0x186F, 0x184F, 0x180F, 0x1800 }; // 0x0018  O
    keymaps[KEYMAP_IT][KEY_P] = (keymap_t){ 0x1970, 0x1950, 0x1910, 0x1900 }; // 0x0019  P
    keymaps[KEYMAP_IT][KEY_Q] = (keymap_t){ 0x1071, 0x1051, 0x1011, 0x1000 }; // 0x0010  Q
    keymaps[KEYMAP_IT][KEY_R] = (keymap_t){ 0x1372, 0x1352, 0x1312, 0x1300 }; // 0x0013  R
    keymaps[KEYMAP_IT][KEY_S] = (keymap_t){ 0x1F73, 0x1F53, 0x1F13, 0x1F00 }; // 0x001F  S
    keymaps[KEYMAP_IT][KEY_T] = (keymap_t){ 0x1474, 0x1454, 0x1414, 0x1400 }; // 0x0014  T
    keymaps[KEYMAP_IT][KEY_U] = (keymap_t){ 0x1675, 0x1655, 0x1615, 0x1600 }; // 0x0016  U
    keymaps[KEYMAP_IT][KEY_V] = (keymap_t){ 0x2F76, 0x2F56, 0x2F16, 0x2F00 }; // 0x002F  V
    keymaps[KEYMAP_IT][KEY_W] = (keymap_t){ 0x1177, 0x1157, 0x1117, 0x1100 }; // 0x0011  W
    keymaps[KEYMAP_IT][KEY_X] = (keymap_t){ 0x2D78, 0x2D58, 0x2D18, 0x2D00 }; // 0x002D  X
    keymaps[KEYMAP_IT][KEY_Y] = (keymap_t){ 0x1579, 0x1559, 0x1519, 0x1500 }; // 0x0015  Y
    keymaps[KEYMAP_IT][KEY_Z] = (keymap_t){ 0x2C7A, 0x2C5A, 0x2C1A, 0x2C00 }; // 0x002C  Z

    keymaps[KEYMAP_IT][KEY_1] = (keymap_t){ 0x0231, 0x0221, 0x0000, 0x7800 }; // 0x0002  1 !
    keymaps[KEYMAP_IT][KEY_2] = (keymap_t){ 0x0332, 0x0322, 0x0300, 0x7900 }; // 0x0003  2 "
    keymaps[KEYMAP_IT][KEY_3] = (keymap_t){ 0x0433, 0x049C, 0x0000, 0x7A00 }; // 0x0004  3 £
    keymaps[KEYMAP_IT][KEY_4] = (keymap_t){ 0x0534, 0x0524, 0x0000, 0x7B00 }; // 0x0005  4 $
    keymaps[KEYMAP_IT][KEY_5] = (keymap_t){ 0x0635, 0x0625, 0x0000, 0x7C00 }; // 0x0006  5 %
    keymaps[KEYMAP_IT][KEY_6] = (keymap_t){ 0x0736, 0x0726, 0x071E, 0x7D00 }; // 0x0007  6 &
    keymaps[KEYMAP_IT][KEY_7] = (keymap_t){ 0x0837, 0x082F, 0x0000, 0x7E00 }; // 0x0008  7 /
    keymaps[KEYMAP_IT][KEY_8] = (keymap_t){ 0x0938, 0x0928, 0x0000, 0x7F00 }; // 0x0009  8 (
    keymaps[KEYMAP_IT][KEY_9] = (keymap_t){ 0x0A39, 0x0A29, 0x0000, 0x8000 }; // 0x000A  9 )
    keymaps[KEYMAP_IT][KEY_0] = (keymap_t){ 0x0B30, 0x0B3D, 0x0000, 0x8100 }; // 0x000B  0 =

    keymaps[KEYMAP_IT][KEY_GRAVE]         = (keymap_t){ 0x295C, 0x297C, 0x0000, 0x2900 }; // 0x0029  \ |
    keymaps[KEYMAP_IT][KEY_APOSTROPHE]    = (keymap_t){ 0x0C27, 0x0C3F, 0x0000, 0x8200 }; // 0x000C  ' ?
    keymaps[KEYMAP_IT][KEY_I_ACC]         = (keymap_t){ 0x0D8D, 0x0D5E, 0x0000, 0x8300 }; // 0x000D  ì ^
    keymaps[KEYMAP_IT][KEY_LEFT_BRAKET]   = (keymap_t){ 0x1A8A, 0x1A82, 0x0000, 0x1A5B }; // 0x001A  è é [
    keymaps[KEYMAP_IT][KEY_RIGHT_BRAKET]  = (keymap_t){ 0x1B2B, 0x1B2A, 0x0000, 0x1B5D }; // 0x001B  + * ]
    keymaps[KEYMAP_IT][KEY_SEMICOLON]     = (keymap_t){ 0x2795, 0x2780, 0x0000, 0x2740 }; // 0x0027  ò ç @
    keymaps[KEYMAP_IT][KEY_DOUBLE_QUOTES] = (keymap_t){ 0x2885, 0x28A7, 0x0000, 0x2823 }; // 0x0028  à ° #
    keymaps[KEYMAP_IT][KEY_BACKSLASH]     = (keymap_t){ 0x2B97, 0x2BA7, 0x0000, 0x2B00 }; // 0x002B  ù §
    keymaps[KEYMAP_IT][KEY_KP_LESS]       = (keymap_t){ 0x563C, 0x563E, 0x0000, 0x5600 }; // 0x0056  < >
    keymaps[KEYMAP_IT][KEY_COMMA]         = (keymap_t){ 0x332C, 0x333B, 0x0000, 0x3300 }; // 0x0033  , ;
    keymaps[KEYMAP_IT][KEY_PERIOD]        = (keymap_t){ 0x342E, 0x343A, 0x0000, 0x3400 }; // 0x0034  . :
    keymaps[KEYMAP_IT][KEY_MINUS]         = (keymap_t){ 0x352D, 0x355F, 0x0000, 0x3500 }; // 0x0035  - _

    keymaps[KEYMAP_IT][KEY_F1]  = (keymap_t){ 0x3B00, 0x5400, 0x5E00, 0x6800 }; // 0x003B  F1
    keymaps[KEYMAP_IT][KEY_F2]  = (keymap_t){ 0x3C00, 0x5500, 0x5F00, 0x6900 }; // 0x003C  F2
    keymaps[KEYMAP_IT][KEY_F3]  = (keymap_t){ 0x3D00, 0x5600, 0x6000, 0x6A00 }; // 0x003D  F3
    keymaps[KEYMAP_IT][KEY_F4]  = (keymap_t){ 0x3E00, 0x5700, 0x6100, 0x6B00 }; // 0x003E  F4
    keymaps[KEYMAP_IT][KEY_F5]  = (keymap_t){ 0x3F00, 0x5800, 0x6200, 0x6C00 }; // 0x003F  F5
    keymaps[KEYMAP_IT][KEY_F6]  = (keymap_t){ 0x4000, 0x5900, 0x6300, 0x6D00 }; // 0x0040  F6
    keymaps[KEYMAP_IT][KEY_F7]  = (keymap_t){ 0x4100, 0x5A00, 0x6400, 0x6E00 }; // 0x0041  F7
    keymaps[KEYMAP_IT][KEY_F8]  = (keymap_t){ 0x4200, 0x5B00, 0x6500, 0x6F00 }; // 0x0042  F8
    keymaps[KEYMAP_IT][KEY_F9]  = (keymap_t){ 0x4300, 0x5C00, 0x6600, 0x7000 }; // 0x0043  F9
    keymaps[KEYMAP_IT][KEY_F10] = (keymap_t){ 0x4400, 0x5D00, 0x6700, 0x7100 }; // 0x0044  F10
    keymaps[KEYMAP_IT][KEY_F11] = (keymap_t){ 0x5700, 0x5900, 0x5B00, 0x5D00 }; // 0x0057  F11
    keymaps[KEYMAP_IT][KEY_F12] = (keymap_t){ 0x5800, 0x5A00, 0x5C00, 0x5E00 }; // 0x0058  F12

    keymaps[KEYMAP_IT][KEY_BACKSPACE] = (keymap_t){ 0x0E08, 0x0E08, 0x0E7F, 0x0E00 }; // 0x000E  BackSpace
    keymaps[KEYMAP_IT][KEY_ENTER]     = (keymap_t){ 0x1C0D, 0x1C0D, 0x1C0A, 0xA600 }; // 0x001C  Enter
    keymaps[KEYMAP_IT][KEY_ESCAPE]    = (keymap_t){ 0x011B, 0x011B, 0x011B, 0x0100 }; // 0x0001  Esc
    keymaps[KEYMAP_IT][KEY_TAB]       = (keymap_t){ 0x0F09, 0x0F00, 0x9400, 0xA500 }; // 0x000F  Tab
    keymaps[KEYMAP_IT][KEY_SPACE]     = (keymap_t){ 0x3920, 0x3920, 0x3920, 0x3920 }; // 0x0039  SpaceBar

    keymaps[KEYMAP_IT][KEY_INSERT]    = (keymap_t){ 0x5200, 0x5230, 0x9200, 0xA200 }; // 0xE052  Ins
    keymaps[KEYMAP_IT][KEY_HOME]      = (keymap_t){ 0x4700, 0x4737, 0x7700, 0x9700 }; // 0xE047  Home
    keymaps[KEYMAP_IT][KEY_PAGE_UP]   = (keymap_t){ 0x4900, 0x4939, 0x8400, 0x9900 }; // 0xE049  PgUp
    keymaps[KEYMAP_IT][KEY_DELETE]    = (keymap_t){ 0x5300, 0x532E, 0x9300, 0xA300 }; // 0xE053  Del
    keymaps[KEYMAP_IT][KEY_END]       = (keymap_t){ 0x4F00, 0x4F31, 0x7500, 0x9F00 }; // 0xE04F  End
    keymaps[KEYMAP_IT][KEY_PAGE_DOWN] = (keymap_t){ 0x5100, 0x5133, 0x7600, 0xA100 }; // 0xE051  PgDn

    keymaps[KEYMAP_IT][KEY_UP_ARROW]    = (keymap_t){ 0x4800, 0x4838, 0x8D00, 0x9800 }; // 0xE048  Up Arrow
    keymaps[KEYMAP_IT][KEY_DOWN_ARROW]  = (keymap_t){ 0x5000, 0x5032, 0x9100, 0xA000 }; // 0xE050  Down Arrow
    keymaps[KEYMAP_IT][KEY_LEFT_ARROW]  = (keymap_t){ 0x4B00, 0x4B34, 0x7300, 0x9B00 }; // 0xE04B  Left Arrow
    keymaps[KEYMAP_IT][KEY_RIGHT_ARROW] = (keymap_t){ 0x4D00, 0x4D36, 0x7400, 0x9D00 }; // 0xE04D  Right Arrow

    keymaps[KEYMAP_IT][KEY_KP_DIV]    = (keymap_t){ 0x352F, 0x352F, 0x3500, 0x3500 }; // 0xE035  Kp.Div
    keymaps[KEYMAP_IT][KEY_KP_MUL]    = (keymap_t){ 0x372A, 0x372A, 0x3700, 0x3700 }; // 0x0037  Kp.Mul
    keymaps[KEYMAP_IT][KEY_KP_SUB]    = (keymap_t){ 0x4A2D, 0x4A2D, 0x4A00, 0x4A00 }; // 0x004A  Kp.Sub
    keymaps[KEYMAP_IT][KEY_KP_ADD]    = (keymap_t){ 0x4E2B, 0x4E2B, 0x4E00, 0x4E00 }; // 0x004E  Kp.Add
    keymaps[KEYMAP_IT][KEY_KP_RETURN] = (keymap_t){ 0xE01C, 0xE01C, 0x1C00, 0x1C00 }; // 0xE01C  Kp.Enter

    keymaps[KEYMAP_IT][KEY_KP7] = (keymap_t){ 0x4737, 0x4737, 0x4700, 0x4700 }; // 0x0047
    keymaps[KEYMAP_IT][KEY_KP8] = (keymap_t){ 0x4838, 0x4838, 0x4800, 0x4800 }; // 0x0048
    keymaps[KEYMAP_IT][KEY_KP9] = (keymap_t){ 0x4939, 0x4939, 0x4900, 0x4900 }; // 0x0049
    keymaps[KEYMAP_IT][KEY_KP4] = (keymap_t){ 0x4B34, 0x4B34, 0x4B00, 0x4B00 }; // 0x004B
    keymaps[KEYMAP_IT][KEY_KP5] = (keymap_t){ 0x4C35, 0x4C35, 0x4C00, 0x4C00 }; // 0x004C
    keymaps[KEYMAP_IT][KEY_KP6] = (keymap_t){ 0x4D36, 0x4D36, 0x4D00, 0x4D00 }; // 0x004D
    keymaps[KEYMAP_IT][KEY_KP1] = (keymap_t){ 0x4F31, 0x4F31, 0x4F00, 0x4F00 }; // 0x004F
    keymaps[KEYMAP_IT][KEY_KP2] = (keymap_t){ 0x5032, 0x5032, 0x5000, 0x5000 }; // 0x0050
    keymaps[KEYMAP_IT][KEY_KP3] = (keymap_t){ 0x5133, 0x5133, 0x5100, 0x5100 }; // 0x0051
    keymaps[KEYMAP_IT][KEY_KP0] = (keymap_t){ 0x5230, 0x5230, 0x5200, 0x5200 }; // 0x0052

    keymaps[KEYMAP_IT][KEY_KP_DEC] = (keymap_t){ 0x532E, 0x532E, 0x5300, 0x5300 }; // 0x0053

#if 0

    keymaps[KEYMAP_IT].base[KEY_ESCAPE]        = 27;
    keymaps[KEYMAP_IT].base[KEY_ONE]           = '1';
    keymaps[KEYMAP_IT].base[KEY_TWO]           = '2';
    keymaps[KEYMAP_IT].base[KEY_THREE]         = '3';
    keymaps[KEYMAP_IT].base[KEY_FOUR]          = '4';
    keymaps[KEYMAP_IT].base[KEY_FIVE]          = '5';
    keymaps[KEYMAP_IT].base[KEY_SIX]           = '6';
    keymaps[KEYMAP_IT].base[KEY_SEVEN]         = '7';
    keymaps[KEYMAP_IT].base[KEY_EIGHT]         = '8';
    keymaps[KEYMAP_IT].base[KEY_NINE]          = '9';
    keymaps[KEYMAP_IT].base[KEY_ZERO]          = '0';
    keymaps[KEYMAP_IT].base[KEY_APOSTROPHE]    = '\'';
    keymaps[KEYMAP_IT].base[KEY_I_ACC]         = 141;
    keymaps[KEYMAP_IT].base[KEY_BACKSPACE]     = '\b';
    keymaps[KEYMAP_IT].base[KEY_TAB]           = '\t';
    keymaps[KEYMAP_IT].base[KEY_Q]             = 'q';
    keymaps[KEYMAP_IT].base[KEY_W]             = 'w';
    keymaps[KEYMAP_IT].base[KEY_E]             = 'e';
    keymaps[KEYMAP_IT].base[KEY_R]             = 'r';
    keymaps[KEYMAP_IT].base[KEY_T]             = 't';
    keymaps[KEYMAP_IT].base[KEY_Y]             = 'y';
    keymaps[KEYMAP_IT].base[KEY_U]             = 'u';
    keymaps[KEYMAP_IT].base[KEY_I]             = 'i';
    keymaps[KEYMAP_IT].base[KEY_O]             = 'o';
    keymaps[KEYMAP_IT].base[KEY_P]             = 'p';
    keymaps[KEYMAP_IT].base[KEY_LEFT_BRAKET]   = 138;
    keymaps[KEYMAP_IT].base[KEY_RIGHT_BRAKET]  = '+';
    keymaps[KEYMAP_IT].base[KEY_ENTER]         = 13;
    keymaps[KEYMAP_IT].base[KEY_S]             = 's';
    keymaps[KEYMAP_IT].base[KEY_D]             = 'd';
    keymaps[KEYMAP_IT].base[KEY_F]             = 'f';
    keymaps[KEYMAP_IT].base[KEY_G]             = 'g';
    keymaps[KEYMAP_IT].base[KEY_H]             = 'h';
    keymaps[KEYMAP_IT].base[KEY_J]             = 'j';
    keymaps[KEYMAP_IT].base[KEY_K]             = 'k';
    keymaps[KEYMAP_IT].base[KEY_L]             = 'l';
    keymaps[KEYMAP_IT].base[KEY_SEMICOLON]     = 149;
    keymaps[KEYMAP_IT].base[KEY_DOUBLE_QUOTES] = 133;
    keymaps[KEYMAP_IT].base[KEY_GRAVE]         = '\\';
    keymaps[KEYMAP_IT].base[KEY_BACKSLASH]     = 151;
    keymaps[KEYMAP_IT].base[KEY_Z]             = 'z';
    keymaps[KEYMAP_IT].base[KEY_X]             = 'x';
    keymaps[KEYMAP_IT].base[KEY_C]             = 'c';
    keymaps[KEYMAP_IT].base[KEY_V]             = 'v';
    keymaps[KEYMAP_IT].base[KEY_B]             = 'b';
    keymaps[KEYMAP_IT].base[KEY_N]             = 'n';
    keymaps[KEYMAP_IT].base[KEY_M]             = 'm';
    keymaps[KEYMAP_IT].base[KEY_COMMA]         = ',';
    keymaps[KEYMAP_IT].base[KEY_PERIOD]        = '.';
    keymaps[KEYMAP_IT].base[KEY_MINUS]         = '-';
    keymaps[KEYMAP_IT].base[KEY_KP_MUL]        = '*';
    keymaps[KEYMAP_IT].base[KEY_SPACE]         = ' ';
    keymaps[KEYMAP_IT].base[KEY_KP_SUB]        = '-';
    keymaps[KEYMAP_IT].base[KEY_KP_ADD]        = '+';
    keymaps[KEYMAP_IT].base[KEY_KP_LESS]       = '<';
    keymaps[KEYMAP_IT].base[KEY_KP_DIV]        = '/';

    keymaps[KEYMAP_IT].shift[KEY_ONE]           = '!';
    keymaps[KEYMAP_IT].shift[KEY_TWO]           = '"';
    keymaps[KEYMAP_IT].shift[KEY_THREE]         = 156;
    keymaps[KEYMAP_IT].shift[KEY_FOUR]          = '$';
    keymaps[KEYMAP_IT].shift[KEY_FIVE]          = '%';
    keymaps[KEYMAP_IT].shift[KEY_SIX]           = '&';
    keymaps[KEYMAP_IT].shift[KEY_SEVEN]         = '/';
    keymaps[KEYMAP_IT].shift[KEY_EIGHT]         = '(';
    keymaps[KEYMAP_IT].shift[KEY_NINE]          = ')';
    keymaps[KEYMAP_IT].shift[KEY_ZERO]          = '=';
    keymaps[KEYMAP_IT].shift[KEY_APOSTROPHE]    = '?';
    keymaps[KEYMAP_IT].shift[KEY_I_ACC]         = '^';
    keymaps[KEYMAP_IT].shift[KEY_Q]             = 'Q';
    keymaps[KEYMAP_IT].shift[KEY_W]             = 'W';
    keymaps[KEYMAP_IT].shift[KEY_E]             = 'E';
    keymaps[KEYMAP_IT].shift[KEY_R]             = 'R';
    keymaps[KEYMAP_IT].shift[KEY_T]             = 'T';
    keymaps[KEYMAP_IT].shift[KEY_Y]             = 'Y';
    keymaps[KEYMAP_IT].shift[KEY_U]             = 'U';
    keymaps[KEYMAP_IT].shift[KEY_I]             = 'I';
    keymaps[KEYMAP_IT].shift[KEY_O]             = 'O';
    keymaps[KEYMAP_IT].shift[KEY_P]             = 'P';
    keymaps[KEYMAP_IT].shift[KEY_LEFT_BRAKET]   = 130;
    keymaps[KEYMAP_IT].shift[KEY_RIGHT_BRAKET]  = '*';
    keymaps[KEYMAP_IT].shift[KEY_A]             = 'A';
    keymaps[KEYMAP_IT].shift[KEY_S]             = 'S';
    keymaps[KEYMAP_IT].shift[KEY_D]             = 'D';
    keymaps[KEYMAP_IT].shift[KEY_F]             = 'F';
    keymaps[KEYMAP_IT].shift[KEY_G]             = 'G';
    keymaps[KEYMAP_IT].shift[KEY_H]             = 'H';
    keymaps[KEYMAP_IT].shift[KEY_J]             = 'J';
    keymaps[KEYMAP_IT].shift[KEY_K]             = 'K';
    keymaps[KEYMAP_IT].shift[KEY_L]             = 'L';
    keymaps[KEYMAP_IT].shift[KEY_SEMICOLON]     = 128;
    keymaps[KEYMAP_IT].shift[KEY_DOUBLE_QUOTES] = 167;
    keymaps[KEYMAP_IT].shift[KEY_GRAVE]         = '|';
    keymaps[KEYMAP_IT].shift[KEY_Z]             = 'Z';
    keymaps[KEYMAP_IT].shift[KEY_X]             = 'X';
    keymaps[KEYMAP_IT].shift[KEY_C]             = 'C';
    keymaps[KEYMAP_IT].shift[KEY_V]             = 'V';
    keymaps[KEYMAP_IT].shift[KEY_B]             = 'B';
    keymaps[KEYMAP_IT].shift[KEY_N]             = 'N';
    keymaps[KEYMAP_IT].shift[KEY_M]             = 'M';
    keymaps[KEYMAP_IT].shift[KEY_COMMA]         = ';';
    keymaps[KEYMAP_IT].shift[KEY_PERIOD]        = ':';
    keymaps[KEYMAP_IT].shift[KEY_MINUS]         = '_';
    keymaps[KEYMAP_IT].shift[KEY_KP_MUL]        = '*';
    keymaps[KEYMAP_IT].shift[KEY_SPACE]         = ' ';
    keymaps[KEYMAP_IT].shift[KEY_KP_SUB]        = '-';
    keymaps[KEYMAP_IT].shift[KEY_KP_ADD]        = '+';
    keymaps[KEYMAP_IT].shift[KEY_KP_LESS]       = '>';
    keymaps[KEYMAP_IT].shift[KEY_KP_DIV]        = '/';

    keymaps[KEYMAP_IT].alt[KEY_LEFT_BRAKET]   = '[';
    keymaps[KEYMAP_IT].alt[KEY_RIGHT_BRAKET]  = ']';
    keymaps[KEYMAP_IT].alt[KEY_SEMICOLON]     = '@';
    keymaps[KEYMAP_IT].alt[KEY_DOUBLE_QUOTES] = '#';

    keymaps[KEYMAP_IT].numlock[KEY_KP_DEC] = '.';
    keymaps[KEYMAP_IT].numlock[KEY_KP0]    = '0';
    keymaps[KEYMAP_IT].numlock[KEY_KP1]    = '1';
    keymaps[KEYMAP_IT].numlock[KEY_KP2]    = '2';
    keymaps[KEYMAP_IT].numlock[KEY_KP3]    = '3';
    keymaps[KEYMAP_IT].numlock[KEY_KP4]    = '4';
    keymaps[KEYMAP_IT].numlock[KEY_KP5]    = '5';
    keymaps[KEYMAP_IT].numlock[KEY_KP6]    = '6';
    keymaps[KEYMAP_IT].numlock[KEY_KP7]    = '7';
    keymaps[KEYMAP_IT].numlock[KEY_KP8]    = '8';
    keymaps[KEYMAP_IT].numlock[KEY_KP9]    = '9';

    // == US KEY MAPPING ======================================================

    keymaps[KEYMAP_US].base[KEY_ESCAPE]        = 27;
    keymaps[KEYMAP_US].base[KEY_ONE]           = '1';
    keymaps[KEYMAP_US].base[KEY_TWO]           = '2';
    keymaps[KEYMAP_US].base[KEY_THREE]         = '3';
    keymaps[KEYMAP_US].base[KEY_FOUR]          = '4';
    keymaps[KEYMAP_US].base[KEY_FIVE]          = '5';
    keymaps[KEYMAP_US].base[KEY_SIX]           = '6';
    keymaps[KEYMAP_US].base[KEY_SEVEN]         = '7';
    keymaps[KEYMAP_US].base[KEY_EIGHT]         = '8';
    keymaps[KEYMAP_US].base[KEY_NINE]          = '9';
    keymaps[KEYMAP_US].base[KEY_ZERO]          = '0';
    keymaps[KEYMAP_US].base[KEY_APOSTROPHE]    = '-';
    keymaps[KEYMAP_US].base[KEY_I_ACC]         = '=';
    keymaps[KEYMAP_US].base[KEY_BACKSPACE]     = '\b';
    keymaps[KEYMAP_US].base[KEY_TAB]           = '\t';
    keymaps[KEYMAP_US].base[KEY_Q]             = 'q';
    keymaps[KEYMAP_US].base[KEY_W]             = 'w';
    keymaps[KEYMAP_US].base[KEY_E]             = 'e';
    keymaps[KEYMAP_US].base[KEY_R]             = 'r';
    keymaps[KEYMAP_US].base[KEY_T]             = 't';
    keymaps[KEYMAP_US].base[KEY_Y]             = 'y';
    keymaps[KEYMAP_US].base[KEY_U]             = 'u';
    keymaps[KEYMAP_US].base[KEY_I]             = 'i';
    keymaps[KEYMAP_US].base[KEY_O]             = 'o';
    keymaps[KEYMAP_US].base[KEY_P]             = 'p';
    keymaps[KEYMAP_US].base[KEY_LEFT_BRAKET]   = '[';
    keymaps[KEYMAP_US].base[KEY_RIGHT_BRAKET]  = ']';
    keymaps[KEYMAP_US].base[KEY_ENTER]         = 13;
    keymaps[KEYMAP_US].base[KEY_A]             = 'a';
    keymaps[KEYMAP_US].base[KEY_S]             = 's';
    keymaps[KEYMAP_US].base[KEY_D]             = 'd';
    keymaps[KEYMAP_US].base[KEY_F]             = 'f';
    keymaps[KEYMAP_US].base[KEY_G]             = 'g';
    keymaps[KEYMAP_US].base[KEY_H]             = 'h';
    keymaps[KEYMAP_US].base[KEY_J]             = 'j';
    keymaps[KEYMAP_US].base[KEY_K]             = 'k';
    keymaps[KEYMAP_US].base[KEY_L]             = 'l';
    keymaps[KEYMAP_US].base[KEY_SEMICOLON]     = ';';
    keymaps[KEYMAP_US].base[KEY_DOUBLE_QUOTES] = '\'';
    keymaps[KEYMAP_US].base[KEY_GRAVE]         = '`';
    keymaps[KEYMAP_US].base[KEY_BACKSLASH]     = '\\';
    keymaps[KEYMAP_US].base[KEY_Z]             = 'z';
    keymaps[KEYMAP_US].base[KEY_X]             = 'x';
    keymaps[KEYMAP_US].base[KEY_C]             = 'c';
    keymaps[KEYMAP_US].base[KEY_V]             = 'v';
    keymaps[KEYMAP_US].base[KEY_B]             = 'b';
    keymaps[KEYMAP_US].base[KEY_N]             = 'n';
    keymaps[KEYMAP_US].base[KEY_M]             = 'm';
    keymaps[KEYMAP_US].base[KEY_COMMA]         = ',';
    keymaps[KEYMAP_US].base[KEY_PERIOD]        = '.';
    keymaps[KEYMAP_US].base[KEY_MINUS]         = '/';
    keymaps[KEYMAP_US].base[KEY_KP_MUL]        = '*';
    keymaps[KEYMAP_US].base[KEY_SPACE]         = ' ';
    keymaps[KEYMAP_US].base[KEY_KP_SUB]        = '-';
    keymaps[KEYMAP_US].base[KEY_KP_ADD]        = '+';
    keymaps[KEYMAP_US].base[KEY_KP_LESS]       = '<';
    keymaps[KEYMAP_US].base[KEY_KP_DIV]        = '/';

    keymaps[KEYMAP_US].shift[KEY_ONE]           = '!';
    keymaps[KEYMAP_US].shift[KEY_TWO]           = '@';
    keymaps[KEYMAP_US].shift[KEY_THREE]         = '#';
    keymaps[KEYMAP_US].shift[KEY_FOUR]          = '$';
    keymaps[KEYMAP_US].shift[KEY_FIVE]          = '%';
    keymaps[KEYMAP_US].shift[KEY_SIX]           = '^';
    keymaps[KEYMAP_US].shift[KEY_SEVEN]         = '&';
    keymaps[KEYMAP_US].shift[KEY_EIGHT]         = '*';
    keymaps[KEYMAP_US].shift[KEY_NINE]          = '(';
    keymaps[KEYMAP_US].shift[KEY_ZERO]          = ')';
    keymaps[KEYMAP_US].shift[KEY_APOSTROPHE]    = '_';
    keymaps[KEYMAP_US].shift[KEY_I_ACC]         = '+';
    keymaps[KEYMAP_US].shift[KEY_Q]             = 'Q';
    keymaps[KEYMAP_US].shift[KEY_W]             = 'W';
    keymaps[KEYMAP_US].shift[KEY_E]             = 'E';
    keymaps[KEYMAP_US].shift[KEY_R]             = 'R';
    keymaps[KEYMAP_US].shift[KEY_T]             = 'T';
    keymaps[KEYMAP_US].shift[KEY_Y]             = 'Y';
    keymaps[KEYMAP_US].shift[KEY_U]             = 'U';
    keymaps[KEYMAP_US].shift[KEY_I]             = 'I';
    keymaps[KEYMAP_US].shift[KEY_O]             = 'O';
    keymaps[KEYMAP_US].shift[KEY_P]             = 'P';
    keymaps[KEYMAP_US].shift[KEY_LEFT_BRAKET]   = '{';
    keymaps[KEYMAP_US].shift[KEY_RIGHT_BRAKET]  = '}';
    keymaps[KEYMAP_US].shift[KEY_A]             = 'A';
    keymaps[KEYMAP_US].shift[KEY_S]             = 'S';
    keymaps[KEYMAP_US].shift[KEY_D]             = 'D';
    keymaps[KEYMAP_US].shift[KEY_F]             = 'F';
    keymaps[KEYMAP_US].shift[KEY_G]             = 'G';
    keymaps[KEYMAP_US].shift[KEY_H]             = 'H';
    keymaps[KEYMAP_US].shift[KEY_J]             = 'J';
    keymaps[KEYMAP_US].shift[KEY_K]             = 'K';
    keymaps[KEYMAP_US].shift[KEY_L]             = 'L';
    keymaps[KEYMAP_US].shift[KEY_SEMICOLON]     = ':';
    keymaps[KEYMAP_US].shift[KEY_DOUBLE_QUOTES] = '"';
    keymaps[KEYMAP_US].shift[KEY_GRAVE]         = '~';
    keymaps[KEYMAP_US].shift[KEY_Z]             = 'Z';
    keymaps[KEYMAP_US].shift[KEY_X]             = 'X';
    keymaps[KEYMAP_US].shift[KEY_C]             = 'C';
    keymaps[KEYMAP_US].shift[KEY_V]             = 'V';
    keymaps[KEYMAP_US].shift[KEY_B]             = 'B';
    keymaps[KEYMAP_US].shift[KEY_N]             = 'N';
    keymaps[KEYMAP_US].shift[KEY_M]             = 'M';
    keymaps[KEYMAP_US].shift[KEY_COMMA]         = '<';
    keymaps[KEYMAP_US].shift[KEY_PERIOD]        = '>';
    keymaps[KEYMAP_US].shift[KEY_MINUS]         = '?';
    keymaps[KEYMAP_US].shift[KEY_KP_MUL]        = '*';
    keymaps[KEYMAP_US].shift[KEY_SPACE]         = ' ';
    keymaps[KEYMAP_US].shift[KEY_KP_SUB]        = '-';
    keymaps[KEYMAP_US].shift[KEY_KP_ADD]        = '+';
    keymaps[KEYMAP_US].shift[KEY_KP_LESS]       = '>';
    keymaps[KEYMAP_US].shift[KEY_KP_DIV]        = '/';

    keymaps[KEYMAP_US].numlock[KEY_KP_DEC] = '.';
    keymaps[KEYMAP_US].numlock[KEY_KP0]    = '0';
    keymaps[KEYMAP_US].numlock[KEY_KP1]    = '1';
    keymaps[KEYMAP_US].numlock[KEY_KP2]    = '2';
    keymaps[KEYMAP_US].numlock[KEY_KP3]    = '3';
    keymaps[KEYMAP_US].numlock[KEY_KP4]    = '4';
    keymaps[KEYMAP_US].numlock[KEY_KP5]    = '5';
    keymaps[KEYMAP_US].numlock[KEY_KP6]    = '6';
    keymaps[KEYMAP_US].numlock[KEY_KP7]    = '7';
    keymaps[KEYMAP_US].numlock[KEY_KP8]    = '8';
    keymaps[KEYMAP_US].numlock[KEY_KP9]    = '9';
#endif
}

/// @}