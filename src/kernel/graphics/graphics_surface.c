/* kernel/graphics/graphics_surface.c */
#include "graphics_surface.h"
#include <stddef.h>

/* Sisteminizin malloc/free fonksiyonları çağrılır */
extern void *kmalloc(size_t size);
extern void kfree(void *ptr);

graphics_surface_t* graphics_surface_create(uint32_t width, uint32_t height, graphics_pixel_format_t format) {
    if (width == 0 || height == 0) return NULL;

    graphics_surface_t *surface = (graphics_surface_t*)kmalloc(sizeof(graphics_surface_t));
    if (!surface) return NULL;

    surface->width = width;
    surface->height = height;
    surface->pitch = width * sizeof(uint32_t);
    surface->format = format;
    surface->is_hardware_backed = false;
    surface->buffer = (uint32_t*)kmalloc(surface->pitch * height);

    if (!surface->buffer) {
        kfree(surface);
        return NULL;
    }

    return surface;
}

void graphics_surface_destroy(graphics_surface_t *surface) {
    if (!surface) return;
    if (surface->buffer && !surface->is_hardware_backed) {
        kfree(surface->buffer);
    }
    kfree(surface);
}

void graphics_surface_blit(graphics_surface_t *src, const void *src_rect, graphics_surface_t *dst, const void *dst_rect) {
    /* İleride donanımsal Blit veya H/W acceleration buraya yönlendirilebilir */
    (void)src; (void)src_rect; (void)dst; (void)dst_rect;
}