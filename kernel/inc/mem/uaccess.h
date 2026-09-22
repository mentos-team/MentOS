/// @file uaccess.h
/// @brief The boundary between the kernel and a caller's address space.
/// @copyright (c) 2014-2026 This file is distributed under the MIT License.
/// See LICENSE.md for details.
///
/// @details Everything the kernel reads from, or writes into, a pointer that
/// came from userspace goes through this header. That is the whole point of
/// it being a header of its own: the check and the copy are one call, so a
/// call site cannot perform the copy and forget the check. Reviewing for the
/// mistake becomes reviewing for a raw dereference of a caller pointer, which
/// is visible, instead of reviewing for an absent statement, which is not.
///
/// The mechanism underneath lives in mem/paging.h — those functions answer
/// whether a range belongs to the current task. This header is the policy
/// built on them, and is what callers outside the memory subsystem should
/// use (#401).
///
/// A caveat that matters for the future. These are check-then-use: the range
/// is validated, then the bytes move. Nothing can invalidate the mapping in
/// between, because there is one kernel stack, the scheduler only runs on a
/// trap from user mode, and no two tasks share an mm_struct. All three stop
/// holding once tasks can sleep in the kernel and share an address space
/// (#204), and at that point the copy has to recover from a fault rather
/// than predict one. Keeping every caller behind these four functions is
/// what makes that a change to this file instead of to every syscall.

#pragma once

#include "stddef.h"

/// @brief The direction of an access, from the kernel's point of view.
typedef enum uaccess_dir {
    USER_READ,  ///< The kernel reads from the caller's memory.
    USER_WRITE, ///< The kernel writes into the caller's memory.
} uaccess_dir_t;

/// @brief Tells whether the kernel may access a range of the caller's memory.
/// @param dir whether the kernel intends to read it or write it.
/// @param addr the start of the range, as handed to a syscall.
/// @param size the size of the range, in bytes.
/// @return 1 when the access is allowed, 0 otherwise.
/// @details The escape hatch, for a buffer the syscall does not copy itself
///          but hands to a layer below that writes into it directly — the
///          filesystem `read_f` and `getdents_f` are the cases that exist.
///          Anywhere the syscall moves the bytes, use `copy_from_user` or
///          `copy_to_user` instead: this function leaves the responsibility
///          with the caller, and the point of the other two is to take it
///          away.
int access_ok(uaccess_dir_t dir, const void *addr, size_t size);

/// @brief Copies a block out of the caller's memory.
/// @param dst the kernel destination.
/// @param src the caller's source.
/// @param size the number of bytes to copy.
/// @return 0 on success, -EFAULT when the source is not readable memory of
///         the current task.
int copy_from_user(void *dst, const void *src, size_t size);

/// @brief Copies a block into the caller's memory.
/// @param dst the caller's destination.
/// @param src the kernel source.
/// @param size the number of bytes to copy.
/// @return 0 on success, -EFAULT when the destination is not writable memory
///         of the current task.
/// @details The write direction is not the same check as the read one. The
///          kernel writes with supervisor rights and `CR0.WP` is clear, so
///          the hardware would not refuse a read-only user page; the refusal
///          happens here (#397).
int copy_to_user(void *dst, const void *src, size_t size);

/// @brief Copies a NUL-terminated string out of the caller's memory.
/// @param dst the kernel destination, which always ends up terminated on
///        success.
/// @param src the caller's string.
/// @param size the size of the destination, terminator included.
/// @return the length of the copied string without its terminator, -EFAULT
///         when a page of the source is not the caller's memory, or
///         -ENAMETOOLONG when no terminator appears within `size`.
long strncpy_from_user(char *dst, const char *src, size_t size);

/// @brief Measures a NUL-terminated string in the caller's memory, without
///        copying it.
/// @param str the caller's string.
/// @param maxlen the greatest length worth reporting.
/// @return the length without the terminator, -EFAULT when a page of it is
///         not the caller's memory, or -ENAMETOOLONG when no terminator
///         appears within `maxlen`.
/// @details Prefer `strncpy_from_user`. Measuring a string and then handing
///          the original pointer to the code that consumes it reads it twice,
///          and the second read is not the one that was checked (#287).
///          This exists for the call sites that still do that.
long strnlen_user(const char *str, size_t maxlen);
