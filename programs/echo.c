/// @file echo.c
/// @brief
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/bitops.h>

#define ENV_NORM 1
#define ENV_BRAK 2
#define ENV_PROT 3

void expand_env(char *str, char *buf, size_t buf_len, int first_word, int last_word)
{
    // Buffer where we store the name of the variable.
    char buffer[BUFSIZ] = { 0 };
    // Flags used to keep track of the special characters.
    unsigned flags = 0;
    // We keep track of where teh
    char *env_start = NULL;
    // Where we store the retrieved environmental variable value.
    char *ENV = NULL;
    // Get the length of the string.
    size_t str_len = strlen(str);
    // Position where we are writing on the buffer.
    int b_pos = 0;
    // Iterate the string.
    for (int s_pos = 0; s_pos < str_len; ++s_pos) {
        if (first_word && (s_pos == 0) && str[s_pos] == '"')
            continue;
        if (last_word && (s_pos == (str_len - 1)) && str[s_pos] == '"')
            continue;
        // If we find the backslash, we need to protect the next character.
        if (str[s_pos] == '\\') {
            if (bit_check(flags, ENV_PROT))
                buf[b_pos++] = '\\';
            else
                bit_set_assign(flags, ENV_PROT);
            continue;
        }
        // If we find the dollar, we need to catch the meaning.
        if (str[s_pos] == '$') {
            // If the previous character is a backslash, we just need to print the dollar.
            if (bit_check(flags, ENV_PROT)) {
                buf[b_pos++] = '$';
            } else if ((s_pos < (str_len - 2)) && ((str[s_pos + 1] == '{'))) {
                // Toggle the open bracket method of accessing env variables.
                bit_set_assign(flags, ENV_BRAK);
                // We need to skip both the dollar and the open bracket `${`.
                env_start = &str[s_pos + 2];
            } else {
                // Toggle the normal method of accessing env variables.
                bit_set_assign(flags, ENV_NORM);
                // We need to skip the dollar `$`.
                env_start = &str[s_pos + 1];
            }
            continue;
        }
        if (bit_check(flags, ENV_BRAK)) {
            if (str[s_pos] == '}') {
                // Copy the environmental variable name.
                strncpy(buffer, env_start, &str[s_pos] - env_start);
                // Search for the environmental variable, and print it.
                if ((ENV = getenv(buffer)))
                    for (int k = 0; k < strlen(ENV); ++k)
                        buf[b_pos++] = ENV[k];
                // Remove the flag.
                bit_clear_assign(flags, ENV_BRAK);
            }
            continue;
        }
        if (bit_check(flags, ENV_NORM)) {
            if (str[s_pos] == ':') {
                // Copy the environmental variable name.
                strncpy(buffer, env_start, &str[s_pos] - env_start);
                // Search for the environmental variable, and print it.
                if ((ENV = getenv(buffer)))
                    for (int k = 0; k < strlen(ENV); ++k)
                        buf[b_pos++] = ENV[k];
                // Copy the `:`.
                buf[b_pos++] = str[s_pos];
                // Remove the flag.
                bit_clear_assign(flags, ENV_NORM);
            }
            continue;
        }
        buf[b_pos++] = str[s_pos];
    }
    if (bit_check(flags, ENV_NORM)) {
        // Copy the environmental variable name.
        strcpy(buffer, env_start);
        // Search for the environmental variable, and print it.
        if ((ENV = getenv(buffer)))
            for (int k = 0; k < strlen(ENV); ++k)
                buf[b_pos++] = ENV[k];
        // Remove the flag.
        bit_clear_assign(flags, ENV_NORM);
    }
}

int main(int argc, char **argv)
{
    char buffer[BUFSIZ];
    // Iterate all the words.
    for (int i = 1; i < argc; ++i) {
        memset(buffer, 0, BUFSIZ);
        expand_env(argv[i], buffer, BUFSIZ, (i == 1), (i == (argc - 1)));
        puts(buffer);
        if (i < (argc - 1))
            putchar(' ');
    }
    printf("\n\n");
    return 0;
}
