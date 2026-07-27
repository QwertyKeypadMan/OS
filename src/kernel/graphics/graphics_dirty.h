/* kernel/graphics/graphics_dirty.h */
#ifndef GRAPHICS_DIRTY_H
#define GRAPHICS_DIRTY_H

#include <stdint.h>
#include <stdbool.h>

#define MAX_DIRTY_RECTS 64

typedef struct {
    int x, y, width, height;
} graphics_rect_t;

typedef struct {
    graphics_rect_t rects[MAX_DIRTY_RECTS];
    uint32_t count;
} graphics_dirty_set_t;

void graphics_dirty_init(graphics_dirty_set_t *set);
void graphics_dirty_add(graphics_dirty_set_t *set, graphics_rect_t rect);
void graphics_dirty_clear(graphics_dirty_set_t *set);
bool graphics_rect_clip(const graphics_rect_t *src, const graphics_rect_t *clip, graphics_rect_t *out);

#endif