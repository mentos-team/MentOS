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
keymap_t keymaps[KEYMAP_TYPE_MAX];

keymap_type_t get_keymap_type()
{
    return keymap_type;
}

void set_keymap_type(keymap_type_t type)
{
    if (type != keymap_type)
        keymap_type = type;
}

const keymap_t *get_keymap()
{
    return &keymaps[keymap_type];
}

void init_keymaps()
{
    for (int i = 0; i < KEYMAP_TYPE_MAX; ++i)
        memset(&keymaps[i], -1, sizeof(keymap_t));

    // == ITALIAN KEY MAPPING =================================================
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
    keymaps[KEYMAP_IT].base[KEY_A]             = 'a';
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
}

/// @}