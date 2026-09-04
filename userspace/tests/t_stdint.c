/// @file t_stdint.c
/// @brief Regression test for #270: the integer types are the widths they
/// are named for, and the 64-bit ones have the range the kernel relies on.
/// @details `lib/inc/stdint.h` defined the 64-bit types as 32-bit:
///
///     typedef int int64_t;
///     typedef unsigned int uint64_t;
///
/// Nothing could warn about it. A cast to a type that turns out to be
/// narrower than advertised is perfectly legal, so code that cast to
/// `uint64_t` specifically to gain range gained nothing and compiled clean.
/// What it cost:
///
///   - the ELF bounds checks in elf.c cast to `uint64_t` so that the sum
///     `offset + filesz` could not wrap, and the sum wrapped anyway;
///   - ext2 byte offsets `(uint64_t)block_index * block_size` truncated past
///     4 GiB;
///   - the ATA 48-bit sector count did not fit in the field meant to hold it,
///     and `ata_identity_t` came out 508 bytes instead of the 512 the drive
///     sends, so IDENTIFY read 254 of its 256 words.
///
/// The widths themselves are pinned at compile time, in `stdint.h` and next to
/// the ATA structure, so a future change to a typedef fails the build rather
/// than being caught here. What this test adds is the part a compile-time
/// check cannot express: that the arithmetic actually carries beyond 32 bits
/// at run time, on the same shape of expression the ELF checks are built from.
/// @copyright (c) 2014-2026 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>

/// @brief Checks that a type has the expected size.
/// @param name the name of the type, for the log.
/// @param actual its measured size.
/// @param expected the size it has to have.
/// @return 0 when they match, -1 otherwise.
static int __check_size(const char *name, unsigned actual, unsigned expected)
{
    if (actual != expected) {
        syslog(LOG_ERR, "[t_stdint] sizeof(%s) is %u, expected %u", name, actual, expected);
        return -1;
    }
    return 0;
}

int main(void)
{
    int failures = 0;

    // The 64-bit types are the point of the issue.
    failures += (__check_size("uint64_t", (unsigned)sizeof(uint64_t), 8) < 0);
    failures += (__check_size("int64_t", (unsigned)sizeof(int64_t), 8) < 0);

    // The narrower types are the control: widening everything would pass the
    // two checks above and break the on-disk and hardware layouts that the
    // whole kernel is built on.
    failures += (__check_size("uint8_t", (unsigned)sizeof(uint8_t), 1) < 0);
    failures += (__check_size("int8_t", (unsigned)sizeof(int8_t), 1) < 0);
    failures += (__check_size("uint16_t", (unsigned)sizeof(uint16_t), 2) < 0);
    failures += (__check_size("int16_t", (unsigned)sizeof(int16_t), 2) < 0);
    failures += (__check_size("uint32_t", (unsigned)sizeof(uint32_t), 4) < 0);
    failures += (__check_size("int32_t", (unsigned)sizeof(int32_t), 4) < 0);

    // The shape of the ELF check: a 32-bit value promoted so the sum cannot
    // wrap. `0xFFFFFFFF + 1` has to be 0x100000000, not 0.
    uint32_t near_max = UINT32_MAX;
    uint64_t widened  = (uint64_t)near_max + 1;
    if (widened != 0x100000000ULL) {
        syslog(LOG_ERR, "[t_stdint] (uint64_t)UINT32_MAX + 1 did not carry past 32 bits");
        ++failures;
    }

    // And the shape of the ext2 offset: a block index past 2^20 multiplied by
    // a 4 KiB block size, which used to truncate.
    uint32_t block_index = 2u * 1024u * 1024u;
    uint64_t offset      = (uint64_t)block_index * 4096u;
    if (offset != 8589934592ULL) {
        syslog(LOG_ERR, "[t_stdint] a block offset past 4 GiB did not fit a uint64_t");
        ++failures;
    }

    // The limits have to describe the types they are named for. UINT64_MAX
    // did not exist at all before this change.
    if ((UINT64_MAX + 1ULL) != 0ULL) {
        syslog(LOG_ERR, "[t_stdint] UINT64_MAX is not the largest unsigned 64-bit value");
        ++failures;
    }
    if ((INT64_MAX > 0) && ((INT64_MAX + INT64_MIN) != -1)) {
        syslog(LOG_ERR, "[t_stdint] INT64_MIN and INT64_MAX are not a matched pair");
        ++failures;
    }

    // Signed 64-bit arithmetic has to reach past 32 bits too, in both
    // directions: ext2 compares an offset against an inode size as ssize_t.
    int64_t negative = -(int64_t)block_index * 4096;
    if (negative != -8589934592LL) {
        syslog(LOG_ERR, "[t_stdint] signed 64-bit arithmetic truncated");
        ++failures;
    }

    if (failures == 0) {
        syslog(LOG_INFO, "[t_stdint] the integer types are the widths they are named for");
        return EXIT_SUCCESS;
    }
    syslog(LOG_ERR, "[t_stdint] %d FAILURES", failures);
    return EXIT_FAILURE;
}
