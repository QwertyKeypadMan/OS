#ifndef GRAPHICS_H
#define GRAPHICS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "kernel/multiboot.h"

/* Temel Veri Tipleri */

/* Çekirdek API Fonksiyonları */
bool graphics_initialize(const multiboot_info_t *info);
bool graphics_available(void);
uint32_t graphics_width(void);
uint32_t graphics_height(void);
void graphics_present(void);
void graphics_clear(uint32_t color);
void graphics_scroll_up(uint32_t pixels, uint32_t fill_color);

/* Kırpma (Clipping) Fonksiyonları */
void graphics_set_clip(int32_t x, int32_t y, int32_t w, int32_t h);
void graphics_reset_clip(void);

/* İlkel Çizim API'leri */
void graphics_draw_pixel(int x, int y, uint32_t color);
void graphics_fill_rect(int x, int y, int width, int height, uint32_t color);
void graphics_draw_rect(int x, int y, int width, int height, uint32_t color);
void graphics_draw_rounded_rect(int x, int y, int w, int h, int r, uint32_t color);
void graphics_draw_line(int x0, int y0, int x1, int y1, uint32_t color);
void graphics_draw_triangle(int x0, int y0, int x1, int y1, int x2, int y2, uint32_t color);

/* Yazı Motoru API'leri */
void graphics_draw_char(int x, int y, char ch, uint32_t foreground, uint32_t background, int scale);
void graphics_draw_text(int x, int y, const char *text, uint32_t foreground, uint32_t background, int scale);
void graphics_draw_ttf_text(const uint8_t *ttf_buffer, const char *text, int x, int y, 
                            uint32_t foreground, uint32_t background, float font_size);

uint32_t graphics_rgb(uint8_t red, uint8_t green, uint8_t blue);

/* Kaba Kuvvet Eski Köprüler */
void put_pixel_fast(int x, int y, uint32_t color);
void update_screen(void);

#endif