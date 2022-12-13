/// @file libgen.h
/// @brief String routines.
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
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

/// @brief Return the canonicalized absolute pathname.
/// @param path     The path we are canonicalizing.
/// @param resolved The canonicalized path.
/// @return
/// If there is no error, realpath() returns a pointer to the resolved.
/// Otherwise, it returns NULL, the contents of the array resolved
/// are undefined, and errno is set to indicate the error.
/// @details
/// If resolved is NULL, then realpath() uses malloc
/// to allocate a buffer of up to PATH_MAX bytes to hold the
/// resolved pathname, and returns a pointer to this buffer.
char *realpath(const char *path, char *resolved);
