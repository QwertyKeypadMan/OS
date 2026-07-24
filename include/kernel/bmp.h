#ifndef KERNEL_BMP_H
#define KERNEL_BMP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

bool bmp_draw(const uint8_t *data, size_t size, int x, int y, bool transparent_magenta);
bool bmp_draw_asset(const char *name, int x, int y, bool transparent_magenta);
bool bmp_draw_stretched_asset(const char *name, int x, int y, int target_w, int target_h, bool transparent_magenta);

#endif

