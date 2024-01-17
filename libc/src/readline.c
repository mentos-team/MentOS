/// @file readline.c
/// @author Enrico Fraccaroli (enry.frak@gmail.com)
/// @brief
/// @version 0.1
/// @date 2023-08-30
///
/// @copyright Copyright (c) 2023
///

#include "readline.h"

#include "stdio.h"
#include "string.h"
#include "sys/unistd.h"

int readline(int fd, char *buffer, size_t buflen, ssize_t *read_len)
{
    ssize_t length, rollback, num_read;
    memset(buffer, 0, buflen);
    unsigned char found_newline = 1;
    // Read from the file.
    num_read = read(fd, buffer, buflen);
    if (num_read == 0) {
        return 0;
    }
    // Search for termination character.
    char *newline = strchr(buffer, '\n');
    if (newline == NULL) {
        found_newline = 0;
        newline       = strchr(buffer, EOF);
        if (newline == NULL) {
            newline = strchr(buffer, 0);
            if (newline == NULL) {
                return 0;
            }
        }
    }
    // Compute the length of the string.
    length = (newline - buffer);
    if (length <= 0) {
        return 0;
    }
    // Close the string.
    buffer[length] = 0;
    // Compute how much we need to rollback.
    rollback = length - num_read + 1;
    if (rollback > 1) {
        return 0;
    }
    // Rollback the reading position in the file.
    lseek(fd, rollback, SEEK_CUR);
    // Set how much we were able to read from the file.
    if (read_len) {
        *read_len = length;
    }
    return (found_newline) ? 1 : -1;
}
