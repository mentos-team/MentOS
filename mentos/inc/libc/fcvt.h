///                MentOS, The Mentoring Operating system project
/// @file fcvt.h
/// @brief
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

// TODO: doxygen comment.
/// @brief
/// @param arg
/// @param ndigits
/// @param decpt
/// @param sign
/// @param buf
/// @result
char *ecvtbuf(double arg, int ndigits, int *decpt, int *sign, char *buf);

// TODO: doxygen comment.
/// @brief
/// @param arg
/// @param ndigits
/// @param decpt
/// @param sign
/// @param buf
/// @result
char *fcvtbuf(double arg, int ndigits, int *decpt, int *sign, char *buf);
