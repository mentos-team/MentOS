/// @file string.h
/// @brief String routines.
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "stddef.h"

/// @brief Copies the first num characters of source to destination.
/// @param destination Pointer to the destination array where the content is to be copied.
/// @param source      String to be copied.
/// @param num         Maximum number of characters to be copied from source.
/// @return destination is returned.
char *strncpy(char *destination, const char *source, size_t num);

/// @brief Compares up to n characters of s1 to those of s2.
/// @param s1 First string to be compared.
/// @param s2 Second string to be compared.
/// @param n  Maximum number of characters to compare.
/// @return
/// Returns an integral value indicating the relationship between the strings:
///     <0 the first character that does not match has a lower
///        value in s1 than in s2
///      0 the contents of both strings are equal
///     >0 the first character that does not match has a greater
///        value in s1 than in s2
int strncmp(const char *s1, const char *s2, size_t n);

/// @brief Case insensitive string compare.
/// @param s1 First string to be compared.
/// @param s2 Second string to be compared.
/// @return
/// Returns an integral value indicating the relationship between the strings:
///     <0 the first character that does not match has a lower
///        value in s1 than in s2
///      0 the contents of both strings are equal
///     >0 the first character that does not match has a greater
///        value in s1 than in s2
int stricmp(const char *s1, const char *s2);

/// @brief Case-insensitively compare up to n characters of s1 to those of s2.
/// @param s1 First string to be compared.
/// @param s2 Second string to be compared.
/// @param n  Maximum number of characters to compare.
/// @return
/// Returns an integral value indicating the relationship between the strings:
///     <0 the first character that does not match has a lower
///        value in s1 than in s2
///      0 the contents of both strings are equal
///     >0 the first character that does not match has a greater
///        value in s1 than in s2
int strnicmp(const char *s1, const char *s2, size_t n);

/// @brief Returns a pointer to the first occurrence of ch in str.
/// @param s  The string where the search is performed.
/// @param ch Character to be located.
/// @return A pointer to the first occurrence of character in str.
char *strchr(const char *s, int ch);

/// @brief Returns a pointer to the last occurrence of ch in str.
/// @param s  The string where the search is performed.
/// @param ch Character to be located.
/// @return A pointer to the last occurrence of character in str.
char *strrchr(const char *s, int ch);

/// @brief Returns a pointer to the first occurrence of s2 in s1,
///        or NULL if s2 is not part of s1.
/// @param s1 String to be scanned
/// @param s2 String containing the sequence of characters to match.
/// @return A pointer to the first occurrence in s1 of the entire
///         sequence of characters specified in s2, or a null pointer
///         if the sequence is not present in s1.
char *strstr(const char *s1, const char *s2);

/// @brief Returns the length of the initial portion of string which consists
///        only of characters that are part of control.
/// @param string  String to be scanned.
/// @param control String containing the characters to match.
/// @return The number of characters in the initial segment of string which
///         consist only of characters from control.
size_t strspn(const char *string, const char *control);

/// @brief Calculates the length of the initial segment of string which
///        consists entirely of characters not in control.
/// @param string  String to be scanned.
/// @param control String containing the characters to match.
/// @return The number of characters in the initial segment of string which
///         consist only of characters that are not inside control.
size_t strcspn(const char *string, const char *control);

/// @brief Finds the first character in the string string that matches any
///        character specified in control.
/// @param string  String to be scanned.
/// @param control String containing the characters to match.
/// @return
/// A pointer to the first occurrence in string of any of the characters
/// that are part of control, or a null pointer if none of the characters
/// of control is found in string before the terminating null-character.
char *strpbrk(const char *string, const char *control);

/// @brief Make a copy of the given string.
/// @param s String to duplicate.
/// @return On success, returns a pointer to the duplicated string.
///         On failure, returns NULL with errno indicating the cause.
char *strdup(const char *s);

/// @brief Make a copy of at most n bytes of the given string.
/// @param s String to duplicate.
/// @param n The number of character to duplicate.
/// @return On success, returns a pointer to the duplicated string.
///         On failure, returns NULL with errno indicating the cause.
char *strndup(const char *s, size_t n);

/// @brief Appends a copy of the string src to the string dst.
/// @param dst Pointer to the destination array, which should be large enough
///            to contain the concatenated resulting string.
/// @param src String to be appended. This should not overlap dst.
/// @return destination is returned.
char *strcat(char *dst, const char *src);

/// @brief Appends a copy of the string src to the string dst, up to n bytes.
/// @param dst Pointer to the destination array, which should be large enough
///            to contain the concatenated resulting string.
/// @param src String to be appended. This should not overlap dst.
/// @param n   The number of bytes to copy.
/// @return destination is returned.
char *strncat(char *dst, const char *src, size_t n);

/// @brief Fill the string s with the character c.
/// @param s The string that you want to fill.
/// @param c The character that you want to fill the string with.
/// @return The address of the string, s.
char *strset(char *s, int c);

/// @brief Fill the string s with the character c, up to the given length n.
/// @param s The string that you want to fill.
/// @param c The character that you want to fill the string with.
/// @param n The maximum number of bytes to fill.
/// @return The address of the string, s.
char *strnset(char *s, int c, size_t n);

/// @brief Reverse the string s.
/// @param s The given string which is needed to be reversed.
/// @return The address of the string, s.
char *strrev(char *s);

/// @brief Splits string into tokens.
/// @param str   String to truncate.
/// @param delim String containing the delimiter characters.
/// @return If a token is found, a pointer to the beginning of the token.
///         Otherwise, a null pointer.
/// @details
/// Notice that str is modified by being broken into smaller strings (tokens).
/// A null pointer may be specified, in which case the function continues
/// scanning where a previous successful call to the function ended.
char *strtok(char *str, const char *delim);

/// @brief This function is a reentrant version strtok().
/// @param str     String to truncate.
/// @param delim   String containing the delimiter characters.
/// @param saveptr Pointer used internally to maintain context between calls.
/// @return
/// @details
/// The saveptr argument is a pointer to a char * variable that is used
/// internally by strtok_r() in order to maintain context between successive
/// calls that parse the same string.
/// On the first call to strtok_r(), str should point to the string to be
/// parsed, and the value of saveptr is ignored. In subsequent calls, str
/// should be NULL, and saveptr should be unchanged since the previous call.
char *strtok_r(char *str, const char *delim, char **saveptr);

/// @brief Copies the values of num bytes from the location pointed by source
/// to the memory block pointed by destination.
/// @param dst Pointer to the destination array where the content is to be
///            copied, type-casted to a pointer of type void*.
/// @param src Pointer to the source of data to be copied, type-casted to
///            a pointer of type const void*.
/// @param n   Number of bytes to copy.
/// @return A pointer to dst is returned.
void *memmove(void *dst, const void *src, size_t n);

/// @brief Searches for the first occurrence of the character c (an unsigned
///        char) in the first n bytes of the string pointed to, by the
///        argument str.
/// @param ptr Pointer to the block of memory where the search is performed.
/// @param c   Value to be located.
/// @param n   Number of bytes to be analyzed.
/// @return A pointer to the first occurrence of value in the block of memory.
void *memchr(const void *ptr, int c, size_t n);

/// @brief Converts a given string into lowercase.
/// @param s String which we want to convert into lowercase.
/// @return A pointer to s.
char *strlwr(char *s);

/// @brief Converts a given string into uppercase.
/// @param s String which we want to convert into uppercase.
/// @return A pointer to s.
char *strupr(char *s);

/// @brief The memccpy function copies no more than n bytes from memory
///        area src to memory area dest, stopping when the character c is
///        found.
/// @param dst Points to the destination memory area.
/// @param src Points to the source memory area.
/// @param c   The delimiter used to stop.
/// @param n   The maximum number of copied bytes.
/// @return A pointer to the next character in dst after c, or NULL if c
///         was not found in the first n characters of src.
void *memccpy(void *dst, const void *src, int c, size_t n);

/// @brief Copy a block of memory, handling overlap.
/// @param dst Pointer to the destination.
/// @param src Pointer to the source.
/// @param num  Number of bytes to be copied.
/// @return Pointer to the destination.
void *memcpy(void *dst, const void *src, size_t num);

/// @brief Compares the first n bytes of str1 and str2.
/// @param ptr1 First pointer to block of memory.
/// @param ptr2 Second pointer to block of memory.
/// @param n    Number of bytes to compare.
/// @return
/// Returns an integral value indicating the relationship between
/// the memory blocks:
///     <0 the first byte that does not match has a lower
///        value in ptr1 than in ptr2
///      0 the contents of both memory blocks are equal
///     >0 the first byte that does not match has a greater
///        value in ptr1 than in ptr2
int memcmp(const void *ptr1, const void *ptr2, size_t n);

/// @brief Sets the first num bytes of the block of memory pointed by ptr
///        to the specified value.
/// @param ptr   Pointer to the block of memory to set.
/// @param value Value to be set.
/// @param num   Number of bytes to be set to the given value.
/// @return The same ptr.
void *memset(void *ptr, int value, size_t num);

/// @brief Copy the string src into the array dst.
/// @param dst The destination array where the content is to be copied.
/// @param src String to be copied.
/// @return A pointer to dst is returned.
char *strcpy(char *dst, const char *src);

/// @brief Checks if the two strings are equal.
/// @param s1 First string to be compared.
/// @param s2 Second string to be compared.
/// @return
/// Returns an integral value indicating the relationship between the strings:
///     <0 the first character that does not match has a lower
///        value in s1 than in s2
///      0 the contents of both strings are equal
///     >0 the first character that does not match has a greater
///        value in s1 than in s2
int strcmp(const char *s1, const char *s2);

/// @brief Returns the length of the string s.
/// @param s Pointer to the null-terminated byte string to be examined.
/// @return The length of the null-terminated string str.
size_t strlen(const char *s);

/// @brief Returns the number of characters inside s, excluding the
///        terminating null byte ('\0'), but at most count.
/// @param s Pointer to the null-terminated byte string to be examined.
/// @param maxlen The upperbound on the length.
/// @return Returns strlen(s), if that is less than maxlen, or maxlen
///         if there is no null terminating ('\0') among the first maxlen
///         characters pointed to by s.
size_t strnlen(const char *s, size_t maxlen);

/// @brief Removes any whitespace characters from the beginning and end of str.
/// @param str The string to trim.
/// @return A pointer to str.
char *trim(char *str);

/// @brief Separate the given string based on a given delimiter.
/// @param stringp The string to separate.
/// @param delim   The delimiter used to separate the string.
/// @return Returns a pointer to stringp.
/// @details
/// Finds the first token in stringp, that is delimited by one of the bytes in
/// the string delim. This token is terminated by overwriting the delimiter
/// with a null byte ('\0'), and *stringp is updated to point past the token.
/// In case no delimiter was found, the token is taken to be the entire string
/// *stringp, and *stringp is made NULL.
char *strsep(char **stringp, const char *delim);

/// @brief Move the number "num" into a string.
/// @param buffer The string containing the number.
/// @param num    The number to convert.
/// @param base   The base used to convert.
/// @return A pointer to buffer.
char *itoa(char *buffer, unsigned int num, unsigned int base);

/// @brief Replaces the occurrences of find with replace inside str.
/// @param str     The string to manipulate.
/// @param find    The character to replace.
/// @param replace The character used to replace.
/// @return A pointer to str.
char *replace_char(char *str, char find, char replace);

/// @brief Converts a file mode (the type and permission information associated
///        with an inode) into a symbolic string which is stored in the location
///        referenced by p.
/// @param mode File mode that encodes access permissions and file type.
/// @param p    Buffer used to hold the string representation of file mode m.
void strmode(mode_t mode, char *p);
