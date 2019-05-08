///                MentOS, The Mentoring Operating system project
/// @file stddef.h
/// @brief Define basic data types.
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#ifndef NULL

/// @brief Define NULL.
#define NULL ((void*)0)

#endif

#ifndef EOF

/// @brief Define End-Of-File.
#define EOF (-1)

#endif

/// @brief Define the size of a buffer.
#define BUFSIZ 512

/// @brief Define the size of the kernel.
#define KERNEL_SIZE 0x200

#ifndef TRUE

/// @brief Define the value of true.
#define TRUE 1

#endif

#ifndef FALSE

/// @brief Define the value of false.
#define FALSE 0

#endif

/// @brief Define the byte type.
typedef unsigned char byte_t;

/// @brief Define the generic size type.
typedef unsigned long size_t;

/// @brief Define the generic signed size type.
typedef long ssize_t;

/// @brief Define the type of an inode.
typedef unsigned int ino_t;

// TODO: doxygen comment.
typedef unsigned int dev_t;

/// @brief The type of user-id.
typedef unsigned int uid_t;

/// @brief The type of group-id.
typedef unsigned int gid_t;

/// @brief The type of offset.
typedef unsigned int off_t;

/// @brief The type of mode.
typedef unsigned int mode_t;

// TODO: doxygen comment.
typedef mode_t pgprot_t;

// TODO: doxygen comment.
typedef unsigned long int ulong;

// TODO: doxygen comment.
typedef unsigned short int ushort;

// TODO: doxygen comment.
typedef unsigned int uint;

// TODO: doxygen comment.
typedef int key_t;

// TODO: doxygen comment.
// Check with clock.h
typedef long __time_t;
