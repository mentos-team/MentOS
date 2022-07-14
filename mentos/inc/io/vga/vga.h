/// @file vga.h
/// @brief Functions required to manage the Video Graphics Array (VGA).
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

/// @brief Initializes the VGA.
void vga_initialize();

/// @brief Finalizes the VGA.
void vga_finalize();

/// @brief Checks if the VGA is enabled.
/// @return 1 if enabled, 0 otherwise.
int vga_is_enabled();

/// @brief Returns the width of the screen.
/// @return the width of the screen.
int vga_width();

/// @brief Returns the height of the screen.
/// @return the height of the screen.
int vga_height();

/// @brief Clears the screen.
void vga_clear_screen();

/// @brief Draws a pixel at the given position.
/// @param x x-axis position.
/// @param y y-axis position.
/// @param color color of the character.
void vga_draw_pixel(int x, int y, unsigned char color);

/// @brief Reads a pixel at the given position.
/// @param x x-axis position.
/// @param y y-axis position.
unsigned int vga_read_pixel(int x, int y);

/// @brief Draws a character at the given position.
/// @param x x-axis position.
/// @param y y-axis position.
/// @param c character to draw.
/// @param color color of the character.
void vga_draw_char(int x, int y, unsigned char c, unsigned char color);

/// @brief Draws a string at the given position.
/// @param x x-axis position.
/// @param y y-axis position.
/// @param str string to draw.
/// @param color color of the character.
void vga_draw_string(int x, int y, char *str, unsigned char color);

/// @brief Draws a line from point 1 to point 2.
/// @param x0 point 1 x-axis position.
/// @param y0 point 1 y-axis position.
/// @param x1 point 2 x-axis position.
/// @param y1 point 2 y-axis position.
/// @param color color of the line.
void vga_draw_line(int x0, int y0, int x1, int y1, unsigned char color);

/// @brief Draws a rectangle provided the position of the starting corner and the ending corner.
/// @param sx top-left corner x-axis position.
/// @param sy top-left corner y-axis position.
/// @param w width.
/// @param h height.
/// @param color color of the rectangle.
void vga_draw_rectangle(int sx, int sy, int w, int h, unsigned char color);

/// @brief Draws a circle provided the position of the center and the radius.
/// @param xc x-axis position.
/// @param yc y-axis position.
/// @param r radius.
/// @param color used to draw the circle.
void vga_draw_circle(int xc, int yc, int r, unsigned char color);

/// @brief Draws a triangle.
/// @param x1 1st point x-axis position.
/// @param y1 1st point y-axis position.
/// @param x2 2nd point x-axis position. 
/// @param y2 2nd point y-axis position. 
/// @param x3 3rd point x-axis position. 
/// @param y3 3rd point y-axis position. 
/// @param color used to draw the triangle.
void vga_draw_triangle(int x1, int y1, int x2, int y2, int x3, int y3, unsigned char color);

void vga_run_test();