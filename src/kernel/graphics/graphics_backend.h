/* kernel/graphics/graphics_backend.h */
#ifndef GRAPHICS_BACKEND_H
#define GRAPHICS_BACKEND_H

#include <stdint.h>
#include <stdbool.h>
#include "graphics_surface.h"

/* Forward declaration */
typedef struct graphics_context graphics_context_t;

typedef struct graphics_backend {
    const char *name;
    bool (*init)(graphics_context_t *ctx);
    void (*shutdown)(graphics_context_t *ctx);
    void (*clear)(graphics_context_t *ctx, uint32_t color);
    void (*draw_pixel)(graphics_context_t *ctx, int x, int y, uint32_t color);
    void (*draw_line)(graphics_context_t *ctx, int x1, int y1, int x2, int y2, uint32_t color);
    void (*draw_rect)(graphics_context_t *ctx, int x, int y, int w, int h, uint32_t color);
    void (*fill_rect)(graphics_context_t *ctx, int x, int y, int w, int h, uint32_t color);
    void (*draw_bitmap)(graphics_context_t *ctx, int x, int y, int w, int h, const uint32_t *data);
    void (*present)(graphics_context_t *ctx);
} graphics_backend_t;

#define MAX_BACKENDS 8

int graphics_backend_register(graphics_backend_t *backend);
graphics_backend_t* graphics_backend_get(const char *name);

#endif