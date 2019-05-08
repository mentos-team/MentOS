///                MentOS, The Mentoring Operating system project
/// @file string.h
/// @brief String routines.
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "list.h"
#include "stddef.h"

/// @brief Copies the first num characters of source to destination.
char *strncpy(char *destination, const char *source, size_t num);

/// @brief Compares up to n characters of s1 to those of s2.
int strncmp(const char *s1, const char *s2, size_t n);

/// @brief Case insensitive string compare.
int stricmp(const char *s1, const char *s2);

/// @brief Case-insensitively compare up to n characters of s1 to those of s2.
int strnicmp(const char *s1, const char *s2, size_t n);

/// @brief Returns a pointer to the first occurrence of ch in str.
char *strchr(const char *s, int ch);

/// @brief Returns a pointer to the last occurrence of ch in str.
char *strrchr(const char *s, int ch);

/// @brief Returns a pointer to the first occurrence of s2 in s1, or NULL if
///        s2 is not part of s1.
char *strstr(const char *s1, const char *s2);

/// @brief Returns the length of the initial portion of string which consists
///        only of characters that are part of control.
size_t strspn(const char *string, const char *control);

/// @brief Calculates the length of the initial segment of string which
///        consists entirely of characters not in control.
size_t strcspn(const char *string, const char *control);

/// @brief Finds the first character in the string string that matches any
///        character specified in control.
char *strpbrk(const char *string, const char *control);

/// @brief Make a copy of the given string.
char *strdup(const char *s);

/// @brief Make a copy of the given string.
char *kstrdup(const char *s);

/// @brief Appends the string pointed to, by s2 to the end of the string
///        pointed to, by s1 up to n characters long.
char *strncat(char *s1, const char *s2, size_t n);

/// @brief Fill the string s with the character c, to the given length n.
char *strnset(char *s, int c, size_t n);

/// @brief Fill the string s with the character c.
char *strset(char *s, int c);

/// @brief Reverse the string s.
char *strrev(char *s);

// TODO: Check behaviour & doxygen comment.
char *strtok(char *str, const char *delim);

// TODO: Check behaviour & doxygen comment.
char *strtok_r(char *string, const char *control, char **lasts);

/// @brief Another function to copy n characters from str2 to str1.
void *memmove(void *dst, const void *src, size_t n);

/// @brief Searches for the first occurrence of the character c (an unsigned
///        char) in the first n bytes of the string pointed to, by the
///        argument str.
void *memchr(const void *str, int c, size_t n);

char *strlwr(char *s);

char *strupr(char *s);

/// @brief Copies the first n bytes from memory area src to memory area dest,
///        stopping when the character c is found.
void *memccpy(void *dst, const void *src, int c, size_t n);

/// @brief      Copy a block of memory, handling overlap.
/// @param _dst Pointer to the destination.
/// @param _src Pointer to the source.
/// @param num  Number of bytes to be copied.
/// @return     Pointer to the destination.
void *memcpy(void *_dst, const void *_src, size_t num);

/// @brief Compares the first n bytes of str1 and str2.
int memcmp(const void *str1, const void *str2, size_t n);

/// @brief       Sets the first num bytes of the block of memory pointed by ptr
///              to the specified value.
/// @param ptr   Pointer to the block of memory to set.
/// @param value Value to be set.
/// @param num   Number of bytes to be set to the given value.
/// @return      The same ptr.
void *memset(void *ptr, int value, size_t num);

/// @brief Copy the string src into the array dst.
char *strcpy(char *dst, const char *src);

/// @brief Appends a copy of the string src to the string dst.
char *strcat(char *dst, const char *src);

/// @brief Checks if the two strings are equal.
int strcmp(const char *s1, const char *s2);

/// @brief Returns the lenght of the string s.
size_t strlen(const char *s);

/// @brief Returns the number of characters inside s, excluding the
///        terminating null byte ('\0'), but at most count.
size_t strnlen(const char *s, size_t count);

/// @brief Separate a string in token according to the delimiter.
///        If str is NULL, the scanning will continue for the previous string.
///        It can be bettered.
char *strtok(char *str, const char *delim);

/// @brief Compare num characters of s2 and s1.
int _kstrncmp(const char *s1, const char *s2, size_t num);

/// @brief Removes the whitespaces in from of str.
char *trim(char *str);

/// @brief Create a copy of str.
char *strdup(const char *src);

/// @brief Separate stringp based on delim.
char *strsep(char **stringp, const char *delim);

/// @brief Split a string into list of strings.
list_t *str_split(const char *str, const char *delim, unsigned int *num);

/// @brief Reconstruct a string from list using delim as delimiter.
char *list2str(list_t *list, const char *delim);

/// @brief        Move the number "num" into a string.
/// @param buffer The string containing the number.
/// @param num    The number to convert.
/// @param base   The base used to convert.
void int_to_str(char *buffer, unsigned int num, unsigned int base);

/// @brief Transforms num into a string.
void _knntos(char *buffer, int num, int base);

/// @brief Replaces the occurences of find with replace inside str.
char *replace_char(char *str, char find, char replace);

/// @brief Converts a file mode (the type and permission information associated
///        with an inode) into a symbolic string which is stored in the location
///        referenced by p.
void strmode(mode_t mode, char *p);
