/// @file   stdio.c
/// @brief  Standard I/0 functions.
/// @date   Apr 2019

#include "stdio.h"
#include "video.h"
#include "ctype.h"
#include "string.h"
#include "keyboard.h"
#include "unistd.h"
#include "debug.h"

void putchar(int character)
{
	write(STDOUT_FILENO, &character, 1);
}

void puts(char *str)
{
	write(STDOUT_FILENO, str, strlen(str));
}

int getchar(void)
{
#if 1
	char c;
	while (true) {
		read(STDIN_FILENO, &c, 1);
		if (c != -1)
			break;
	}
	return c;
#else
	int tmpchar;
	while ((tmpchar = keyboard_getc()) == -1)
		;

	return tmpchar;
#endif
}

char *gets(char *str)
{
	int count = 0;
	char tmp[255];
	memset(tmp, '\0', 255);

	do {
		int c = getchar();

		// Return Key.
		if (c == '\n') {
			break;
		}
		// Backspace key.

		if (c == '\b') {
			if (count > 0) {
				count--;
			}
		} else {
			tmp[count++] = (char)c;
		}
	} while (count < 255);

	tmp[count] = '\0';
	/* tmp cant simply be returned, it is allocated in this stack frame and
     * it will be lost!
     */
	strcpy(str, tmp);

	return str;
}

int atoi(const char *str)
{
	// Initialize sign as positive.
	int sign = 1;

	// Initialize the result.
	int result = 0;

	// Initialize the index.
	int i = 0;

	// If the number is negative, then update the sign.
	if (str[0] == '-') {
		sign = -1;
	}

	// Check that the rest of the numbers are digits.
	for (i = (sign == -1) ? 1 : 0; str[i] != '\0'; ++i) {
		if (!isdigit(str[i])) {
			return -1;
		}
	}

	// Iterate through all digits and update the result.
	for (i = (sign == -1) ? 1 : 0; str[i] != '\0'; ++i) {
		result = (result * 10) + str[i] - '0';
	}

	return sign * result;
}

int printf(const char *format, ...)
{
	char buffer[4096];
	va_list ap;

	// Start variabile argument's list.
	va_start(ap, format);

	int len = vsprintf(buffer, format, ap);
	va_end(ap);

	// Write the contento to standard output.
	write(STDOUT_FILENO, buffer, len);

	return len;
}

size_t scanf(const char *format, ...)
{
	size_t count = 0;
	va_list scan;
	va_start(scan, format);

	for (; *format; format++) {
		if (*format == '%') {
			// Declare an input string.
			char input[255];
			// Get the input string.
			gets(input);
			// Evaluate the length of the string.
			size_t input_length = strlen(input);
			// Add the length of the input to the counter.
			count += input_length;
			// Evaluate the maximum number of input characters.
			size_t max_chars = 0;
			if (isdigit(*++format)) {
				char max_char_num[16];
				int i = 0;

				while (isdigit(*format)) {
					max_char_num[i++] = *format;
					format++;
				}
				max_char_num[i] = '\0';
				int number = atoi(max_char_num);

				if (number > 0) {
					max_chars = (size_t)number;
				}
			}
			switch (*format) {
			case 's': {
				char *s_ptr = va_arg(scan, char *);
				if (max_chars == 0 || input_length <= max_chars)
					strncpy(s_ptr, input, input_length);
				else
					strncpy(s_ptr, input, max_chars);
				break;
			}
			case 'd': {
				int *d_ptr = va_arg(scan, int *);
				if (max_chars != 0 && input_length > max_chars) {
					input[max_chars] = '\0';
				}
				(*d_ptr) = atoi(input);
				break;
			}
			default:
				break;
			}
		}
	}
	va_end(scan);

	return count;
}
