/* kernel/graphics/graphics_backend.c */
#include "graphics_backend.h"
#include <stddef.h>

static graphics_backend_t *s_backends[MAX_BACKENDS];
static uint32_t s_backend_count = 0;

int graphics_backend_register(graphics_backend_t *backend) {
    if (!backend || s_backend_count >= MAX_BACKENDS) return -1;
    s_backends[s_backend_count++] = backend;
    return 0;
}

graphics_backend_t* graphics_backend_get(const char *name) {
    if (!name) return NULL;
    for (uint32_t i = 0; i < s_backend_count; i++) {
        /* C kütüphanesi strcmp kullanımı */
        const char *p1 = s_backends[i]->name;
        const char *p2 = name;
        while (*p1 && (*p1 == *p2)) { p1++; p2++; }
        if (*(const unsigned char*)p1 - *(const unsigned char*)p2 == 0) {
            return s_backends[i];
        }
    }
    return NULL;
}