/// @file libgen.h
/// @brief String routines.
/// @copyright (c) 2014-2023 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "stddef.h"

/// @brief Extracts the parent directory of the given path and saves it inside
/// the given buffer, e.g., from "/home/user/test.txt" it extracts "/home/user".
/// @param path the path we are parsing.
/// @param buffer the buffer where we save the directory name.
/// @param buflen the length of the buffer.
/// @return 1 if succesfull, or 0 if the buffer cannot contain the path.
int dirname(const char *path, char *buffer, size_t buflen);

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
