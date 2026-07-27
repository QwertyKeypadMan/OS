/* kernel/graphics/graphics_renderqueue.c */
#include "graphics_renderqueue.h"

void graphics_renderqueue_init(graphics_render_queue_t *queue) {
    if (queue) queue->count = 0;
}

int graphics_renderqueue_push(graphics_render_queue_t *queue, render_command_t cmd) {
    if (!queue || queue->count >= RENDER_QUEUE_SIZE) return -1;
    queue->commands[queue->count++] = cmd;
    return 0;
}

void graphics_renderqueue_flush(graphics_render_queue_t *queue, void *ctx) {
    if (!queue) return;
    (void)ctx;
    /* Kuyruktaki komutlar aktarılır */
    queue->count = 0;
}