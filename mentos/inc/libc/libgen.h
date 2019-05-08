///                MentOS, The Mentoring Operating system project
/// @file libgen.h
/// @brief String routines.
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "stddef.h"

// TODO: doxygen comment.
/// @brief
/// @param out
/// @param cur
/// @param sep
/// @param max
/// @result
int parse_path(char *out, char **cur, char sep, size_t max);

// TODO: doxygen comment.
/// @brief
/// @param path
/// @result
char *dirname(const char *path);

// TODO: doxygen comment.
/// @brief
/// @param path
/// @result
char *basename(const char *path);
