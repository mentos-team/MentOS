/// @file echo.c
/// @brief
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    int newline = 1;
    int eflag = 0;
    char buffer[BUFSIZ];
    char *buf = buffer;
    char *arg;

    // Iterate all the words.
    while ((arg = *++argv) != NULL) {
        if (arg[0] != '-') {
            break;
        }

        // parse the options string
        while (*arg++) {
            if (*arg == 'n') {
                newline = 0;
            } else if (*arg == 'e') {
                eflag = '\\';
            } else {
                break;
            }
        }
    }

    // Iterate all remaining words.
    while ((arg = *argv) != NULL) {
        // We do not expand escape codes.
        if (!eflag) {
            puts(arg);
            goto next_word;
        }

        // Expand escape codes
        int c;
        while ((c = *arg++) != 0) {
            if (c != eflag) {
                *buf++ = c;
            } else {
                switch (*arg) {
                case 'n':
                    *buf++ = '\n';
                    break;
                default:
                    *buf++ =  *(arg-1);
                    *buf++ = *arg;
                }
                arg++;
            }
        }
        // null-terminate the current word
        *buf = 0;
        puts(buffer);

next_word:
        // Add space if there are more words and the last word did not end with a new line
        if ((*(argv+1) != NULL) && *(buf-1) != '\n') {
            putchar(' ');
        }
        // reset buf pointer
        buf = buffer;
        argv++;
    }

    if (newline) {
        printf("\n");
    }
    return 0;
}
