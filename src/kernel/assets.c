#include "kernel/assets.h"

#include "kernel/kstring.h"

const asset_t *asset_find(const char *name)
{
    for (size_t i = 0; i < kernel_asset_count; i++) {
        if (kernel_assets[i].name != 0 && k_streq(kernel_assets[i].name, name)) {
            return &kernel_assets[i];
        }
    }

    return 0;
}

