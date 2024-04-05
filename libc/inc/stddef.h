/// @file stddef.h
/// @brief Define basic data types.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#ifndef NULL
/// @brief Define NULL.
#define NULL ((void *)0)
#endif

#ifndef EOF
/// @brief Define End-Of-File.
#define EOF (-1)
#endif

/// @brief Define the size of a buffer.
#define BUFSIZ 512

/// Is the signed integer type of the result of subtracting two pointers.
typedef long signed int ptrdiff_t;

/// Define the byte type.
typedef unsigned char byte_t;

/// Define the generic size type.
typedef unsigned long size_t;

/// Define the generic signed size type.
typedef long ssize_t;

/// Define the type of an inode.
typedef unsigned int ino_t;

/// Used for device IDs.
typedef unsigned int dev_t;

/// The type of user-id.
typedef int uid_t;

/// The type of group-id.
typedef int gid_t;

/// The type of offset.
typedef long int off_t;

/// The type of mode.
typedef unsigned int mode_t;

/// This data-type is used to set protection bits of pages.
typedef unsigned int pgprot_t;

/// It evaluates to the offset (in bytes) of a given member within
/// a struct or union type, an expression of type size_t.
#define offsetof(type, member) \
    ((size_t) & (((type *)0)->member))

/// Retrieve an enclosing structure from a pointer to a nested element.
#if 1
#define container_of(ptr, type, member) \
    ((type *)((char *)(1 ? (ptr) : &((type *)0)->member) - offsetof(type, member)))
#else
#define container_of(ptr, type, member) \
    ((type *)((char *)ptr - offsetof(type, member)))
#endif

/// Returns the alignment, in bytes, of the specified type.
#define alignof(type) offsetof( \
    struct { char c; type member; }, member)

/// Counts the number of elements of an array.
#define count_of(x) ((sizeof(x) / sizeof((x)[0])) / ((size_t)(!(sizeof(x) % sizeof((x)[0])))))

/// @brief Swaps two values.
/// @details
/// do { ... } while (0) allows to place multistatement operations, for
/// instance, inside the following if stantement:
/// if (condition)
///     swap(var1, var2)
/// which otherwise would be wrong.
#define swap(a, b)             \
    do {                       \
        typeof(a) temp = (a);  \
        (a)            = (b);  \
        (b)            = temp; \
    } while (0)
