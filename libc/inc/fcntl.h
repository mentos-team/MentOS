/// @file fcntl.h
/// @brief Headers of functions fcntl() and open().
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#define O_ACCMODE   0003      ///< Bits defining the open mode
#define O_RDONLY    00000000U ///< Open for reading only.
#define O_WRONLY    00000001U ///< Open for writing only.
#define O_RDWR      00000002U ///< Open for reading and writing.
#define O_CREAT     00000100U ///< Create if nonexistant.
#define O_EXCL      00000200U ///< Error if already exists.
#define O_TRUNC     00001000U ///< Truncate to zero length.
#define O_APPEND    00002000U ///< Set append mode.
#define O_NONBLOCK  00004000U ///< No delay.
#define O_DIRECTORY 00200000U ///< If file exists has no effect. Otherwise, the file is created.
