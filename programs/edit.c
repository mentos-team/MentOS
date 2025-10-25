/// @file edit.c
/// @brief A simple text editor with line-based buffer management.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <ctype.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>

#define MAX_LINE_LENGTH 256
#define MAX_LINES       1024
#define TAB_SIZE        4
#define SCREEN_HEIGHT   20
#define LINE_NUM_WIDTH  6  ///< Width of line number column (including space).
#define DISPLAY_WIDTH   74 ///< Maximum characters displayed per line (80 - LINE_NUM_WIDTH).

/// @brief Structure representing a single line in the editor.
typedef struct {
    char *data;      ///< Line content (dynamically allocated).
    size_t length;   ///< Current length of the line.
    size_t capacity; ///< Allocated capacity for the line.
} line_t;

/// @brief Structure representing the editor state.
typedef struct {
    line_t *lines;        ///< Array of lines.
    size_t line_count;    ///< Number of lines in the buffer.
    size_t line_capacity; ///< Allocated capacity for lines array.
    size_t cursor_x;      ///< Cursor column position.
    size_t cursor_y;      ///< Cursor line position.
    size_t row_offset;    ///< Vertical scroll offset.
    int insert_mode;      ///< Insert mode (1) vs overwrite mode (0).
    int modified;         ///< Has the file been modified?
    char *filename;       ///< Name of the file being edited.
    char status_msg[256]; ///< Status message to display.
} editor_state_t;

static struct termios orig_termios;

// ============================================================================
// Terminal Control Functions
// ============================================================================

/// @brief Enables raw mode for terminal input.
void enable_raw_mode(void)
{
    tcgetattr(STDIN_FILENO, &orig_termios);
    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON | ISIG);
    tcsetattr(STDIN_FILENO, 0, &raw);
}

/// @brief Restores terminal to original mode.
void disable_raw_mode(void)
{
    tcsetattr(STDIN_FILENO, 0, &orig_termios);
}

// ============================================================================
// Line Management Functions
// ============================================================================

/// @brief Initializes a line structure.
/// @param line Pointer to the line to initialize.
static void line_init(line_t *line)
{
    line->data     = NULL;
    line->length   = 0;
    line->capacity = 0;
}

/// @brief Frees memory allocated for a line.
/// @param line Pointer to the line to free.
static void line_free(line_t *line)
{
    if (line->data) {
        free(line->data);
        line->data = NULL;
    }
    line->length   = 0;
    line->capacity = 0;
}

/// @brief Ensures a line has enough capacity to store the specified size.
/// @param line Pointer to the line.
/// @param size Minimum required capacity.
static void line_ensure_capacity(line_t *line, size_t size)
{
    if (size <= line->capacity) {
        return;
    }
    size_t new_capacity = line->capacity == 0 ? 32 : line->capacity;
    while (new_capacity < size) {
        new_capacity *= 2;
    }
    char *new_data = realloc(line->data, new_capacity);
    if (!new_data) {
        return;
    }
    line->data     = new_data;
    line->capacity = new_capacity;
}

/// @brief Sets the content of a line.
/// @param line Pointer to the line.
/// @param data Data to set.
/// @param length Length of the data.
static void line_set(line_t *line, const char *data, size_t length)
{
    line_ensure_capacity(line, length + 1);
    memcpy(line->data, data, length);
    line->data[length] = '\0';
    line->length       = length;
}

/// @brief Inserts a character at a specific position in a line.
/// @param line Pointer to the line.
/// @param pos Position to insert at.
/// @param c Character to insert.
static void line_insert_char(line_t *line, size_t pos, char c)
{
    if (pos > line->length) {
        pos = line->length;
    }
    line_ensure_capacity(line, line->length + 2);
    memmove(&line->data[pos + 1], &line->data[pos], line->length - pos);
    line->data[pos] = c;
    line->length++;
    line->data[line->length] = '\0';
}

/// @brief Deletes a character at a specific position in a line.
/// @param line Pointer to the line.
/// @param pos Position to delete from.
static void line_delete_char(line_t *line, size_t pos)
{
    if (pos >= line->length) {
        return;
    }
    memmove(&line->data[pos], &line->data[pos + 1], line->length - pos);
    line->length--;
}

/// @brief Overwrites a character at a specific position in a line.
/// @param line Pointer to the line.
/// @param pos Position to overwrite.
/// @param c Character to write.
static void line_overwrite_char(line_t *line, size_t pos, char c)
{
    if (pos >= line->length) {
        line_insert_char(line, pos, c);
    } else {
        line->data[pos] = c;
    }
}

/// @brief Appends data to a line.
/// @param line Pointer to the line.
/// @param data Data to append.
/// @param length Length of the data.
static void line_append(line_t *line, const char *data, size_t length)
{
    if (length == 0) {
        return;
    }
    line_ensure_capacity(line, line->length + length + 1);
    memcpy(&line->data[line->length], data, length);
    line->length += length;
    line->data[line->length] = '\0';
}

// ============================================================================
// Editor State Management Functions
// ============================================================================

/// @brief Initializes the editor state.
/// @param editor Pointer to the editor state.
/// @param filename Name of the file to edit.
static void editor_init(editor_state_t *editor, const char *filename)
{
    editor->lines         = malloc(sizeof(line_t) * 16);
    editor->line_count    = 0;
    editor->line_capacity = 16;
    editor->cursor_x      = 0;
    editor->cursor_y      = 0;
    editor->row_offset    = 0;
    editor->insert_mode   = 1;
    editor->modified      = 0;
    editor->filename      = strdup(filename);
    editor->status_msg[0] = '\0';

    // Initialize with one empty line
    line_init(&editor->lines[0]);
    editor->line_count = 1;
}

/// @brief Frees all memory allocated for the editor.
/// @param editor Pointer to the editor state.
static void editor_free(editor_state_t *editor)
{
    for (size_t i = 0; i < editor->line_count; i++) {
        line_free(&editor->lines[i]);
    }
    free(editor->lines);
    free(editor->filename);
}

/// @brief Ensures the editor has enough capacity for the specified number of lines.
/// @param editor Pointer to the editor state.
/// @param count Required number of lines.
static void editor_ensure_line_capacity(editor_state_t *editor, size_t count)
{
    if (count <= editor->line_capacity) {
        return;
    }
    size_t new_capacity = editor->line_capacity;
    while (new_capacity < count) {
        new_capacity *= 2;
    }
    line_t *new_lines = realloc(editor->lines, sizeof(line_t) * new_capacity);
    if (!new_lines) {
        return;
    }
    editor->lines         = new_lines;
    editor->line_capacity = new_capacity;
}

/// @brief Inserts a new line at the specified position.
/// @param editor Pointer to the editor state.
/// @param at Line index to insert at.
static void editor_insert_line(editor_state_t *editor, size_t at)
{
    if (at > editor->line_count) {
        at = editor->line_count;
    }
    editor_ensure_line_capacity(editor, editor->line_count + 1);
    memmove(&editor->lines[at + 1], &editor->lines[at], sizeof(line_t) * (editor->line_count - at));
    line_init(&editor->lines[at]);
    editor->line_count++;
    editor->modified = 1;
}

/// @brief Deletes a line at the specified position.
/// @param editor Pointer to the editor state.
/// @param at Line index to delete.
static void editor_delete_line(editor_state_t *editor, size_t at)
{
    if (at >= editor->line_count) {
        return;
    }
    line_free(&editor->lines[at]);
    memmove(&editor->lines[at], &editor->lines[at + 1], sizeof(line_t) * (editor->line_count - at - 1));
    editor->line_count--;
    editor->modified = 1;
}

// ============================================================================
// File I/O Functions
// ============================================================================

/// @brief Loads a file into the editor.
/// @param editor Pointer to the editor state.
/// @return 0 on success, -1 on failure.
static int editor_load_file(editor_state_t *editor)
{
    int fd = open(editor->filename, O_RDONLY, 0);
    if (fd == -1) {
        // File doesn't exist, start with empty buffer
        return 0;
    }

    // Clear existing content
    for (size_t i = 0; i < editor->line_count; i++) {
        line_free(&editor->lines[i]);
    }
    editor->line_count = 0;

    char buffer[4096];
    ssize_t bytes_read;
    size_t line_start = 0;

    while ((bytes_read = read(fd, buffer, sizeof(buffer))) > 0) {
        for (ssize_t i = 0; i < bytes_read; i++) {
            if (buffer[i] == '\n') {
                // Found newline, save the line
                editor_ensure_line_capacity(editor, editor->line_count + 1);
                line_init(&editor->lines[editor->line_count]);
                line_set(&editor->lines[editor->line_count], &buffer[line_start], i - line_start);
                editor->line_count++;
                line_start = i + 1;
            }
        }
        // Handle partial line
        if (line_start < (size_t)bytes_read) {
            editor_ensure_line_capacity(editor, editor->line_count + 1);
            if (editor->line_count == 0) {
                line_init(&editor->lines[0]);
                editor->line_count = 1;
            }
            line_append(&editor->lines[editor->line_count - 1], &buffer[line_start], bytes_read - line_start);
            line_start = 0;
        } else {
            line_start = 0;
        }
    }

    close(fd);

    // Ensure at least one line exists
    if (editor->line_count == 0) {
        editor_ensure_line_capacity(editor, 1);
        line_init(&editor->lines[0]);
        editor->line_count = 1;
    }

    editor->modified = 0;
    return 0;
}

/// @brief Saves the editor content to a file.
/// @param editor Pointer to the editor state.
/// @return 0 on success, -1 on failure.
static int editor_save_file(editor_state_t *editor)
{
    int fd = open(editor->filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) {
        snprintf(editor->status_msg, sizeof(editor->status_msg), "Error: Cannot save file!");
        return -1;
    }

    for (size_t i = 0; i < editor->line_count; i++) {
        if (editor->lines[i].length > 0) {
            write(fd, editor->lines[i].data, editor->lines[i].length);
        }
        if (i < editor->line_count - 1) {
            write(fd, "\n", 1);
        }
    }

    close(fd);
    editor->modified = 0;
    snprintf(editor->status_msg, sizeof(editor->status_msg), "File saved (%zu lines)", editor->line_count);
    return 0;
}

// ============================================================================
// Editor Operations
// ============================================================================

/// @brief Inserts a character at the current cursor position.
/// @param editor Pointer to the editor state.
/// @param c Character to insert.
static void editor_insert_char(editor_state_t *editor, char c)
{
    if (editor->cursor_y >= editor->line_count) {
        editor_insert_line(editor, editor->line_count);
    }

    line_t *line = &editor->lines[editor->cursor_y];

    if (editor->insert_mode) {
        line_insert_char(line, editor->cursor_x, c);
    } else {
        line_overwrite_char(line, editor->cursor_x, c);
    }

    editor->cursor_x++;
    editor->modified = 1;
}

/// @brief Inserts a newline at the current cursor position.
/// @param editor Pointer to the editor state.
static void editor_insert_newline(editor_state_t *editor)
{
    if (editor->cursor_y >= editor->line_count) {
        editor_insert_line(editor, editor->line_count);
    }

    line_t *line = &editor->lines[editor->cursor_y];

    if (editor->cursor_x == 0) {
        // At beginning of line, insert empty line before
        editor_insert_line(editor, editor->cursor_y);
    } else if (editor->cursor_x >= line->length) {
        // At end of line, insert empty line after
        editor_insert_line(editor, editor->cursor_y + 1);
    } else {
        // In middle of line, split the line
        editor_insert_line(editor, editor->cursor_y + 1);
        line_t *new_line = &editor->lines[editor->cursor_y + 1];
        line_set(new_line, &line->data[editor->cursor_x], line->length - editor->cursor_x);
        line->length             = editor->cursor_x;
        line->data[line->length] = '\0';
    }

    editor->cursor_y++;
    editor->cursor_x = 0;
    editor->modified = 1;
}

/// @brief Deletes the character before the cursor (backspace).
/// @param editor Pointer to the editor state.
static void editor_delete_char(editor_state_t *editor)
{
    if (editor->cursor_y >= editor->line_count) {
        return;
    }

    if (editor->cursor_x == 0) {
        if (editor->cursor_y == 0) {
            return;
        }
        // Merge with previous line
        line_t *prev_line = &editor->lines[editor->cursor_y - 1];
        line_t *curr_line = &editor->lines[editor->cursor_y];
        editor->cursor_x  = prev_line->length;
        line_append(prev_line, curr_line->data, curr_line->length);
        editor_delete_line(editor, editor->cursor_y);
        editor->cursor_y--;
    } else {
        line_delete_char(&editor->lines[editor->cursor_y], editor->cursor_x - 1);
        editor->cursor_x--;
        editor->modified = 1;
    }
}

/// @brief Deletes the character at the cursor (delete key).
/// @param editor Pointer to the editor state.
static void editor_delete_char_forward(editor_state_t *editor)
{
    if (editor->cursor_y >= editor->line_count) {
        return;
    }

    line_t *line = &editor->lines[editor->cursor_y];

    if (editor->cursor_x >= line->length) {
        // At end of line, merge with next line
        if (editor->cursor_y < editor->line_count - 1) {
            line_t *next_line = &editor->lines[editor->cursor_y + 1];
            line_append(line, next_line->data, next_line->length);
            editor_delete_line(editor, editor->cursor_y + 1);
            editor->modified = 1;
        }
    } else {
        line_delete_char(line, editor->cursor_x);
        editor->modified = 1;
    }
}

/// @brief Moves the cursor up one line.
/// @param editor Pointer to the editor state.
static void editor_move_cursor_up(editor_state_t *editor)
{
    if (editor->cursor_y > 0) {
        editor->cursor_y--;
        line_t *line = &editor->lines[editor->cursor_y];
        if (editor->cursor_x > line->length) {
            editor->cursor_x = line->length;
        }
    }
}

/// @brief Moves the cursor down one line.
/// @param editor Pointer to the editor state.
static void editor_move_cursor_down(editor_state_t *editor)
{
    if (editor->cursor_y < editor->line_count - 1) {
        editor->cursor_y++;
        line_t *line = &editor->lines[editor->cursor_y];
        if (editor->cursor_x > line->length) {
            editor->cursor_x = line->length;
        }
    }
}

/// @brief Moves the cursor left one character.
/// @param editor Pointer to the editor state.
static void editor_move_cursor_left(editor_state_t *editor)
{
    if (editor->cursor_x > 0) {
        editor->cursor_x--;
    } else if (editor->cursor_y > 0) {
        editor->cursor_y--;
        editor->cursor_x = editor->lines[editor->cursor_y].length;
    }
}

/// @brief Moves the cursor right one character.
/// @param editor Pointer to the editor state.
static void editor_move_cursor_right(editor_state_t *editor)
{
    if (editor->cursor_y < editor->line_count) {
        line_t *line = &editor->lines[editor->cursor_y];
        if (editor->cursor_x < line->length) {
            editor->cursor_x++;
        } else if (editor->cursor_y < editor->line_count - 1) {
            editor->cursor_y++;
            editor->cursor_x = 0;
        }
    }
}

/// @brief Adjusts the row offset for scrolling, accounting for line wrapping.
/// @param editor Pointer to the editor state.
static void editor_scroll(editor_state_t *editor)
{
    // Calculate how many screen rows are needed from row_offset to cursor_y
    size_t screen_rows_used = 0;
    
    // Count screen rows for lines before cursor
    for (size_t i = editor->row_offset; i < editor->cursor_y && i < editor->line_count; i++) {
        line_t *line = &editor->lines[i];
        size_t line_rows = (line->length == 0) ? 1 : ((line->length + DISPLAY_WIDTH - 1) / DISPLAY_WIDTH);
        if (line_rows == 0) line_rows = 1;
        screen_rows_used += line_rows;
    }
    
    // Add screen rows for current line up to cursor
    if (editor->cursor_y < editor->line_count) {
        size_t cursor_row_within_line = editor->cursor_x / DISPLAY_WIDTH;
        screen_rows_used += cursor_row_within_line + 1;
    }
    
    // Scroll up if cursor is above visible area
    while (editor->cursor_y < editor->row_offset && editor->row_offset > 0) {
        editor->row_offset--;
    }
    
    // Scroll down if cursor is below visible area
    while (screen_rows_used > SCREEN_HEIGHT && editor->row_offset < editor->line_count) {
        // Remove the first line from the display
        line_t *first_line = &editor->lines[editor->row_offset];
        size_t first_line_rows = (first_line->length == 0) ? 1 : ((first_line->length + DISPLAY_WIDTH - 1) / DISPLAY_WIDTH);
        if (first_line_rows == 0) first_line_rows = 1;
        screen_rows_used -= first_line_rows;
        editor->row_offset++;
    }
}

// ============================================================================
// Display Functions
// ============================================================================

/// @brief Draws the editor screen.
/// @param editor Pointer to the editor state.
static void editor_draw_screen(editor_state_t *editor)
{
    puts("\033[H\033[J"); // Clear screen and move cursor to home

    // Draw lines with wrapping support
    size_t screen_row = 0;
    size_t file_row = editor->row_offset;
    
    while (screen_row < SCREEN_HEIGHT && file_row < editor->line_count) {
        line_t *line = &editor->lines[file_row];
        
        // Calculate how many screen rows this line needs
        size_t line_rows = (line->length == 0) ? 1 : ((line->length + DISPLAY_WIDTH - 1) / DISPLAY_WIDTH);
        if (line_rows == 0) line_rows = 1;
        
        // Draw each wrapped segment of the line
        for (size_t segment = 0; segment < line_rows && screen_row < SCREEN_HEIGHT; segment++) {
            if (segment == 0) {
                // First segment: show line number
                printf("\033[36m%4d\033[0m ", (int)(file_row + 1));
            } else {
                // Continuation: show spaces instead of line number
                printf("      ");
            }
            
            // Draw the segment of the line
            size_t start = segment * DISPLAY_WIDTH;
            size_t end = start + DISPLAY_WIDTH;
            if (end > line->length) end = line->length;
            
            for (size_t j = start; j < end; j++) {
                putchar(line->data[j]);
            }
            
            putchar('\n');
            screen_row++;
        }
        
        file_row++;
    }
    
    // Fill remaining screen with tildes
    while (screen_row < SCREEN_HEIGHT) {
        puts("\033[36m~\033[0m");
        screen_row++;
    }

    // Draw status bar (file info and cursor position)
    printf("\033[7m"); // Reverse video
    printf(" %s", editor->filename);
    printf("\033[0m"); // Reset before modified indicator
    if (editor->modified) {
        printf("\033[7m [+]\033[0m");
    } else {
        printf("\033[7m    \033[0m");
    }
    printf("\033[7m | Line %d/%d | Col %d | %s\033[K\033[0m\n",
           (int)(editor->cursor_y + 1), (int)editor->line_count,
           (int)(editor->cursor_x + 1),
           editor->insert_mode ? "INS" : "OVR");

    // Draw message/help bar
    if (editor->status_msg[0]) {
        puts(editor->status_msg);
        editor->status_msg[0] = '\0'; // Clear message after displaying
    } else {
        printf("\033[1m^S\033[0m Save  \033[1m^Q\033[0m Quit  \033[1mINS\033[0m Toggle Mode");
    }
    puts("\033[K"); // Clear to end of line

    // Calculate cursor position accounting for wrapping
    screen_row = 0;
    for (size_t i = editor->row_offset; i < editor->cursor_y && i < editor->line_count; i++) {
        line_t *line = &editor->lines[i];
        size_t line_rows = (line->length == 0) ? 1 : ((line->length + DISPLAY_WIDTH - 1) / DISPLAY_WIDTH);
        if (line_rows == 0) line_rows = 1;
        screen_row += line_rows;
    }
    
    // Add rows for current line up to cursor position
    size_t cursor_row_within_line = editor->cursor_x / DISPLAY_WIDTH;
    size_t cursor_col_within_row = editor->cursor_x % DISPLAY_WIDTH;
    screen_row += cursor_row_within_line;
    
    printf("\033[%d;%dH", (int)(screen_row + 1), (int)(cursor_col_within_row + LINE_NUM_WIDTH));
}

// ============================================================================
// Input Processing
// ============================================================================

/// @brief Processes a keypress.
/// @param editor Pointer to the editor state.
/// @return 0 to quit, 1 to continue.
static int editor_process_keypress(editor_state_t *editor)
{
    int c = getchar();

    // Handle escape sequences
    if (c == '\033') {
        int next = getchar();
        if (next == '[') {
            int seq = getchar();
            switch (seq) {
            case 'A': // Up arrow
                editor_move_cursor_up(editor);
                break;
            case 'B': // Down arrow
                editor_move_cursor_down(editor);
                break;
            case 'C': // Right arrow
                editor_move_cursor_right(editor);
                break;
            case 'D': // Left arrow
                editor_move_cursor_left(editor);
                break;
            case 'H': // Home
                editor->cursor_x = 0;
                break;
            case 'F': // End
                if (editor->cursor_y < editor->line_count) {
                    editor->cursor_x = editor->lines[editor->cursor_y].length;
                }
                break;
            case '2': // Insert key
                if (getchar() == '~') {
                    editor->insert_mode = !editor->insert_mode;
                }
                break;
            case '3': // Delete key
                if (getchar() == '~') {
                    editor_delete_char_forward(editor);
                }
                break;
            case '5': // Page up
                if (getchar() == '~') {
                    for (int i = 0; i < SCREEN_HEIGHT; i++) {
                        editor_move_cursor_up(editor);
                    }
                }
                break;
            case '6': // Page down
                if (getchar() == '~') {
                    for (int i = 0; i < SCREEN_HEIGHT; i++) {
                        editor_move_cursor_down(editor);
                    }
                }
                break;
            }
        }
        return 1;
    }

    // Handle control characters
    if (c == 0x13) { // Ctrl+S (Save)
        editor_save_file(editor);
        return 1;
    }
    if (c == 0x11) { // Ctrl+Q (Quit)
        if (editor->modified) {
            snprintf(editor->status_msg, sizeof(editor->status_msg), "Warning: Unsaved changes! Press Ctrl+Q again to quit.");
            editor->modified = 0; // Next Ctrl+Q will quit
            return 1;
        }
        return 0;
    }

    // Handle special keys
    if (c == '\n' || c == '\r') {
        editor_insert_newline(editor);
        return 1;
    }
    if (c == '\b' || c == 127) { // Backspace or DEL
        editor_delete_char(editor);
        return 1;
    }
    if (c == '\t') {
        // Insert spaces for tab
        for (int i = 0; i < TAB_SIZE; i++) {
            editor_insert_char(editor, ' ');
        }
        return 1;
    }

    // Handle printable characters
    if (c >= 32 && c < 127) {
        editor_insert_char(editor, (char)c);
        return 1;
    }

    return 1;
}

// ============================================================================
// Main Function
// ============================================================================

/// @brief Main entry point.
/// @param argc Number of arguments.
/// @param argv Argument array.
/// @return Exit status.
int main(int argc, char *argv[])
{
    if (argc != 2) {
        puts("Usage: edit <filename>");
        return 1;
    }

    editor_state_t editor;
    editor_init(&editor, argv[1]);

    if (editor_load_file(&editor) != 0) {
        puts("Error loading file");
        editor_free(&editor);
        return 1;
    }

    enable_raw_mode();

    int running = 1;
    while (running) {
        editor_scroll(&editor);
        editor_draw_screen(&editor);
        running = editor_process_keypress(&editor);
    }

    disable_raw_mode();
    puts("\033[H\033[J"); // Clear screen

    editor_free(&editor);
    return 0;
}
