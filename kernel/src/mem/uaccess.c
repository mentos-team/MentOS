/// @file uaccess.c
/// @brief The boundary between the kernel and a caller's address space.
/// @copyright (c) 2014-2026 This file is distributed under the MIT License.
/// See LICENSE.md for details.

// Setup the logging for this file (do this before any other include).
#include "sys/kernel_levels.h"           // Include kernel log levels.
#define __DEBUG_HEADER__ "[UACCES]"      ///< Change header.
#define __DEBUG_LEVEL__  LOGLEVEL_NOTICE ///< Set log level.
#include "io/debug.h"                    // Include debugging functions.

#include "errno.h"
#include "mem/paging.h"
#include "mem/uaccess.h"
#include "string.h"

int access_ok(uaccess_dir_t dir, const void *addr, size_t size)
{
    if (dir == USER_WRITE) {
        return paging_is_user_range_writable(addr, size);
    }
    return paging_is_user_range(addr, size);
}

int copy_from_user(void *dst, const void *src, size_t size)
{
    if (!access_ok(USER_READ, src, size)) {
        return -EFAULT;
    }
    memcpy(dst, src, size);
    return 0;
}

int copy_to_user(void *dst, const void *src, size_t size)
{
    if (!access_ok(USER_WRITE, dst, size)) {
        return -EFAULT;
    }
    memcpy(dst, src, size);
    return 0;
}

long strncpy_from_user(char *dst, const char *src, size_t size)
{
    // A zero-sized destination cannot even hold a terminator, so there is no
    // answer to give that is not a lie about what was copied.
    if (size == 0) {
        return -ENAMETOOLONG;
    }
    // The measurement is bounded by what the destination can hold, so a
    // source longer than that is reported rather than truncated: a truncated
    // path names a different file, and silently resolving the wrong one is
    // worse than failing (#284).
    long length = strnlen_user(src, size);
    if (length < 0) {
        return length;
    }
    memcpy(dst, src, (size_t)length);
    dst[length] = 0;
    return length;
}

long strnlen_user(const char *str, size_t maxlen)
{
    // The length of a string handed in by a caller cannot be learned with
    // strlen: that is the unbounded walk this whole exercise exists to
    // prevent. Instead the string is followed one page at a time, and each
    // page must prove to be the caller's memory before a single byte of it
    // is read.
    uintptr_t cursor = (uintptr_t)str;
    size_t scanned   = 0;
    while (scanned < maxlen) {
        if (!access_ok(USER_READ, (const void *)(cursor + scanned), 1)) {
            return -EFAULT;
        }
        uintptr_t page_end = ((cursor + scanned) & ~(uintptr_t)(PAGE_SIZE - 1)) + PAGE_SIZE;
        size_t available   = page_end - (cursor + scanned);
        if (available > (maxlen - scanned)) {
            available = maxlen - scanned;
        }
        size_t length = strnlen((const char *)(cursor + scanned), available);
        if (length < available) {
            // The terminator is inside this chunk.
            return (long)(scanned + length);
        }
        scanned += available;
    }
    // No terminator within the maximum: an unterminated string names
    // nothing the caller may ask the kernel to walk.
    return -ENAMETOOLONG;
}
