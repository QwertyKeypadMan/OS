#ifndef KERNEL_MOUSE_H
#define KERNEL_MOUSE_H

#include <stdbool.h>

typedef struct {
    int dx;
    int dy;
    bool left_button;
    bool right_button;
    bool middle_button;
    bool overflow;
} mouse_packet_t;

bool mouse_initialize(void);
void mouse_shutdown(void);
bool mouse_poll(mouse_packet_t *packet);

#endif

