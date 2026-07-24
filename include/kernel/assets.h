#ifndef KERNEL_ASSETS_H
#define KERNEL_ASSETS_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    const char *name;
    const uint8_t *data;
    size_t size;
} asset_t;

const asset_t *asset_find(const char *name);

extern const asset_t kernel_assets[];
extern const size_t kernel_asset_count;

#endif

