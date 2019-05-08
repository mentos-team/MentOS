///                MentOS, The Mentoring Operating system project
/// @file ctype.h
/// @brief Functions related to character handling.
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

/// @brief Check if the given value is a digit.
int isdigit(int c);

/// @brief Check if the given value is a letter.
int isalpha(int c);

/// @brief Check if the given value is either a letter or a digit.
int isalnum(int c);

/// @brief Check if the given value is an hexadecimal digit.
int isxdigit(int c);

/// @brief Check if the given value is a lower case letter.
int islower(int c);

/// @brief Check if the given value is an upper case letter.
int isupper(int c);

/// @brief Transforms the given value into a lower case letter.
int tolower(int c);

/// @brief Transforms the given value into an upper case letter.
int toupper(int c);

/// @brief Check if the given value is a whitespace.
int isspace(int c);
