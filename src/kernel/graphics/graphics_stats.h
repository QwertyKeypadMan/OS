/* kernel/graphics/graphics_stats.h */
#ifndef GRAPHICS_STATS_H
#define GRAPHICS_STATS_H

#include <stdint.h>

typedef struct {
    uint32_t frame_count;
    uint32_t fps;
    uint32_t draw_calls;
    uint32_t dirty_rects_processed;
    uint64_t last_fps_update_ms;
} graphics_stats_t;

void graphics_stats_reset(graphics_stats_t *stats);
void graphics_stats_on_frame(graphics_stats_t *stats, uint64_t current_time_ms);
void graphics_stats_inc_draw_call(graphics_stats_t *stats);

#endif