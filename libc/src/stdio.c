/// @file stdio.c
/// @brief Standard I/0 functions.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "sys/errno.h"
#include "ctype.h"
#include "stdbool.h"
#include "stdio.h"
#include "strerror.h"
#include "string.h"
#include "unistd.h"
#include "limits.h"

void putchar(int character)
{
    write(STDOUT_FILENO, &character, 1U);
}

void puts(const char *str)
{
    write(STDOUT_FILENO, str, strlen(str));
}

int getchar(void)
{
    char c = 0;
    while (read(STDIN_FILENO, &c, 1) == 0) {
        continue;
    }
    return c;
}

char *gets(char *str)
{
    // Check the input string.
    if (str == NULL) {
        return NULL;
    }
    // Buffer for reading input.
    char buffer[GETS_BUFFERSIZE];
    memset(buffer, '\0', GETS_BUFFERSIZE);
    // Char pointer to the buffer.
    char *cptr = buffer;
    // Character storage and counter to prevent overflow.
    int ch, counter = 0;
    // Read until we find a newline or we exceed the buffer size.
    while (((ch = getchar()) != '\n') && (counter++ < GETS_BUFFERSIZE)) {
        // If we encounter EOF, stop.
        if (ch == EOF) {
            // EOF at start of line return NULL.
            if (cptr == str) {
                return NULL;
            }
            break;
        }
        // Backspace key
        if (ch == '\b') {
            if (counter > 0) {
                counter--;
                --cptr;
                putchar('\b');
            }
        } else {
            // The character is stored at address, and the pointer is incremented.
            *cptr++ = (char)ch;
        }
    }
    // Add the null-terminating character.
    *cptr = '\0';
    // Copy the string we have read.
    strcpy(str, buffer);
    // Return a pointer to the original string.
    return str;
}

int atoi(const char *str)
{
    // Check the input string.
    if (str == NULL) {
        return 0;
    }
    // Initialize sign, the result variable, and two indices.
    int sign = (str[0] == '-') ? -1 : +1, result = 0, i;
    // Find where the number ends.
    for (i = (sign == -1) ? 1 : 0; (str[i] != '\0') && isdigit(str[i]); ++i) {
        result = (result * 10) + str[i] - '0';
    }
    return sign * result;
}

long strtol(const char *str, char **endptr, int base)
{
    const char *s;
    long acc, cutoff;
    int c;
    int neg, any, cutlim;
    // Skip white space and pick up leading +/- sign if any.
    // If base is 0, allow 0x for hex and 0 for octal, else
    // assume decimal; if base is already 16, allow 0x.
    s = str;
    do {
        c = (unsigned char)*s++;
    } while (isspace(c));
    if (c == '-') {
        neg = 1;
        c   = (int)*s++;
    } else {
        neg = 0;
        if (c == '+') {
            c = (int)*s++;
        }
    }
    if ((base == 0 || base == 16) &&
        c == '0' && (*s == 'x' || *s == 'X')) {
        c = (int)s[1];
        s += 2;
        base = 16;
    }
    if (base == 0) {
        base = c == '0' ? 8 : 10;
    }
    // Compute the cutoff value between legal numbers and illegal
    // numbers.  That is the largest legal value, divided by the
    // base.  An input number that is greater than this value, if
    // followed by a legal input character, is too big.  One that
    // is equal to this value may be valid or not; the limit
    // between valid and invalid numbers is then based on the last
    // digit.  For instance, if the range for longs is
    // [-2147483648..2147483647] and the input base is 10,
    // cutoff will be set to 214748364 and cutlim to either
    // 7 (neg==0) or 8 (neg==1), meaning that if we have accumulated
    // a value > 214748364, or equal but the next digit is > 7 (or 8),
    // the number is too big, and we will return a range error.
    //
    // Set any if any `digits' consumed; make it negative to indicate
    // overflow.
    cutoff = neg ? LONG_MIN : LONG_MAX;
    cutlim = cutoff % base;
    cutoff /= base;
    if (neg) {
        if (cutlim > 0) {
            cutlim -= base;
            cutoff += 1;
        }
        cutlim = -cutlim;
    }
    for (acc = 0, any = 0;; c = (unsigned char)*s++) {
        if (isdigit(c)) {
            c -= '0';
        } else if (isalpha(c)) {
            c -= isupper(c) ? 'A' - 10 : 'a' - 10;
        } else {
            break;
        }
        if (c >= base) {
            break;
        }
        if (any < 0) {
            continue;
        }
        if (neg) {
            if (acc < cutoff || (acc == cutoff && c > cutlim)) {
                any   = -1;
                acc   = LONG_MIN;
                errno = ERANGE;
            } else {
                any = 1;
                acc *= base;
                acc -= c;
            }
        } else {
            if (acc > cutoff || (acc == cutoff && c > cutlim)) {
                any   = -1;
                acc   = LONG_MAX;
                errno = ERANGE;
            } else {
                any = 1;
                acc *= base;
                acc += c;
            }
        }
    }
    if (endptr != 0) {
        *endptr = (char *)(any ? s - 1 : str);
    }
    return (acc);
}

int fgetc(int fd)
{
    char c;
    ssize_t bytes_read;

    // Read a single character from the file descriptor.
    bytes_read = read(fd, &c, 1);

    // Check for errors or EOF.
    if (bytes_read == -1) {
        perror("Error reading from file descriptor");
        return EOF; // Return EOF on error.
    } else if (bytes_read == 0) {
        return EOF; // Return EOF if no bytes were read (end of file).
    }

    // Return the character as an unsigned char.
    return (unsigned char)c;
}

char *fgets(char *buf, int n, int fd)
{
    int c;
    char *p   = buf;
    int count = n - 1; // Leave space for null terminator

    // Read characters until reaching the limit or newline
    while (count > 0) {
        ssize_t bytes_read = read(fd, &c, 1); // Read one character

        if (bytes_read < 0) {
            perror("Error reading from file descriptor");
            return NULL; // Return NULL on error
        } else if (bytes_read == 0) {
            // End of file
            break;
        }

        *p++ = (char)c; // Store the character in the buffer

        if (c == '\n') {
            break; // Stop if we reach a newline
        }
        count--;
    }

    *p = '\0'; // Null-terminate the string

    if (p == buf || c == EOF) {
        return NULL; // Return NULL if no characters were read or EOF was reached
    }

    return buf; // Return the buffer
}

void perror(const char *s)
{
    if (s) {
        puts(s);
        putchar(':');
        putchar(' ');
    }
    puts(strerror(errno));
    putchar('\n');
}
