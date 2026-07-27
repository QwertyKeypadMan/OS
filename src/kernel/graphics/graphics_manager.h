/* kernel/graphics/graphics_manager.h */
#ifndef GRAPHICS_MANAGER_H
#define GRAPHICS_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "graphics_backend.h"
#include "graphics_surface.h"
#include "graphics_dirty.h"
#include "graphics_stats.h"
#include "graphics_renderqueue.h"

typedef struct graphics_context {
    graphics_backend_t *active_backend;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint32_t bpp;
    graphics_pixel_format_t pixel_format;
    
    void *framebuffer;  /* Fiziksel/VRAM adresi */
    void *backbuffer;   /* Arka plan tamponu */
    
    graphics_surface_t *target_surface;
    graphics_dirty_set_t dirty_regions;
    graphics_stats_t stats;
    graphics_render_queue_t render_queue;
} graphics_context_t;

bool graphics_manager_init(uint32_t width, uint32_t height, uint32_t bpp, void *framebuffer, void *backbuffer);
graphics_context_t* graphics_manager_get_context(void);
bool graphics_manager_set_backend(const char *backend_name);

void graphics_manager_clear(uint32_t color);
void graphics_manager_draw_pixel(int x, int y, uint32_t color);
void graphics_manager_draw_line(int x1, int y1, int x2, int y2, uint32_t color);
void graphics_manager_draw_rect(int x, int y, int w, int h, uint32_t color);
void graphics_manager_fill_rect(int x, int y, int w, int h, uint32_t color);
void graphics_manager_draw_bitmap(int x, int y, int w, int h, const uint32_t *data);
void graphics_manager_present(void);

#endif