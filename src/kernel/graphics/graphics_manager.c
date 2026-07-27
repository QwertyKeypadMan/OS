/* kernel/graphics/graphics_manager.c */
#include "graphics_manager.h"
#include "framebuffer_backend.h"
#include <stddef.h>

static graphics_context_t g_graphics_context;
static bool s_initialized = false;

bool graphics_manager_init(uint32_t width, uint32_t height, uint32_t bpp, void *framebuffer, void *backbuffer) {
    g_graphics_context.width = width;
    g_graphics_context.height = height;
    g_graphics_context.bpp = bpp;
    g_graphics_context.pitch = width * (bpp / 8);
    g_graphics_context.pixel_format = GRAPHICS_FORMAT_RGBA8888;
    g_graphics_context.framebuffer = framebuffer;
    g_graphics_context.backbuffer = backbuffer;

    graphics_dirty_init(&g_graphics_context.dirty_regions);
    graphics_stats_reset(&g_graphics_context.stats);
    graphics_renderqueue_init(&g_graphics_context.render_queue);

    /* Backend'leri Kaydet */
    graphics_backend_register(framebuffer_backend_get());

    /* Varsayılan Backend Olarak Framebuffer Seç */
    g_graphics_context.active_backend = framebuffer_backend_get();
    if (g_graphics_context.active_backend->init) {
        g_graphics_context.active_backend->init(&g_graphics_context);
    }

    s_initialized = true;
    return true;
}

graphics_context_t* graphics_manager_get_context(void) {
    return s_initialized ? &g_graphics_context : NULL;
}

bool graphics_manager_set_backend(const char *backend_name) {
    graphics_backend_t *backend = graphics_backend_get(backend_name);
    if (!backend) return false;

    if (g_graphics_context.active_backend && g_graphics_context.active_backend->shutdown) {
        g_graphics_context.active_backend->shutdown(&g_graphics_context);
    }

    g_graphics_context.active_backend = backend;
    if (backend->init) {
        backend->init(&g_graphics_context);
    }

    return true;
}

void graphics_manager_clear(uint32_t color) {
    if (s_initialized && g_graphics_context.active_backend->clear) {
        g_graphics_context.active_backend->clear(&g_graphics_context, color);
    }
}

void graphics_manager_draw_pixel(int x, int y, uint32_t color) {
    if (s_initialized && g_graphics_context.active_backend->draw_pixel) {
        g_graphics_context.active_backend->draw_pixel(&g_graphics_context, x, y, color);
        graphics_stats_inc_draw_call(&g_graphics_context.stats);
    }
}

void graphics_manager_draw_line(int x1, int y1, int x2, int y2, uint32_t color) {
    if (s_initialized && g_graphics_context.active_backend->draw_line) {
        g_graphics_context.active_backend->draw_line(&g_graphics_context, x1, y1, x2, y2, color);
        graphics_stats_inc_draw_call(&g_graphics_context.stats);
    }
}

void graphics_manager_draw_rect(int x, int y, int w, int h, uint32_t color) {
    if (s_initialized && g_graphics_context.active_backend->draw_rect) {
        g_graphics_context.active_backend->draw_rect(&g_graphics_context, x, y, w, h, color);
        graphics_stats_inc_draw_call(&g_graphics_context.stats);
    }
}

void graphics_manager_fill_rect(int x, int y, int w, int h, uint32_t color) {
    if (s_initialized && g_graphics_context.active_backend->fill_rect) {
        g_graphics_context.active_backend->fill_rect(&g_graphics_context, x, y, w, h, color);
        graphics_stats_inc_draw_call(&g_graphics_context.stats);
    }
}

void graphics_manager_draw_bitmap(int x, int y, int w, int h, const uint32_t *data) {
    if (s_initialized && g_graphics_context.active_backend->draw_bitmap) {
        g_graphics_context.active_backend->draw_bitmap(&g_graphics_context, x, y, w, h, data);
        graphics_stats_inc_draw_call(&g_graphics_context.stats);
    }
}

void graphics_manager_present(void) {
    if (s_initialized && g_graphics_context.active_backend->present) {
        g_graphics_context.active_backend->present(&g_graphics_context);
    }
}