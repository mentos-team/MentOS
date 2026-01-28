/// @file libgen.h
/// @brief String routines.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "stddef.h"

/// @brief Extracts the parent directory of the given path and saves it inside
/// the given buffer, e.g., from "/home/user/test.txt" it extracts "/home/user".
/// If the path does not contain a '/', it will return ".".
/// @param path the path we are parsing.
/// @param buffer the buffer where we save the directory name.
/// @param buflen the length of the buffer.
/// @return 1 if succesfull, or 0 if the buffer cannot contain the path.
int dirname(const char *path, char *buffer, size_t buflen);

/// @brief Extract the component after the final '/'.
/// @param path the path from which we extract the final component.
/// @return a pointer after the final '/', or path itself it none was found.
const char *basename(const char *path);

/// @brief Return the canonicalized absolute pathname.
/// @param path the path we are canonicalizing.
/// @param buffer where we will store the canonicalized path.
/// @param buflen the size of the buffer.
/// @return If there is no error, realpath() returns a pointer to the buffer.
/// Otherwise, it returns NULL, the contents of the array buffer are undefined,
/// and errno is set to indicate the error.
char *realpath(const char *path, char *buffer, size_t buflen);
