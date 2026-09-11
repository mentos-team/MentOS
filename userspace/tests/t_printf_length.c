/// @file t_printf_length.c
/// @brief Regression test for #332: length modifiers must consume and print
/// their argument correctly.
/// @details vsprintf parsed no length modifier at all, so `%lld` printed as
/// the literal `%l` followed by a `d`, consumed no argument, and left every
/// later argument in the call to be read from the wrong offset. The parsing
/// that replaced it still read `ll` arguments with `va_arg(long)` — four
/// bytes instead of eight — so the cursor stayed misaligned, and the `z`
/// modifier truncated its argument through a stray `(short)` cast, so even
/// `%zu` printed at most sixteen bits. None of the number paths could hold a
/// value wider than `unsigned long`.
///
/// Each case formats into a buffer with snprintf and compares against the
/// expected text, so the check is on the output and not on the return value
/// alone. The mixed-conversion cases are the ones that catch a misaligned
/// argument cursor: a modifier that consumes the wrong width reads the next
/// conversion's argument instead of its own.
/// @copyright (c) 2014-2026 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

/// @brief Formats with snprintf and compares the result with the expectation.
/// @param what human-readable description of the conversion under test.
/// @param expected the text the conversion has to produce.
/// @param format the format string to check.
/// The test stops at the first mismatch: a misaligned cursor tends to corrupt
/// every later case as well, so the first divergence is the informative one.
#define CHECK_FMT(what, expected, format, ...)                                            \
    do {                                                                                   \
        char buffer[256];                                                                  \
        snprintf(buffer, sizeof(buffer), format, __VA_ARGS__);                             \
        if (strcmp(buffer, expected) != 0) {                                               \
            syslog(LOG_ERR, "[t_printf_length] %s: got `%s`, expected `%s`", what, buffer, \
                   expected);                                                              \
            closelog();                                                                    \
            exit(EXIT_FAILURE);                                                            \
        }                                                                                  \
        syslog(LOG_INFO, "[t_printf_length] %s: `%s`", what, buffer);                      \
    } while (0)

int main(void)
{
    openlog("t_printf_length", LOG_PID | LOG_CONS, LOG_USER);

    // `ll` must read eight bytes and print the whole value: these do not fit
    // in the four bytes the broken paths consumed.
    CHECK_FMT("%lld over 32 bits", "5000000000", "%lld", 5000000000LL);
    CHECK_FMT("%lld negative over 32 bits", "-9000000000", "%lld", -9000000000LL);
    CHECK_FMT("%llu max", "18446744073709551615", "%llu", 18446744073709551615ULL);
    CHECK_FMT("%llx over 32 bits", "deadbeefcafe", "%llx", 0xdeadbeefcafeULL);

    // A wrong-width read shifts the argument cursor: the conversions after a
    // broken one read somebody else's argument. These are the cases #332 is
    // named after.
    CHECK_FMT("%lld then %d", "1 then 42", "%lld then %d", 1LL, 42);
    CHECK_FMT("%d then %lld then %d", "7 then 5000000000 then 9", "%d then %lld then %d", 7, 5000000000LL, 9);
    CHECK_FMT("%llu then %s", "18446744073709551615 then tail", "%llu then %s", 18446744073709551615ULL, "tail");

    // Flags, width and precision keep working on the wide paths.
    CHECK_FMT("%08lld", "00001234", "%08lld", 1234LL);
    CHECK_FMT("%-8lld|", "1234    |", "%-8lld|", 1234LL);
    CHECK_FMT("%+lld", "+5000000000", "%+lld", 5000000000LL);

    // `z` must read a full size_t/ssize_t, not a truncated one. The casts use
    // the compiler's own types rather than the ones `stddef.h` declares: the
    // format check compares against `__SIZE_TYPE__`, which the two cross
    // toolchains spell differently (`unsigned int` on the i686-elf gcc of the
    // Linux job, `long unsigned int` on the one Homebrew ships) even though
    // both are 32 bits here, so a fixed cast is right on one of them only.
    CHECK_FMT("%zu", "4", "%zu", sizeof(long));
    CHECK_FMT("%zd negative", "-7", "%zd", -(__PTRDIFF_TYPE__)7);
    CHECK_FMT("%zx", "ff", "%zx", (__SIZE_TYPE__)0xff);

    // `l` keeps its meaning.
    CHECK_FMT("%lu", "1234567890", "%lu", 1234567890UL);
    CHECK_FMT("%ld negative", "-1234567890", "%ld", -1234567890L);

    // `h` and `hh` truncate on purpose; the widths still match the promoted
    // argument, so the cursor stays aligned.
    CHECK_FMT("%hd wraps", "4464", "%hd", 70000);
    CHECK_FMT("%hhd wraps", "44", "%hhd", 300);
    CHECK_FMT("%hhu wraps", "44", "%hhu", 300);
    CHECK_FMT("%hx", "beef", "%hx", 0xdeadbeef);

    closelog();
    return EXIT_SUCCESS;
}
