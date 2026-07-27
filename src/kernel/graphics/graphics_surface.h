/* kernel/graphics/graphics_surface.h */
#ifndef GRAPHICS_SURFACE_H
#define GRAPHICS_SURFACE_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    GRAPHICS_FORMAT_RGBA8888,
    GRAPHICS_FORMAT_RGB888,
    GRAPHICS_FORMAT_RGB565
} graphics_pixel_format_t;

typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    graphics_pixel_format_t format;
    uint32_t *buffer;
    bool is_hardware_backed;
} graphics_surface_t;

graphics_surface_t* graphics_surface_create(uint32_t width, uint32_t height, graphics_pixel_format_t format);
void graphics_surface_destroy(graphics_surface_t *surface);
void graphics_surface_blit(graphics_surface_t *src, const void *src_rect, graphics_surface_t *dst, const void *dst_rect);

#endif