/* kernel/graphics/framebuffer_backend.c */
#include "framebuffer_backend.h"
#include "graphics_manager.h"
#include <stddef.h>

static bool fb_init(graphics_context_t *ctx) {
    return (ctx != NULL && ctx->framebuffer != NULL);
}

static void fb_shutdown(graphics_context_t *ctx) {
    (void)ctx;
}

static void fb_clear(graphics_context_t *ctx, uint32_t color) {
    if (!ctx || !ctx->backbuffer) return;
    uint32_t size = ctx->width * ctx->height;
    uint32_t *buf = (uint32_t*)ctx->backbuffer;
    for (uint32_t i = 0; i < size; i++) {
        buf[i] = color;
    }
}

static void fb_draw_pixel(graphics_context_t *ctx, int x, int y, uint32_t color) {
    if (!ctx || !ctx->backbuffer) return;
    if (x < 0 || y < 0 || (uint32_t)x >= ctx->width || (uint32_t)y >= ctx->height) return;

    uint32_t *buf = (uint32_t*)ctx->backbuffer;
    buf[y * ctx->width + x] = color;
}

static void fb_fill_rect(graphics_context_t *ctx, int x, int y, int w, int h, uint32_t color) {
    if (!ctx || !ctx->backbuffer) return;

    for (int i = 0; i < h; i++) {
        for (int j = 0; j < w; j++) {
            fb_draw_pixel(ctx, x + j, y + i, color);
        }
    }
}

static void fb_draw_rect(graphics_context_t *ctx, int x, int y, int w, int h, uint32_t color) {
    for (int i = 0; i < w; i++) {
        fb_draw_pixel(ctx, x + i, y, color);
        fb_draw_pixel(ctx, x + i, y + h - 1, color);
    }
    for (int i = 0; i < h; i++) {
        fb_draw_pixel(ctx, x, y + i, color);
        fb_draw_pixel(ctx, x + w - 1, y + i, color);
    }
}

static void fb_draw_line(graphics_context_t *ctx, int x1, int y1, int x2, int y2, uint32_t color) {
    /* Bresenham Çizgi Algoritması */
    int dx = (x2 > x1) ? (x2 - x1) : (x1 - x2);
    int dy = (y2 > y1) ? (y2 - y1) : (y1 - y2);
    int sx = (x1 < x2) ? 1 : -1;
    int sy = (y1 < y2) ? 1 : -1;
    int err = dx - dy;

    while (1) {
        fb_draw_pixel(ctx, x1, y1, color);
        if (x1 == x2 && y1 == y2) break;
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x1 += sx; }
        if (e2 < dx)  { err += dx; y1 += sy; }
    }
}

static void fb_draw_bitmap(graphics_context_t *ctx, int x, int y, int w, int h, const uint32_t *data) {
    if (!ctx || !data) return;
    for (int i = 0; i < h; i++) {
        for (int j = 0; j < w; j++) {
            fb_draw_pixel(ctx, x + j, y + i, data[i * w + j]);
        }
    }
}

static void fb_present(graphics_context_t *ctx) {
    if (!ctx || !ctx->framebuffer || !ctx->backbuffer) return;
    
    /* Backbuffer'ı VRAM'e kopyala (Double Buffering) */
    uint32_t size_bytes = ctx->width * ctx->height * sizeof(uint32_t);
    uint32_t *dst = (uint32_t*)ctx->framebuffer;
    uint32_t *src = (uint32_t*)ctx->backbuffer;

    for (uint32_t i = 0; i < size_bytes / 4; i++) {
        dst[i] = src[i];
	kprintf("PRESENT!\n");
    }
}

static graphics_backend_t s_fb_backend = {
    .name = "framebuffer",
    .init = fb_init,
    .shutdown = fb_shutdown,
    .clear = fb_clear,
    .draw_pixel = fb_draw_pixel,
    .draw_line = fb_draw_line,
    .draw_rect = fb_draw_rect,
    .fill_rect = fb_fill_rect,
    .draw_bitmap = fb_draw_bitmap,
    .present = fb_present
};

graphics_backend_t* framebuffer_backend_get(void) {
    return &s_fb_backend;
}