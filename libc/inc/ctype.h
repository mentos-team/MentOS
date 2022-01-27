/// @file ctype.h
/// @brief Functions related to character handling.
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

/// @brief Check if the given value is a digit.
/// @param c The input character.
/// @return 1 on success, 0 otherwise.
int isdigit(int c);

/// @brief Check if the given value is a letter.
/// @param c The input character.
/// @return 1 on success, 0 otherwise.
int isalpha(int c);

/// @brief Check if the given value is a control character.
/// @param c The input character.
/// @return 1 on success, 0 otherwise.
int iscntrl(int c);

/// @brief Check if the given value is either a letter or a digit.
/// @param c The input character.
/// @return 1 on success, 0 otherwise.
int isalnum(int c);

/// @brief Check if the given value is an hexadecimal digit.
/// @param c The input character.
/// @return 1 on success, 0 otherwise.
int isxdigit(int c);

/// @brief Check if the given value is a lower case letter.
/// @param c The input character.
/// @return 1 on success, 0 otherwise.
int islower(int c);

/// @brief Check if the given value is an upper case letter.
/// @param c The input character.
/// @return 1 on success, 0 otherwise.
int isupper(int c);

/// @brief Transforms the given value into a lower case letter.
/// @param c The input letter.
/// @return The input letter turned into a lower case letter.
int tolower(int c);

/// @brief Transforms the given value into an upper case letter.
/// @param c The input letter.
/// @return The input letter turned into an upper case letter.
int toupper(int c);

/// @brief Check if the given value is a whitespace.
/// @param c The input character.
/// @return 1 on success, 0 otherwise.
int isspace(int c);
