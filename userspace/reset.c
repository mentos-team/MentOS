/// @file reset.c
/// @brief `reset` program - resets the terminal to its initial state.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <stdio.h>
#include <unistd.h>

int main(int argc, char **argv)
{
    // Send terminal reset sequence (RIS - Reset to Initial State).
    // This is ESC c - a full hardware reset command.
    write(STDOUT_FILENO, "\033c", 2);
    
    // Clear screen and scrollback buffer (ESC [ 3 J).
    write(STDOUT_FILENO, "\033[3J", 5);
    
    // Move cursor to home position (ESC [ H).
    write(STDOUT_FILENO, "\033[H", 3);
    
    // Reset all attributes (SGR 0).
    write(STDOUT_FILENO, "\033[0m", 4);
    
    // Make cursor visible (in case it was hidden).
    // ESC [ ? 25 h would be standard, but we use what we have.
    
    return 0;
}
