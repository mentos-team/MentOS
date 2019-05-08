///                MentOS, The Mentoring Operating system project
/// @file   keymap.h
/// @brief  Keymap for keyboard.
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "stdint.h"

/// @defgroup keyboardcodes Keyboard Codes
/// @brief This is the list of keyboard codes.
/// @{

/// Escape character.
#define KEY_ESCAPE 0x01
/// 1.
#define KEY_ONE 0x02
/// 2.
#define KEY_TWO 0x03
/// 3.
#define KEY_THREE 0x04
/// 4.
#define KEY_FOUR 0x05
/// 5.
#define KEY_FIVE 0x06
/// 6.
#define KEY_SIX 0x07
/// 7.
#define KEY_SEVEN 0x08
/// 8.
#define KEY_EIGHT 0x09
/// 9.
#define KEY_NINE 0x0A
/// 0.
#define KEY_ZERO 0x0B
/// '.
#define KEY_APOSTROPHE 0x0C
/// i'.
#define KEY_I_ACC 0x0D
/// Backspace.
#define KEY_BACKSPACE 0x0E
/// Tabular.
#define KEY_TAB 0x0F
/// q.
#define KEY_Q 0x10
/// w.
#define KEY_W 0x11
/// e.
#define KEY_E 0x12
/// r.
#define KEY_R 0x13
/// t.
#define KEY_T 0x14
/// y.
#define KEY_Y 0x15
/// u.
#define KEY_U 0x16
/// i.
#define KEY_I 0x17
/// o.
#define KEY_O 0x18
/// p.
#define KEY_P 0x19
/// (.
#define KEY_LEFT_BRAKET 0x1A
/// ).
#define KEY_RIGHT_BRAKET 0x1B
/// Enter.
#define KEY_ENTER 0x1C
/// Left-ctrl.
#define KEY_LEFT_CONTROL 0x1D
/// a.
#define KEY_A 0x1E
/// s.
#define KEY_S 0x1F
/// d.
#define KEY_D 0x20
/// f.
#define KEY_F 0x21
/// g.
#define KEY_G 0x22
/// h.
#define KEY_H 0x23
/// j.
#define KEY_J 0x24
/// k.
#define KEY_K 0x25
/// l.
#define KEY_L 0x26
/// '.
#define KEY_SEMICOLON 0x27
/// ".
#define KEY_DOUBLE_QUOTES 0x28
///` or ~.
#define KEY_GRAVE 0x29
/// LShift.
#define KEY_LEFT_SHIFT 0x2A
/// \ or |.
#define KEY_BACKSLASH 0x2B
/// Z.
#define KEY_Z 0x2c
/// X.
#define KEY_X 0x2d
/// C.
#define KEY_C 0x2e
/// V.
#define KEY_V 0x2f
/// B.
#define KEY_B 0x30
/// N.
#define KEY_N 0x31
/// M.
#define KEY_M 0x32
/// , or <.
#define KEY_COMMA 0x33
/// . or >.
#define KEY_PERIOD 0x34
/// - or _.
#define KEY_MINUS 0x35
/// RShift.
#define KEY_RIGHT_SHIFT 0x36
/// NP - *.
#define KEY_KP_MUL 0x37
/// LAlt.
#define KEY_LEFT_ALT 0x38
/// Space.
#define KEY_SPACE 0x39
/// Caps Lock.
#define KEY_CAPS_LOCK 0x3a
/// Function 1.
#define KEY_F1 0x3B
/// Function 2.
#define KEY_F2 0x3C
/// Function 3.
#define KEY_F3 0x3D
/// Function 4.
#define KEY_F4 0x3E
/// Function 5.
#define KEY_F5 0x3F
/// Function 6.
#define KEY_F6 0x40
/// Function 7.
#define KEY_F7 0x41
/// Function 8.
#define KEY_F8 0x42
/// Function 9.
#define KEY_F9 0x43
/// Function 10.
#define KEY_F10 0x44
/// Num Lock.
#define KEY_NUM_LOCK 0x45
/// Scroll Lock.
#define KEY_SCROLL_LOCK 0x46
/// NP - Home.
#define KEY_KP7 0x47
/// NP - UpArrow.
#define KEY_KP8 0x48
/// NP - Pgup.
#define KEY_KP9 0x49
/// NP - Grey.
#define KEY_KP_SUB 0x4A
/// NP - LArrow.
#define KEY_KP4 0x4B
/// NP - Center.
#define KEY_KP5 0x4C
/// NP - RArrow.
#define KEY_KP6 0x4D
/// NP - Grey +.
#define KEY_KP_ADD 0x4E
/// NP - End.
#define KEY_KP1 0x4F
/// NP - DArrow.
#define KEY_KP2 0x50
/// NP - Pgdn.
#define KEY_KP3 0x51
/// NP - Ins.
#define KEY_KP0 0x52
/// NP - Del.
#define KEY_KP_DEC 0x53
/// NP - Del.
#define KEY_KP_LESS 0x56
/// NP - Enter 57372.
#define KEY_KP_RETURN 0xe01c
/// Right Ctrl 57373.
#define KEY_RIGHT_CONTROL 0xE01D
/// Divide 57397.
#define KEY_KP_DIV 0xE035
/// Right Alt 57400.
#define KEY_RIGHT_ALT 0xe038
/// F11 57431.
#define KEY_F11 0xe057
/// F12 57432.
#define KEY_F12 0xe058
/// Left Winkey 57435.
#define KEY_LEFT_WIN 0xe05b
/// Right Winkey 57436.
#define KEY_RIGHT_WIN 0xe05c
/// Ins 57426.
#define KEY_INSERT 0xe052
/// Home 57415.
#define KEY_HOME 0xe047
/// Up Arrow 57416.
#define KEY_UP_ARROW 0xe048
/// Pgup 57417.
#define KEY_PAGE_UP 0xe049
/// Left Arrow 57419.
#define KEY_LEFT_ARROW 0xe04b
/// Del 57427.
#define KEY_DELETE 0xe053
/// End 57423.
#define KEY_END 0xe04f
/// Pgdn 57425.
#define KEY_PAGE_DOWN 0xe051
/// Right Arrow 57421.
#define KEY_RIGHT_ARROW 0xe04d
/// Down Arrow 57424.
#define KEY_DOWN_ARROW 0xe050
///< Code break code.
#define CODE_BREAK 0x80

/// @}

/// Num Pad Led.
#define NUM_LED 0x45
/// Scroll Lock Led.
#define SCROLL_LED 0x46
/// Caps Lock Led.
#define CAPS_LED 0x3a

/// @brief Defines a set of arrays used to map key to characters.
typedef struct keymap_t {
	/// The basic mapping.
	int32_t base[65536];
	/// The mapping when shifted.
	int32_t shift[65536];
	/// The mapping when numlock is active.
	uint32_t numlock[65536];
} keymap_t;

/// @brief Italian keymap.
extern const keymap_t keymap_it;
