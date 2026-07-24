#ifndef KERNEL_TERMINAL_H
#define KERNEL_TERMINAL_H

#include <stddef.h>
#include <stdint.h>

typedef enum {
    VGA_COLOR_BLACK = 0,
    VGA_COLOR_BLUE = 1,
    VGA_COLOR_GREEN = 2,
    VGA_COLOR_CYAN = 3,
    VGA_COLOR_RED = 4,
    VGA_COLOR_MAGENTA = 5,
    VGA_COLOR_BROWN = 6,
    VGA_COLOR_LIGHT_GREY = 7,
    VGA_COLOR_DARK_GREY = 8,
    VGA_COLOR_LIGHT_BLUE = 9,
    VGA_COLOR_LIGHT_GREEN = 10,
    VGA_COLOR_LIGHT_CYAN = 11,
    VGA_COLOR_LIGHT_RED = 12,
    VGA_COLOR_LIGHT_MAGENTA = 13,
    VGA_COLOR_LIGHT_BROWN = 14,
    VGA_COLOR_WHITE = 15
} vga_color_t;

void terminal_initialize(void);
void terminal_clear(void);
void terminal_setcolor(vga_color_t foreground, vga_color_t background);
void terminal_putchar(char ch);
void terminal_draw_at(char ch, vga_color_t foreground, vga_color_t background, size_t x, size_t y);
void terminal_fill_rect(size_t x, size_t y, size_t width, size_t height, char ch,
    vga_color_t foreground, vga_color_t background);
size_t terminal_width(void);
size_t terminal_height(void);
void terminal_write(const char *data, size_t size);
void terminal_writestring(const char *data);
void terminal_write_dec(uint32_t value);
void terminal_write_hex(uint32_t value);

#endif
