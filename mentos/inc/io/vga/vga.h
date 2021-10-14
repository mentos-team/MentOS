
#pragma once

void vga_initialize();
void vga_finalize();

int vga_is_enabled();

int vga_width();
int vga_height();

void vga_clear_screen();

void vga_draw_char(unsigned int x, unsigned int y, unsigned char c, unsigned char color);

void vga_draw_string(int x, int y, char *str, unsigned char color);

void vga_draw_line(unsigned int x0, unsigned int y0, unsigned int x1, unsigned int y1, unsigned char color);

void vga_draw_rectangle(unsigned int sx, unsigned int sy, unsigned int ex, unsigned int ey, unsigned char fill);

void vga_draw_circle(unsigned int xc, unsigned int yc, unsigned int r, unsigned char col);

void vga_draw_triangle(unsigned int x1, unsigned int y1, unsigned int x2, unsigned int y2, unsigned int x3, unsigned int y3, unsigned char col);