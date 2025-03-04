/// @file stdio.h
/// @brief Standard I/0 functions.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "stdarg.h"
#include "stddef.h"

/// @brief The maximum number of digits of an integer.
#define MAX_DIGITS_IN_INTEGER 11
/// @brief The size of 'gets' buffer.
#define GETS_BUFFERSIZE       255

#ifndef EOF
/// @brief Define the End-Of-File.
#define EOF (-1)
#endif

#define SEEK_SET 0 ///< The file offset is set to offset bytes.
#define SEEK_CUR 1 ///< The file offset is set to its current location plus offset bytes.
#define SEEK_END 2 ///< The file offset is set to the size of the file plus offset bytes.

#ifndef __KERNEL__
/// @brief Writes the given character to the standard output (stdout).
/// @param character The character to send to stdout.
void putchar(int character);

/// @brief Writes the string pointed by str to the standard output (stdout)
///        and appends a newline character.
/// @param str The string to send to stdout.
void puts(const char *str);

/// @brief Returns the next character from the standard input (stdin).
/// @return The character received from stdin.
int getchar(void);

/// @brief Reads characters from the standard input (stdin).
/// @param str Where the characters are stored.
/// @return The string received from standard input.
char *gets(char *str);

/// @brief Same as getchar but reads from the given file descriptor.
/// @param fd The file descriptor from which it reads.
/// @return The read character.
int fgetc(int fd);

/// @brief Same as gets but reads from the given file descriptor.
/// @param buf The buffer where the string should be placed.
/// @param n   The amount of characters to read.
/// @param fd  The file descriptor from which it reads.
/// @return The read string.
char *fgets(char *buf, int n, int fd);
#endif

/// @brief Convert the given string to an integer.
/// @param str The string to convert.
/// @return The integer contained inside the string.
int atoi(const char *str);

/// @brief Converts the initial part of `str` to a long int value according to
///        the given base, which.
/// @param str    This is the string containing the integral number.
/// @param endptr Set to the character after the numerical value.
/// @param base   The base must be between 2 and 36 inclusive, or special 0.
/// @return Integral number as a long int value, else zero value is returned.
long strtol(const char *str, char **endptr, int base);

/// @brief Write formatted output to stdout.
/// @param format The format string.
/// @param ... The list of arguments.
/// @return On success, the total number of characters written is returned.
///         On failure, a negative number is returned.
int printf(const char *format, ...);

/// @brief Write formatted output to `str`.
/// @param str The buffer where the formatted string will be placed.
/// @param format  Format string, following the same specifications as printf.
/// @param ... The list of arguments.
/// @return On success, the total number of characters written is returned.
///         On failure, a negative number is returned.
int sprintf(char *str, const char *format, ...);

/// @brief Writes formatted output to `str`.
/// @param str The buffer where the formatted string will be placed.
/// @param size The size of the buffer.
/// @param format The format string, following the same specifications as printf.
/// @param ... The list of arguments.
/// @return On success, the total number of characters written (excluding the null terminator) is returned.
///         On failure, a negative number is returned.
int snprintf(char *str, size_t size, const char *format, ...);

#ifndef __KERNEL__
/// @brief Write formatted output to a file.
/// @param fd  The file descriptor associated with the file.
/// @param format  Format string, following the same specifications as printf.
/// @param ... The list of arguments.
/// @return On success, the total number of characters written is returned.
///         On failure, a negative number is returned.
int fprintf(int fd, const char *format, ...);

/// @brief Write formatted data from variable argument list to a file.
/// @param fd  The file descriptor associated with the file.
/// @param format  Format string, following the same specifications as printf.
/// @param args A variable arguments list.
/// @return On success, the total number of characters written is returned.
///         On failure, a negative number is returned.
int vfprintf(int fd, const char *format, va_list args);
#endif

/// @brief Formats a string and ensures buffer boundaries are respected.
/// @param str The output buffer where the formatted string will be stored.
/// @param size The maximum size of the output buffer.
/// @param format The format string.
/// @param args The argument list for the format specifiers.
/// @return int The number of characters written, excluding the null-terminator.
int vsnprintf(char *str, size_t size, const char *format, va_list args);

/// @brief Write formatted data from variable argument list to string.
/// @param str  Pointer to a buffer where the resulting C-string is stored.
/// @param format  Format string, following the same specifications as printf.
/// @param args A variable arguments list.
/// @return On success, the total number of characters written is returned.
///         On failure, a negative number is returned.
int vsprintf(char *str, const char *format, va_list args);

#ifndef __KERNEL__
/// @brief Read formatted input from stdin.
/// @param format  Format string, following the same specifications as printf.
/// @param ... The list of arguments where the values are stored.
/// @return On success, the function returns the number of items of the
///         argument list successfully filled. EOF otherwise.
int scanf(const char *format, ...);

/// @brief Read formatted data from string.
/// @param str String processed as source to retrieve the data.
/// @param format  Format string, following the same specifications as printf.
/// @param ... The list of arguments where the values are stored.
/// @return On success, the function returns the number of items of the
///         argument list successfully filled. EOF otherwise.
int sscanf(const char *str, const char *format, ...);

/// @brief The same as sscanf but the source is a file.
/// @param fd  The file descriptor associated with the file.
/// @param format  Format string, following the same specifications as printf.
/// @param ... The list of arguments where the values are stored.
/// @return On success, the function returns the number of items of the
///         argument list successfully filled. EOF otherwise.
int fscanf(int fd, const char *format, ...);
#endif

/// @brief Prints a system error message.
/// @param s the message we prepend to the actual error message.
void perror(const char *s);
