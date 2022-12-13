/// @file keymap.h
/// @brief Keymap for keyboard.
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.
/// @addtogroup drivers Device Drivers
/// @{
/// @addtogroup keyboard Keyboard
/// @{

#pragma once

#include "stdint.h"

/// @brief The list of supported keyboard layouts.
typedef enum {
    KEYMAP_IT,      ///< Identifies the IT keyboard mapping.
    KEYMAP_US,      ///< Identifies the US keyboard mapping.
    KEYMAP_TYPE_MAX ///< The delimiter for the keyboard types.
} keymap_type_t;

/// @brief Defines the mapping of a single scancode towards a set of characters.
typedef struct keymap_t {
    /// The basic mapping.
    int normal;
    /// The mapping when shifted.
    int shift;
    /// The mapping when ctrl is pressed.
    int ctrl;
    /// The mapping when alt is pressed.
    int alt;
    /// The mapping when numlock is active.
    int numlock;
} keymap_t;

/// @brief Returns the current keymap type.
/// @return The current keymap type.
keymap_type_t get_keymap_type();

/// @brief Changes the current keymap type.
/// @param type The type to set.
void set_keymap_type(keymap_type_t type);


/// @brief Returns the current keymap for the given scancode.
/// @param scancode the scancode we want.
/// @return Pointer to the keymap.
const keymap_t *get_keymap(int scancode);

/// @brief Initializes the supported keymaps.
void init_keymaps();

/// @name Keyboard Codes
/// @brief This is the list of keyboard codes.
/// @{

#define KEY_ESCAPE        0x0001U ///< Escape character
#define KEY_1             0x0002U ///< 1
#define KEY_2             0x0003U ///< 2
#define KEY_3             0x0004U ///< 3
#define KEY_4             0x0005U ///< 4
#define KEY_5             0x0006U ///< 5
#define KEY_6             0x0007U ///< 6
#define KEY_7             0x0008U ///< 7
#define KEY_8             0x0009U ///< 8
#define KEY_9             0x000AU ///< 9
#define KEY_0             0x000BU ///< 0
#define KEY_APOSTROPHE    0x000CU ///< '
#define KEY_I_ACC         0x000DU ///< i'
#define KEY_BACKSPACE     0x000EU ///< Backspace
#define KEY_TAB           0x000FU ///< Tabular
#define KEY_Q             0x0010U ///< q
#define KEY_W             0x0011U ///< w
#define KEY_E             0x0012U ///< e
#define KEY_R             0x0013U ///< r
#define KEY_T             0x0014U ///< t
#define KEY_Y             0x0015U ///< y
#define KEY_U             0x0016U ///< u
#define KEY_I             0x0017U ///< i
#define KEY_O             0x0018U ///< o
#define KEY_P             0x0019U ///< p
#define KEY_LEFT_BRAKET   0x001AU ///< (
#define KEY_RIGHT_BRAKET  0x001BU ///< )
#define KEY_ENTER         0x001CU ///< Enter
#define KEY_LEFT_CONTROL  0x001DU ///< Left-ctrl
#define KEY_A             0x001EU ///< a
#define KEY_S             0x001FU ///< s
#define KEY_D             0x0020U ///< d
#define KEY_F             0x0021U ///< f
#define KEY_G             0x0022U ///< g
#define KEY_H             0x0023U ///< h
#define KEY_J             0x0024U ///< j
#define KEY_K             0x0025U ///< k
#define KEY_L             0x0026U ///< l
#define KEY_SEMICOLON     0x0027U ///< '
#define KEY_DOUBLE_QUOTES 0x0028U ///< "
#define KEY_GRAVE         0x0029U ///< Either ` or ~
#define KEY_LEFT_SHIFT    0x002AU ///< LShift
#define KEY_BACKSLASH     0x002BU ///< Either \ or |
#define KEY_Z             0x002cU ///< Z
#define KEY_X             0x002dU ///< X
#define KEY_C             0x002eU ///< C
#define KEY_V             0x002fU ///< V
#define KEY_B             0x0030U ///< B
#define KEY_N             0x0031U ///< N
#define KEY_M             0x0032U ///< M
#define KEY_COMMA         0x0033U ///< Either , or <
#define KEY_PERIOD        0x0034U ///< Either . or >
#define KEY_MINUS         0x0035U ///< Either - and _
#define KEY_RIGHT_SHIFT   0x0036U ///< RShift
#define KEY_KP_MUL        0x0037U ///< NP - *
#define KEY_LEFT_ALT      0x0038U ///< LAlt
#define KEY_SPACE         0x0039U ///< Space
#define KEY_CAPS_LOCK     0x003aU ///< Caps Lock
#define KEY_F1            0x003BU ///< Function 1
#define KEY_F2            0x003CU ///< Function 2
#define KEY_F3            0x003DU ///< Function 3
#define KEY_F4            0x003EU ///< Function 4
#define KEY_F5            0x003FU ///< Function 5
#define KEY_F6            0x0040U ///< Function 6
#define KEY_F7            0x0041U ///< Function 7
#define KEY_F8            0x0042U ///< Function 8
#define KEY_F9            0x0043U ///< Function 9
#define KEY_F10           0x0044U ///< Function 10
#define KEY_NUM_LOCK      0x0045U ///< Num Lock
#define KEY_SCROLL_LOCK   0x0046U ///< Scroll Lock
#define KEY_KP7           0x0047U ///< NP - Home
#define KEY_KP8           0x0048U ///< NP - UpArrow
#define KEY_KP9           0x0049U ///< NP - Pgup
#define KEY_KP_SUB        0x004AU ///< NP - Grey
#define KEY_KP4           0x004BU ///< NP - LArrow
#define KEY_KP5           0x004CU ///< NP - Center
#define KEY_KP6           0x004DU ///< NP - RArrow
#define KEY_KP_ADD        0x004EU ///< NP - Grey +
#define KEY_KP1           0x004FU ///< NP - End
#define KEY_KP2           0x0050U ///< NP - DArrow
#define KEY_KP3           0x0051U ///< NP - Pgdn
#define KEY_KP0           0x0052U ///< NP - Ins
#define KEY_KP_DEC        0x0053U ///< NP - Del
#define KEY_KP_LESS       0x0056U ///< NP - Del
#define KEY_F11           0x0057U ///< F11 57431
#define KEY_F12           0x0058U ///< F12 57432
#define KEY_KP_RETURN     0xE01cU ///< NP - Enter 57372
#define KEY_RIGHT_CONTROL 0xE01DU ///< Right Ctrl 57373
#define KEY_KP_DIV        0xE035U ///< Divide 57397
#define KEY_RIGHT_ALT     0xE038U ///< Right Alt 57400
#define KEY_LEFT_WIN      0xE05bU ///< Left Winkey 57435
#define KEY_RIGHT_WIN     0xE05cU ///< Right Winkey 57436
#define KEY_INSERT        0xE052U ///< Ins 57426
#define KEY_HOME          0xE047U ///< Home 57415
#define KEY_UP_ARROW      0xE048U ///< Up Arrow 57416
#define KEY_PAGE_UP       0xE049U ///< Pgup 57417
#define KEY_LEFT_ARROW    0xE04bU ///< Left Arrow 57419
#define KEY_DELETE        0xE053U ///< Del 57427
#define KEY_END           0xE04fU ///< End 57423
#define KEY_PAGE_DOWN     0xE051U ///< Pgdn 57425
#define KEY_RIGHT_ARROW   0xE04dU ///< Right Arrow 57421
#define KEY_DOWN_ARROW    0xE050U ///< Down Arrow 57424
#define CODE_BREAK        0x0080U ///< Code break code

#define MULTIMEDIA_SCAN_CODE 0xE0
/// @}

/// @}
/// @}
