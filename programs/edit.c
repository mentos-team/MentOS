/// @file edit.c
/// @brief
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

// Setup the logging for this file (do this before any other include).
#include "sys/kernel_levels.h"           // Include kernel log levels.
#define __DEBUG_HEADER__ "[EDIT  ]"      ///< Change header.
#define __DEBUG_LEVEL__  LOGLEVEL_NOTICE ///< Set log level.
#include "io/debug.h"                    // Include debugging functions.

#include <unistd.h>
#include <sys/stat.h>
#include <strerror.h>
#include <termios.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <fcntl.h>

#define MAX_LINE_LENGTH 160
#define MAX_LINES       512

#define BUFFER_SIZE (MAX_LINE_LENGTH * MAX_LINES)

#define TAB_SIZE  4
#define PAGE_SIZE 4

static struct termios orig_termios;

/// @brief Loads the contents of a file into a buffer and calculates the file
/// length and number of lines.
///
/// @param filename The name of the file to load.
/// @param buffer The buffer where the file contents will be stored.
/// @param bufsize The size of the buffer.
/// @param file_length Pointer to an integer that will hold the length of the file in bytes.
/// @return int The number of lines in the file.
int load_file(const char *filename, char *buffer, size_t bufsize, int *file_length)
{
    // Open the file with read-only permissions.
    int fd = open(filename, O_RDONLY, 666);
    if (fd == -1) {
        perror("Error opening file");
        exit(1);
    }
    // Read the file into the buffer.
    ssize_t bytes_read = read(fd, buffer, bufsize);
    if (bytes_read == -1) {
        perror("Error reading file");
        close(fd);
        exit(1);
    }
    // Ensure null termination of the buffer.
    buffer[bytes_read] = '\0';
    // Initialize the number of lines and set the file length.
    int num_lines = 0;
    // Set file_length to the number of bytes read.
    *file_length = bytes_read;
    // Count the number of lines by counting '\n'.
    for (char *p = buffer; *p != '\0'; p++) {
        if (*p == '\n') {
            num_lines++;
        }
    }
    // Close the file.
    if (close(fd) == -1) {
        perror("Error closing file");
        exit(1);
    }
    // Return the number of lines in the file.
    return num_lines;
}

/// @brief Saves the given buffer to a file, writing only up to the given buffer
/// size.
///
/// @param filename The name of the file to save the buffer to.
/// @param buffer The buffer containing the data to be written to the file.
/// @param bufsize The size of the buffer to be written to the file.
/// @return int Returns 1 on success, 0 on failure.
int save_file(const char *filename, char *buffer, size_t bufsize)
{
    // Open the file with write-only permissions, create it if it doesn't exist,
    // truncate it if it does.
    int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) {
        perror("Error opening file for writing");
        return 0;
    }
    // Write the buffer to the file, limited to the given bufsize.
    ssize_t bytes_written = write(fd, buffer, bufsize);
    if (bytes_written == -1) {
        perror("Error writing to file");
        close(fd);
        return 0;
    }
    // Ensure all data was written.
    if (bytes_written != bufsize) {
        fprintf(stderr, "Partial write: expected %zu bytes, wrote %zd bytes\n", bufsize, bytes_written);
        close(fd);
        return 0;
    }
    // Close the file after writing.
    if (close(fd) == -1) {
        perror("Error closing file");
        return 0;
    }
    // Return success.
    return 1;
}

/// @brief Trims empty lines and trailing spaces at the end of the file buffer,
/// ensuring the file ends with a newline.
///
/// @param lines The buffer containing the file contents.
/// @param file_length Pointer to the length of the file, which will be adjusted after trimming.
/// @return void
void trim_empty_lines_at_end(char *lines, int *file_length)
{
    // Ensure file_length is valid before attempting to trim.
    if (*file_length <= 0) {
        return;
    }
    // Start from the end of the file (file_length - 1) and move backward.
    while (*file_length > 0 && (lines[*file_length - 1] == '\n' || lines[*file_length - 1] == ' ')) {
        // Reduce the file length for each newline or space found at the end.
        (*file_length)--;
    }
    // If there is still content in the file, add a newline at the end.
    if (*file_length > 0) {
        // Ensure the file ends with a newline.
        lines[*file_length] = '\n';
        // Account for the newly added newline.
        *file_length += 1;
    }
    // Null-terminate the string to mark the end of the file
    lines[*file_length] = '\0';
}

/// @brief Shifts the lines in the buffer up, moving the next line into the
/// current one and adjusting file length.
///
/// @param lines The buffer containing all lines of the file.
/// @param cy The current line index.
/// @param cx The cursor position in the current line.
/// @param num_lines The total number of lines in the buffer.
/// @param file_length Pointer to the file length to adjust after shifting lines.
/// @return void
void shift_lines_up(char *lines, int cy, int cx, int num_lines, int *file_length)
{
    // Error checks to ensure valid input. Ensure that cy is within a valid range.
    if (cy < 0 || cy >= num_lines - 1) {
        return;
    }
    // Get pointers to the current line and the next line
    char *line_start = lines;
    // Traverse lines until the current line is found.
    for (int i = 0; i < cy; i++) {
        line_start = strchr(line_start, '\n') + 1;
        // Error handling in case '\n' is not found.
        if (!line_start) {
            return;
        }
    }
    char *next_line_start = strchr(line_start, '\n') + 1;
    // Error handling in case '\n' is not found for the next line.
    if (!next_line_start) {
        return;
    }
    // Get the position of cx within the current line.
    int line_len = strchr(line_start, '\n') - line_start;
    // Calculate the current position in the file.
    int current_position = line_start - lines;
    // Append the next line's content after cx in the current line.
    memmove(&line_start[cx], next_line_start, *file_length - (next_line_start - lines));
    // Shift everything up by removing the newline between cy and cy + 1.
    memmove(next_line_start - 1, next_line_start, *file_length - (next_line_start - lines));
    // Adjust the file length after shifting up.
    *file_length -= (next_line_start - line_start);
}

/// @brief Shifts lines down in the buffer, creating space in the current line
/// and moving part of the line down.
///
/// @param lines The buffer containing all lines of the file.
/// @param cy The current line index.
/// @param cx The cursor position in the current line.
/// @param num_lines The total number of lines in the buffer.
/// @param file_length Pointer to the file length to adjust after shifting lines.
/// @return void
void shift_lines_down(char *lines, int cy, int cx, int num_lines, int *file_length)
{
    // Error checks to ensure valid input. Ensure that cy is within a valid range.
    if (cy < 0 || cy >= num_lines - 1) {
        return;
    }
    // Get pointers to the current line and the next line
    char *line_start = lines;
    // Traverse lines until the current line is found.
    for (int i = 0; i < cy; i++) {
        line_start = strchr(line_start, '\n') + 1;
        // Error handling in case '\n' is not found.
        if (!line_start) {
            return;
        }
    }
    char *next_line_start = strchr(line_start, '\n') + 1;
    // Error handling in case '\n' is not found for the next line.
    if (!next_line_start) {
        return;
    }
    // Get the length of the current line and calculate the remaining part after cx
    int line_len = strchr(line_start, '\n') - line_start;
    // Calculate the current position in the file
    int current_position = line_start - lines;
    // Shift the next lines down to create space for the new part
    memmove(next_line_start + (line_len - cx), next_line_start, *file_length - (next_line_start - lines));
    // Copy the part of the current line after cx to the next line
    memcpy(next_line_start, &line_start[cx], line_len - cx);
    // Insert a newline character at position cx in the current line
    line_start[cx] = '\n';
    // Adjust the file length after shifting down
    *file_length += (line_len - cx);
}

/// @brief Computes the start and end of the current line in the buffer.
///
/// @param lines The buffer containing the file contents.
/// @param cy The current line index (0-based).
/// @param line_start Pointer to a pointer that will be set to the start of the line.
/// @param line_end Pointer to a pointer that will be set to the end of the line.
/// @param file_length The total length of the file in bytes.
/// @return int The length of the current line, or 0 if no valid line is found.
int get_line_start_end(char *lines, int cy, char **line_start, char **line_end, const int file_length)
{
    *line_start = lines;
    // Find the start of the current line by iterating through newlines.
    for (int i = 0; i < cy; i++) {
        *line_start = strchr(*line_start, '\n') + 1;
        // If '\n' was not found, strchr would return NULL.
        if (*line_start == (char *)1) {
            *line_end = NULL;
            return 0;
        }
    }
    // Ensure we don't go beyond the file_length.
    if (*line_start - lines >= file_length) {
        // No valid line at this position.
        *line_end = NULL;
        return 0;
    }
    // Find the end of the current line (until the next newline character or end
    // of file).
    *line_end = strchr(*line_start, '\n');
    if (*line_end == NULL) {
        // If no newline is found, the line ends at the end of the file.
        *line_end = lines + file_length;
    }
    // Return the length of the current line
    return *line_end - *line_start;
}

/// @brief Returns the length of the current line based on the given line index
/// (cy).
///
/// @param lines The buffer containing the file contents.
/// @param cy The current line index (0-based).
/// @param file_length The total length of the file in bytes.
/// @return int The length of the current line, or 0 if no valid line is found.
int get_line_length(char *lines, int cy, const int file_length)
{
    char *line_start, *line_end;
    // Compute the start and end of the current line.
    int line_length = get_line_start_end(lines, cy, &line_start, &line_end, file_length);
    // If line_end is NULL, there is no valid line, so return 0.
    if (line_end == NULL) {
        return 0;
    }
    // Return the length of the current line.
    return line_length;
}

/// @brief Updates the status message with the current editor state.
///
/// @param buffer The buffer where the status message will be stored.
/// @param bufsize The size of the buffer.
/// @param cy The current line index (0-based).
/// @param cx The current cursor position within the line.
/// @param lines The buffer containing the file contents.
/// @param num_lines The total number of lines in the file.
/// @param file_length Pointer to the current length of the file in bytes.
/// @param insert_active The flag indicating if insert mode is active.
void update_status_message(char *buffer, size_t bufsize, int cy, int cx, char *lines, int num_lines, int *file_length, int insert_active)
{
    // Prepare the status message
    snprintf(buffer, bufsize, "(y:%3d, x:%3d, line_len:%3d, lines:%3d, file_length:%3d %s)\n",
             cy, cx, get_line_length(lines, cy, *file_length), num_lines, *file_length,
             insert_active ? "INS" : "   ");
}

/// @brief Edits the file buffer interactively, allowing navigation and editing.
///
/// @param lines The buffer containing the file contents.
/// @param bufsize The size of the buffer.
/// @param num_lines The number of lines in the file.
/// @param filename The name of the file to save changes to.
/// @param file_length Pointer to the current length of the file in bytes.
/// @return void
void edit_file(char *lines, size_t bufsize, int num_lines, const char *filename, int *file_length)
{
    int cx = 0, cy       = 0; // Cursor position (cx: column, cy: row)
    int c, insert_active = 0; // Insert mode flag

    char *line_start = lines, *line_end;
    int line_len     = get_line_start_end(lines, cy, &line_start, &line_end, *file_length);

    char message[MAX_LINE_LENGTH] = { 0 };

    update_status_message(message, MAX_LINE_LENGTH, cy, cx, lines, num_lines, file_length, insert_active);

    while (1) {
        // Clear the screen and move the cursor to the top-left corner.
        printf("\033[H\033[J");

        // Print the initial file content.
        puts(lines);
        putchar('\n');
        puts("================================================================================");
        puts("[ \033[1;32m^W Save \033[1;31m^C Quit\033[0m ]\n");
        puts(message);
        message[0] = '\0';

        // Move cursor to the start of the file.
        printf("\033[%d;%dH", cy + 1, cx + 1);

        c = getchar();

        line_len = get_line_start_end(lines, cy, &line_start, &line_end, *file_length);

        // Handle arrow keys and escape sequences.
        if (c == '\033') {
            pr_debug("[ ](%d)\n", c);
            c = getchar();
            pr_debug("[%c](%d)\n", c, c);
            if (c == '[') {
                c = getchar();
                pr_debug("[%c](%d)\n", c, c);
                // UP Arrow.
                if (c == 'A') {
                    if (cy > 0) {
                        int curr_line_len = line_len;
                        int next_line_len = get_line_length(lines, cy - 1, *file_length);
                        cy--;
                        if (((cx == curr_line_len) && curr_line_len) || (cx > next_line_len)) {
                            cx = next_line_len;
                        }
                    }
                }
                // DOWN Arrow.
                else if (c == 'B') {
                    if ((line_start - lines + cx) < *file_length - 1) {
                        int curr_line_len = line_len;
                        int next_line_len = get_line_length(lines, cy + 1, *file_length);
                        cy++;
                        if (((cx == curr_line_len) && curr_line_len) || (cx > next_line_len)) {
                            cx = next_line_len;
                        }
                    }
                }
                // RIGHT Arrow.
                else if (c == 'C') {
                    // Move right within the current line.
                    if (cx < line_len) {
                        cx++;
                    }
                    // If at the end of the current line, move to the next line
                    // if it exists.
                    else if (cy < num_lines - 1) {
                        cy++;
                        // Move cursor to the beginning of the next line.
                        cx = 0;
                    }
                }
                // LEFT Arrow.
                else if (c == 'D') {
                    // Move left within the current line
                    if (cx > 0) {
                        cx--;
                    }
                    // If at the beginning of the current line, move to the end
                    // of the previous line if it exists.
                    else if (cy > 0) {
                        cy--;
                        // Move cursor to the end of the previous line.
                        cx = get_line_length(lines, cy, *file_length);
                    }
                }
                // INSERT
                else if (c == '2') {
                    if (getchar() == '~') {
                        insert_active = !insert_active;
                        printf(insert_active ? "\033[3 q" : "\033[0 q"); // Change cursor appearance.
                    }
                }
                // HOME
                else if (c == 'H') {
                    cx = 0;
                }
                // END
                else if (c == 'F') {
                    cx = line_len;
                }
                // PAGE UP
                else if (c == '5') {
                    if (getchar() == '~') {
                        cy       = (cy < PAGE_SIZE) ? 0 : cy - PAGE_SIZE;
                        line_len = get_line_start_end(lines, cy, &line_start, &line_end, *file_length);
                        if (cx > line_len) {
                            cx = line_len;
                        }
                    }
                }
                // PAGE DOWN
                else if (c == '6') {
                    if (getchar() == '~') {
                        cy       = (cy + PAGE_SIZE >= num_lines) ? num_lines - 1 : cy + PAGE_SIZE;
                        line_len = get_line_start_end(lines, cy, &line_start, &line_end, *file_length);
                        if (cx > line_len) {
                            cx = line_len;
                        }
                    }
                }
                // Handle Ctrl + Arrows.
                else if (c == '1') {
                    c = getchar();
                    pr_debug("[%c](%d)\n", c, c);

                    // CTRL + ARROW
                    if (c == ';') {
                        c = getchar();
                        pr_debug("[%c](%d)\n", c, c);
                        if (c == '5') {
                            c = getchar();
                            pr_debug("[%c](%d)\n", c, c);
                            // Handle Ctrl+Right Arrow (Move to the beginning of the next word).
                            if (c == 'C') {
                                // Skip spaces first
                                while (cx < line_len && line_start[cx] == ' ') {
                                    cx++;
                                }
                                // Move to the end of the current word (non-space characters)
                                while (cx < line_len && line_start[cx] != ' ') {
                                    cx++;
                                }
                            }
                            // Handle Ctrl+Left Arrow (Move to the beginning of the previous word)
                            else if (c == 'D') {
                                // Move left past spaces first
                                while (cx > 0 && line_start[cx - 1] == ' ') {
                                    cx--;
                                }
                                // Move left to the beginning of the current word (non-space characters)
                                while (cx > 0 && line_start[cx - 1] != ' ') {
                                    cx--;
                                }
                            }
                        }
                    }
                }
            }
        }
        // Ctrl+W (Save file)
        else if (c == 0x17) {
            if (save_file(filename, lines, *file_length)) {
                sprintf(message, "\033[1;33mFile saved!\033[0m\n");
                continue;
            }
        }
        // Ctrl+C (Exit)
        else if (c == 0x03) {
            printf("\033[%d;%dH", num_lines + 4, 0);
            printf("**Exiting without saving**\n");
            printf("\033[0 q");
            break;
        }
        // DELETE
        else if (c == 127) {
            // Check if we're at the end of the file (i.e., cx is at the last character)
            if ((line_start - lines + cx) < *file_length - 1) {
                memmove(&line_start[cx], &line_start[cx + 1], *file_length - (line_start - lines + cx + 1));
                if (line_start[cx] == '\n') {
                    num_lines--;
                }
                // Decrease file length after deleting.
                (*file_length)--;
            }
        }
        // Backspace
        else if (c == '\b') {
            if (cx > 0) {
                // If not at the beginning of the line, remove the character at the cursor
                memmove(&line_start[cx - 1], &line_start[cx], *file_length - (line_start - lines + cx));
                cx--;
            } else if (cy > 0) {
                line_len = get_line_length(lines, cy - 1, *file_length);
                // If not at the beginning of the line, remove the character at the cursor
                memmove(&line_start[cx - 1], &line_start[cx], *file_length - (line_start - lines + cx));
                // If at the beginning of the line (cx == 0), move to the previous line
                cy--;
                cx = line_len;
                num_lines--;
            }
            (*file_length)--;
        }
        // Handle Tab key ('\t')
        else if (c == '\t') {
            int spaces_to_add = TAB_SIZE - (cx % TAB_SIZE);
            // Shift file content forward by the number of spaces needed for the tab.
            memmove(&line_start[cx + spaces_to_add], &line_start[cx], *file_length - (line_start - lines + cx));
            // Insert spaces to simulate the tab
            memset(&line_start[cx], ' ', spaces_to_add);
            // Move cursor forward by the tab size and update file length
            cx += spaces_to_add;
            // Increment file length to account for the new character.
            (*file_length) += spaces_to_add;
        }
        // Handle Enter key ('\n')
        else if (c == '\n') {
            // Shift file content forward by 1 byte to make space for '\n'
            memmove(&line_start[cx + 1], &line_start[cx], *file_length - (line_start - lines + cx));
            // Insert the '\n' character at the current cursor position
            line_start[cx] = '\n';
            // Move to the next line, reset column, and increment number of lines
            cy++;
            cx = 0;
            num_lines++;
            // Increment file length to account for the new character.
            (*file_length)++;
        }
        // Printable characters
        else if (c >= 32 && c <= 126) {
            // If not in insert mode, shift content to the right by 1 byte to make space for new character.
            if (!insert_active) {
                memmove(&line_start[cx + 1], &line_start[cx], *file_length - (line_start - lines + cx));
            }
            // Insert the character at the current cursor position.
            line_start[cx] = c;
            // Move cursor forward by 1.
            cx++;
            // Increment file length to account for the new character.
            (*file_length)++;
        }

        update_status_message(message, MAX_LINE_LENGTH, cy, cx, lines, num_lines, file_length, insert_active);
    }
}

void enable_raw_mode(void)
{
    tcgetattr(STDIN_FILENO, &orig_termios);
    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON | ISIG); // Disable echo, canonical mode, and signals
    tcsetattr(STDIN_FILENO, 0, &raw);
}

void disable_raw_mode(void)
{
    tcsetattr(STDIN_FILENO, 0, &orig_termios);
}

// Main function remains the same as before...
int main(int argc, char *argv[])
{
    if (argc != 2) {
        printf("Usage: %s <filename>\n", argv[0]);
        return 1;
    }

    char filename[256];
    strncpy(filename, argv[1], 256);

    char buffer[BUFFER_SIZE];

    int file_length = 0;

    // Load the file
    int num_lines = load_file(filename, buffer, BUFFER_SIZE, &file_length);

    // Enable raw mode for real-time editing
    enable_raw_mode();

    // Edit the file
    edit_file(buffer, BUFFER_SIZE, num_lines, filename, &file_length);

    // Restore terminal mode
    disable_raw_mode();

    return 0;
}
