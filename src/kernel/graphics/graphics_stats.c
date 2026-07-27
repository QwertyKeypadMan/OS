/* kernel/graphics/graphics_stats.c */
#include "graphics_stats.h"

void graphics_stats_reset(graphics_stats_t *stats) {
    if (!stats) return;
    stats->frame_count = 0;
    stats->fps = 0;
    stats->draw_calls = 0;
    stats->dirty_rects_processed = 0;
    stats->last_fps_update_ms = 0;
}

void graphics_stats_on_frame(graphics_stats_t *stats, uint64_t current_time_ms) {
    if (!stats) return;
    stats->frame_count++;
    if (current_time_ms - stats->last_fps_update_ms >= 1000) {
        stats->fps = stats->frame_count;
        stats->frame_count = 0;
        stats->last_fps_update_ms = current_time_ms;
    }
}

void graphics_stats_inc_draw_call(graphics_stats_t *stats) {
    if (stats) stats->draw_calls++;
}