/* kernel/graphics/graphics_renderqueue.h */
#ifndef GRAPHICS_RENDERQUEUE_H
#define GRAPHICS_RENDERQUEUE_H

#include <stdint.h>

typedef enum {
    RENDER_CMD_CLEAR,
    RENDER_CMD_DRAW_PIXEL,
    RENDER_CMD_DRAW_LINE,
    RENDER_CMD_FILL_RECT,
    RENDER_CMD_DRAW_BITMAP
} render_command_type_t;

typedef struct {
    render_command_type_t type;
    int x1, y1, x2, y2;
    uint32_t color;
    const uint32_t *data;
} render_command_t;

#define RENDER_QUEUE_SIZE 256

typedef struct {
    render_command_t commands[RENDER_QUEUE_SIZE];
    uint32_t count;
} graphics_render_queue_t;

void graphics_renderqueue_init(graphics_render_queue_t *queue);
int graphics_renderqueue_push(graphics_render_queue_t *queue, render_command_t cmd);
void graphics_renderqueue_flush(graphics_render_queue_t *queue, void *ctx);

#endif