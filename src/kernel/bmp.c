#include "kernel/bmp.h"
#include "kernel/assets.h"
#include "kernel/graphics.h"

static uint16_t read_u16(const uint8_t *data)
{
    return (uint16_t)data[0] | ((uint16_t)data[1] << 8);
}

static uint32_t read_u32(const uint8_t *data)
{
    return (uint32_t)data[0] | ((uint32_t)data[1] << 8) |
        ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
}

static int32_t read_i32(const uint8_t *data)
{
    return (int32_t)read_u32(data);
}

/* 32-bit ARGB Renk Paketleme Makrosu */
#ifndef graphics_argb
#define graphics_argb(a, r, g, b) (((uint32_t)(a) << 24) | ((uint32_t)(r) << 16) | ((uint32_t)(g) << 8) | (uint32_t)(b))
#endif

/* graphics.c içinde tanımladığın alpha çizim fonksiyonu */
extern void graphics_draw_pixel_alpha(int x, int y, uint32_t color);

/* 1. Normal BMP Çizim Fonksiyonu (Alpha Destekli) */
bool bmp_draw(const uint8_t *data, size_t size, int x, int y, bool transparent_magenta)
{
    if (!graphics_available() || data == 0 || size < 54 || data[0] != 'B' || data[1] != 'M') {
        return false;
    }

    uint32_t pixel_offset = read_u32(data + 10);
    uint32_t dib_size = read_u32(data + 14);
    if (dib_size < 40 || pixel_offset >= size) {
        return false;
    }

    int32_t width = read_i32(data + 18);
    int32_t height_raw = read_i32(data + 22);
    uint16_t planes = read_u16(data + 26);
    uint16_t bpp = read_u16(data + 28);
    uint32_t compression = read_u32(data + 30);

    if (width <= 0 || height_raw == 0 || planes != 1 || compression != 0 ||
        (bpp != 24 && bpp != 32)) {
        return false;
    }

    bool top_down = height_raw < 0;
    int32_t height = top_down ? -height_raw : height_raw;
    uint32_t bytes_per_pixel = bpp / 8;
    uint32_t row_stride = ((uint32_t)width * bpp + 31) / 32 * 4;

    if (pixel_offset + row_stride * (uint32_t)height > size) {
        return false;
    }

    for (int32_t row = 0; row < height; row++) {
        int32_t source_y = top_down ? row : height - 1 - row;
        const uint8_t *source = data + pixel_offset + (uint32_t)source_y * row_stride;

        for (int32_t col = 0; col < width; col++) {
            const uint8_t *pixel = source + (uint32_t)col * bytes_per_pixel;
            uint8_t blue = pixel[0];
            uint8_t green = pixel[1];
            uint8_t red = pixel[2];
            uint8_t alpha = 255;

            if (bpp == 32) {
                alpha = pixel[3];
            }

            if (transparent_magenta && red == 255 && green == 0 && blue == 255) {
                continue;
            }

            uint32_t color = graphics_argb(alpha, red, green, blue);
            graphics_draw_pixel_alpha(x + col, y + row, color);
        }
    }

    return true;
}

/* 2. Normal Asset Çizimi */
bool bmp_draw_asset(const char *name, int x, int y, bool transparent_magenta)
{
    const asset_t *asset = asset_find(name);
    if (asset == 0) {
        return false;
    }

    return bmp_draw(asset->data, asset->size, x, y, transparent_magenta);
}

/* 3. Esneterek Çizim Fonksiyonu (Gölgeler İçin - Alpha Destekli) */
bool bmp_draw_stretched(const uint8_t *data, size_t size, int x, int y, int target_w, int target_h, bool transparent_magenta)
{
    if (!graphics_available() || data == 0 || size < 54 || data[0] != 'B' || data[1] != 'M') {
        return false;
    }

    uint32_t pixel_offset = read_u32(data + 10);
    uint32_t dib_size = read_u32(data + 14);
    if (dib_size < 40 || pixel_offset >= size) {
        return false;
    }

    int32_t width = read_i32(data + 18);
    int32_t height_raw = read_i32(data + 22);
    uint16_t planes = read_u16(data + 26);
    uint16_t bpp = read_u16(data + 28);
    uint32_t compression = read_u32(data + 30);

    if (width <= 0 || height_raw == 0 || planes != 1 || compression != 0 ||
        (bpp != 24 && bpp != 32)) {
        return false;
    }

    bool top_down = height_raw < 0;
    int32_t height = top_down ? -height_raw : height_raw;
    uint32_t bytes_per_pixel = bpp / 8;
    uint32_t row_stride = ((uint32_t)width * bpp + 31) / 32 * 4;

    if (pixel_offset + row_stride * (uint32_t)height > size) {
        return false;
    }

    for (int dst_y = 0; dst_y < target_h; dst_y++) {
        int32_t src_y = (dst_y * height) / target_h;
        int32_t source_y = top_down ? src_y : height - 1 - src_y;
        const uint8_t *row_source = data + pixel_offset + (uint32_t)source_y * row_stride;

        for (int dst_x = 0; dst_x < target_w; dst_x++) {
            int32_t src_x = (dst_x * width) / target_w;
            const uint8_t *pixel = row_source + (uint32_t)src_x * bytes_per_pixel;

            uint8_t blue = pixel[0];
            uint8_t green = pixel[1];
            uint8_t red = pixel[2];
            uint8_t alpha = 255;

            if (bpp == 32) {
                alpha = pixel[3];
            }

            if (transparent_magenta && red == 255 && green == 0 && blue == 255) {
                continue;
            }

            uint32_t color = graphics_argb(alpha, red, green, blue);
            graphics_draw_pixel_alpha(x + dst_x, y + dst_y, color);
        }
    }

    return true;
}

/* 4. Esneterek Asset Çizimi */
bool bmp_draw_stretched_asset(const char *name, int x, int y, int target_w, int target_h, bool transparent_magenta)
{
    const asset_t *asset = asset_find(name);
    if (asset == 0) {
        return false;
    }

    return bmp_draw_stretched(asset->data, asset->size, x, y, target_w, target_h, transparent_magenta);
}