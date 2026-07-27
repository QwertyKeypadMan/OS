/* kernel/graphics/graphics_dirty.c */
#include "graphics_dirty.h"

void graphics_dirty_init(graphics_dirty_set_t *set) {
    if (set) set->count = 0;
}

void graphics_dirty_add(graphics_dirty_set_t *set, graphics_rect_t rect) {
    if (!set || rect.width <= 0 || rect.height <= 0) return;
    if (set->count < MAX_DIRTY_RECTS) {
        set->rects[set->count++] = rect;
    }
}

void graphics_dirty_clear(graphics_dirty_set_t *set) {
    if (set) set->count = 0;
}

bool graphics_rect_clip(const graphics_rect_t *src, const graphics_rect_t *clip, graphics_rect_t *out) {
    if (!src || !clip || !out) return false;

    int x1 = (src->x > clip->x) ? src->x : clip->x;
    int y1 = (src->y > clip->y) ? src->y : clip->y;
    int x2 = (src->x + src->width < clip->x + clip->width) ? src->x + src->width : clip->x + clip->width;
    int y2 = (src->y + src->height < clip->y + clip->height) ? src->y + src->height : clip->y + clip->height;

    if (x2 <= x1 || y2 <= y1) {
        out->x = 0; out->y = 0; out->width = 0; out->height = 0;
        return false;
    }

    out->x = x1;
    out->y = y1;
    out->width = x2 - x1;
    out->height = y2 - y1;
    return true;
}