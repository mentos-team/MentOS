/// @file vga.h
/// @brief Functions required to manage the Video Graphics Array (VGA).
/// @copyright (c) 2014-2021 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

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

/// @brief Draws a character at the given position.
/// @param x x-axis position.
/// @param y y-axis position.
/// @param c character to draw.
/// @param color color of the character.
void vga_draw_char(unsigned int x, unsigned int y, unsigned char c, unsigned char color);

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
void vga_draw_line(unsigned int x0, unsigned int y0, unsigned int x1, unsigned int y1, unsigned char color);

/// @brief Draws a rectangle provided the position of the starting corner and the ending corner.
/// @param sx starting corner x-axis position.
/// @param sy starting corner y-axis position.
/// @param ex ending corner x-axis position.
/// @param ey ending corner y-axis position.
/// @param fill color used to fill the rectangle.
void vga_draw_rectangle(unsigned int sx, unsigned int sy, unsigned int ex, unsigned int ey, unsigned char fill);

/// @brief Draws a circle provided the position of the center and the radius.
/// @param xc x-axis position.
/// @param yc y-axis position.
/// @param t radius.
/// @param fill color used to fill the circle.
void vga_draw_circle(unsigned int xc, unsigned int yc, unsigned int r, unsigned char fill);

/// @brief Draws a triangle.
/// @param x1 1st point x-axis position.
/// @param y1 1st point y-axis position.
/// @param x2 2nd point x-axis position. 
/// @param y2 2nd point y-axis position. 
/// @param x3 3rd point x-axis position. 
/// @param y3 3rd point y-axis position. 
/// @param fill color used to fill the triangle.
void vga_draw_triangle(unsigned int x1, unsigned int y1, unsigned int x2, unsigned int y2, unsigned int x3, unsigned int y3, unsigned char fill);