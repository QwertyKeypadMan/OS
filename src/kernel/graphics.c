#include "kernel/graphics.h"
#include "kernel/kstring.h"
#include <string.h> 
#include <stdint.h>
#include <stdbool.h>



/* ---- 📦 ÇEKİRDEK GLOBAL EKRAN DURUMU ---- */
uint8_t *framebuffer;      /* Gerçek VRAM Adresi (Front Buffer) */
uint32_t fb_width;
uint32_t fb_height;
uint32_t fb_pitch;
uint8_t fb_bpp;
static bool fb_ready = false;

/* Bootloader'ın ekran kartı bit dizilimine göre dinamik renk ayarlama maskeleri */
static uint8_t red_position;
static uint8_t red_size;
static uint8_t green_position;
static uint8_t green_size;
static uint8_t blue_position;
static uint8_t blue_size;

/* ---- 🛡️ KIRPMA BÖLGESİ (CLIP RECTANGLE - NUKLEAR İÇİN HAYATİ) ---- */
static int clip_x0;
static int clip_y0;
static int clip_x1;
static int clip_y1;

/* ---- 🎛️ ARKA TAMPON (DOUBLE-BUFFERING) MİMARİSİ ---- */
#define GRAPHICS_BACKBUFFER_MAX_BYTES (4u * 1024u * 1024u) /* Max 4MB sayfa hizalamalı tampon */
static uint8_t back_buffer[GRAPHICS_BACKBUFFER_MAX_BYTES] __attribute__((aligned(4096)));
static bool backbuffer_enabled = false;

/* ---- 🚨 KİRLİ BÖLGE (DIRTY RECTANGLE) TAKİBİ ---- */
static int dirty_min_x;
static int dirty_min_y;
static int dirty_max_x;
static int dirty_max_y;
static bool dirty_any = false;

/* ---- 💾 DIŞ BAĞLANTILAR (KERNEL ALLOCATOR & STRINGS) ---- */
extern void *k_malloc(size_t size);
extern void k_free(void *ptr);
extern size_t k_strlen(const char *str);
extern void *k_memset(void *dest, int value, size_t count);
extern void *k_memcpy(void *dest, const void *src, size_t count);

/* ---- 📐 STB_TRUETYPE ENTEGRASYON MATEMATİĞİ ---- */
#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_memcpy k_memcpy
#define STB_TRUETYPE_H

#define STBTT_malloc(x,u)   k_malloc(x)
#define STBTT_free(x,u)    k_free(x)
#define STBTT_strlen(x)    k_strlen(x)
#define STBTT_memset(d,v,c) k_memset(d,v,c)
#define STBTT_memcpy(d,s,c) k_memcpy(d,s,c)
#define STBTT_assert(x)    do { if(!(x)) { /* Kernel Panic */ } } while(0)

static inline float k_fabs(float x) { return x < 0 ? -x : x; }
#define STBTT_fabs(x)      k_fabs(x)

static inline float k_floor(float x) {
    int i = (int)x;
    return (x < i) ? (float)(i - 1) : (float)i;
}
#define STBTT_floor(x)     k_floor(x)

static inline float k_ceil(float x) {
    int i = (int)x;
    return (x > i) ? (float)(i + 1) : (float)i;
}
#define STBTT_ceil(x)      k_ceil(x)

static inline float k_sqrt(float x) {
    if (x <= 0.0f) return 0.0f;
    float z = x;
    for (int i = 0; i < 10; i++) {
        z = 0.5f * (z + x / z);
    }
    return z;
}
#define STBTT_sqrt(x)      k_sqrt(x)

static inline float k_fmod(float x, float y) {
    if (y == 0.0f) return 0.0f;
    float quotient = (float)((int)(x / y));
    return x - quotient * y;
}
#define STBTT_fmod(x, y)     k_fmod(x, y)

static inline float k_pow(float base, float exp) {
    if (exp == 0.0f) return 1.0f;
    if (base == 0.0f) return 0.0f;
    float result = 1.0f;
    int num = (int)(k_fabs(exp));
    for(int i = 0; i < num; i++) result *= base;
    if (exp < 0.0f) return 1.0f / result;
    return result;
}
#define STBTT_pow(x,y)     k_pow(x,y)

static inline float k_cos(float x) {
    float x2 = x * x;
    return 1.0f - (x2 / 2.0f) + (x2 * x2 / 24.0f);
}
#define STBTT_cos(x)       k_cos(x)

static inline float k_acos(float x) {
    if (x < -1.0f) x = -1.0f;
    if (x > 1.0f) x = 1.0f;
    float root = k_sqrt(1.0f - x);
    float result = 1.570796f - (0.570796f * x);
    return root * result;
}
#define STBTT_acos(x)      k_acos(x)

#include "kernel/stb_truetype.h"

/* ---- 🚨 KİRLİ BÖLGE VE BLITTING YÖNETİCİLERİ ---- */
static void mark_dirty(int x0, int y0, int x1, int y1) {
    if (x1 <= x0 || y1 <= y0) return;
    if (!dirty_any) {
        dirty_min_x = x0; dirty_min_y = y0;
        dirty_max_x = x1; dirty_max_y = y1;
        dirty_any = true;
        return;
    }
    if (x0 < dirty_min_x) dirty_min_x = x0;
    if (y0 < dirty_min_y) dirty_min_y = y0;
    if (x1 > dirty_max_x) dirty_max_x = x1;
    if (y1 > dirty_max_y) dirty_max_y = y1;
}

static void mark_dirty_all(void) {
    dirty_min_x = 0;
    dirty_min_y = 0;
    dirty_max_x = (int)fb_width;
    dirty_max_y = (int)fb_height;
    dirty_any = true;
}

/* REIS FIX: draw_wallpaper() ve draw_cursor() backbuffer'a graphics_draw_pixel'i
 * ATLAYARAK dogrudan yaziyor (performans icin), bu yuzden graphics_present()
 * bu bolgeleri hic "kirli" gormuyordu ve VRAM'e hic kopyalanmiyorlardi
 * (ya da sadece baska bir cizim tesaduf yakinlarindan gectiyse). Bu iki
 * fonksiyon gui.c'nin bu bolgeleri manuel isaretlemesini sagliyor. */
void graphics_mark_dirty_rect(int x0, int y0, int x1, int y1) {
    mark_dirty(x0, y0, x1, y1);
}

void graphics_mark_dirty_all(void) {
    mark_dirty_all();
}

static void fast_copy(void *dest, const void *src, size_t bytes) {
    uint32_t *d32 = (uint32_t *)dest;
    const uint32_t *s32 = (const uint32_t *)src;
    size_t words = bytes / 4;
    for (size_t i = 0; i < words; i++) d32[i] = s32[i];
    
    size_t consumed = words * 4;
    uint8_t *d8 = (uint8_t *)dest;
    const uint8_t *s8 = (const uint8_t *)src;
    for (size_t i = consumed; i < bytes; i++) d8[i] = s8[i];
}

/* ---- 🎨 DİNAMİK RENK PAKETLEME ÇARKLARI ---- */
uint32_t graphics_rgb(uint8_t red, uint8_t green, uint8_t blue) {
    return ((uint32_t)red << 16) | ((uint32_t)green << 8) | blue;
}

static uint32_t channel_value(uint8_t value, uint8_t mask_size) {
    if (mask_size == 0) return 0;
    if (mask_size >= 8) return (uint32_t)value << (mask_size - 8);
    return (uint32_t)value >> (8 - mask_size);
}

uint8_t *framebuffer;

// graphics.c içine ekleyip graphics.h'da tanımlayabilirsin
void set_framebuffer_ptr(uint8_t *new_address) {
    framebuffer = new_address;
}

static uint32_t pack_color(uint32_t rgb) {
    uint8_t red   = (uint8_t)((rgb >> 16) & 0xFF);
    uint8_t green = (uint8_t)((rgb >> 8) & 0xFF);
    uint8_t blue  = (uint8_t)(rgb & 0xFF);

    return (channel_value(red, red_size) << red_position) |
           (channel_value(green, green_size) << green_position) |
           (channel_value(blue, blue_size) << blue_position);
}

/* REİS: pack_color'ın tersi. Alfa blend ve imleç save-under için hedef
 * pikseli geri okuyup 0xRRGGBB'ye çevirmemiz lazım. */
static uint32_t unpack_channel(uint32_t native, uint8_t position, uint8_t size) {
    if (size == 0) return 0;
    uint32_t mask = (size >= 32) ? 0xFFFFFFFFu : ((1u << size) - 1u);
    uint32_t value = (native >> position) & mask;
    if (size >= 8) return value >> (size - 8);
    return value << (8 - size);
}

static uint32_t unpack_color(uint32_t native) {
    uint8_t r = (uint8_t)unpack_channel(native, red_position, red_size);
    uint8_t g = (uint8_t)unpack_channel(native, green_position, green_size);
    uint8_t b = (uint8_t)unpack_channel(native, blue_position, blue_size);
    return graphics_rgb(r, g, b);
}

static uint32_t blend_rgb(uint32_t background, uint32_t foreground, uint32_t coverage) {
    uint8_t bg_r = (uint8_t)((background >> 16) & 0xFF);
    uint8_t bg_g = (uint8_t)((background >> 8) & 0xFF);
    uint8_t bg_b = (uint8_t)(background & 0xFF);
    uint8_t fg_r = (uint8_t)((foreground >> 16) & 0xFF);
    uint8_t fg_g = (uint8_t)((foreground >> 8) & 0xFF);
    uint8_t fg_b = (uint8_t)(foreground & 0xFF);

    uint8_t r = (uint8_t)((bg_r * (255 - coverage) + fg_r * coverage) / 255);
    uint8_t g = (uint8_t)((bg_g * (255 - coverage) + fg_g * coverage) / 255);
    uint8_t b = (uint8_t)((bg_b * (255 - coverage) + fg_b * coverage) / 255);

    return graphics_rgb(r, g, b);
}

/* ---- 🔧 TEK PİKSEL OKUMA/YAZMA ÇEKİRDEĞİ (bpp'ye göre) ---- */
static inline uint32_t read_native_pixel(const uint8_t *p) {
    if (fb_bpp == 32) return *(const uint32_t *)p;
    if (fb_bpp == 24) return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16);
    if (fb_bpp == 16) return *(const uint16_t *)p;
    return 0;
}

static inline void write_native_pixel(uint8_t *p, uint32_t packed) {
    if (fb_bpp == 32) {
        *(uint32_t *)p = packed;
    } else if (fb_bpp == 24) {
        p[0] = (uint8_t)(packed & 0xFF);
        p[1] = (uint8_t)((packed >> 8) & 0xFF);
        p[2] = (uint8_t)((packed >> 16) & 0xFF);
    } else if (fb_bpp == 16) {
        *(uint16_t *)p = (uint16_t)packed;
    }
}

/* Bir satırı tek renkle doldurur (bpp'ye göre dword/word/byte-triplet store).
 * Piksel başına branch YOK, clip testi YOK - çağıran taraf bunları önceden
 * bir kere yapmış olmalı. */
static inline void fill_scanline_fast(uint8_t *row_ptr, int width, uint32_t packed) {
    if (fb_bpp == 32) {
        uint32_t *p = (uint32_t *)row_ptr;
        for (int i = 0; i < width; i++) p[i] = packed;
    } else if (fb_bpp == 16) {
        uint16_t *p = (uint16_t *)row_ptr;
        uint16_t v = (uint16_t)packed;
        for (int i = 0; i < width; i++) p[i] = v;
    } else if (fb_bpp == 24) {
        uint8_t *p = row_ptr;
        uint8_t b0 = (uint8_t)(packed & 0xFF);
        uint8_t b1 = (uint8_t)((packed >> 8) & 0xFF);
        uint8_t b2 = (uint8_t)((packed >> 16) & 0xFF);
        for (int i = 0; i < width; i++) {
            p[0] = b0; p[1] = b1; p[2] = b2;
            p += 3;
        }
    }
}

/* ---- 🎛️ KIRPMA AYARLARI (CRITICAL FOR WINDOW MANAGERS) ---- */
void graphics_set_clip(int32_t x, int32_t y, int32_t w, int32_t h) {
    int32_t x1 = x + w;
    int32_t y1 = y + h;

    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x1 > (int32_t)fb_width)  x1 = (int32_t)fb_width;
    if (y1 > (int32_t)fb_height) y1 = (int32_t)fb_height;

    clip_x0 = x;  clip_y0 = y;
    clip_x1 = (x1 < x) ? x : x1;
    clip_y1 = (y1 < y) ? y : y1;
}

void graphics_reset_clip(void) {
    clip_x0 = 0;
    clip_y0 = 0;
    clip_x1 = (int)fb_width;
    clip_y1 = (int)fb_height;
}

/* ---- 🛡️ SÜPER GÜVENLİ DARBOĞAZ PİKSEL MOTORU ---- */
/* Tek piksel çizimi hâlâ burada - çizgi/daire gibi düzensiz şekiller için
 * gerekli. Ama artık dikdörtgen/gradyan/blit gibi toplu işler bunu ARTIK
 * KULLANMIYOR, kendi hızlı satır döngülerine sahipler. */
void graphics_draw_pixel(int x, int y, uint32_t color) {
    if (!fb_ready || x < clip_x0 || x >= clip_x1 || y < clip_y0 || y >= clip_y1) {
        return;
    }

    uint32_t packed = pack_color(color);
    uint8_t *target = backbuffer_enabled ? back_buffer : framebuffer;
    uint8_t *pixel  = target + (uint32_t)y * fb_pitch + (uint32_t)x * (fb_bpp / 8);

    write_native_pixel(pixel, packed);

    if (backbuffer_enabled) {
        mark_dirty(x, y, x + 1, y + 1);
    }
}

/* ---- 📐 GEOMETRİK İLKEL ÇİZİM MOTORU (PRIMITIVES) ---- */
/* REİS FIX: Eskiden burası width*height kere graphics_draw_pixel çağırıyordu
 * (her pikselde: clip testi + pack_color yeniden hesabı + bpp branch +
 * fonksiyon call overhead). 1024x768'lik bir masaüstü fill'i 786.432 gereksiz
 * fonksiyon çağrısı demekti. Şimdi: dikdörtgen TEK SEFERDE clip'leniyor,
 * renk TEK SEFERDE paketleniyor, bpp TEK SEFERDE seçiliyor, iç döngü sadece
 * dword/word/byte store yapıyor. */
void graphics_fill_rect(int x, int y, int width, int height, uint32_t color) {
    if (!fb_ready || width <= 0 || height <= 0) return;

    int x0 = x, y0 = y;
    int x1 = x + width, y1 = y + height;

    if (x0 < clip_x0) x0 = clip_x0;
    if (y0 < clip_y0) y0 = clip_y0;
    if (x1 > clip_x1) x1 = clip_x1;
    if (y1 > clip_y1) y1 = clip_y1;
    if (x0 >= x1 || y0 >= y1) return;

    uint32_t packed = pack_color(color);
    uint8_t *target = backbuffer_enabled ? back_buffer : framebuffer;
    uint32_t bypp = fb_bpp / 8;
    int row_width = x1 - x0;
    uint8_t *row_ptr = target + (uint32_t)y0 * fb_pitch + (uint32_t)x0 * bypp;

    for (int row = y0; row < y1; row++) {
        fill_scanline_fast(row_ptr, row_width, packed);
        row_ptr += fb_pitch;
    }

    if (backbuffer_enabled) {
        mark_dirty(x0, y0, x1, y1);
    }
}

/* Yarı saydam dikdörtgen dolgu - pencere gölgesi, translucent panel,
 * seçim dikdörtgeni gibi "pro OS" efektleri için (Windows Aero / macOS
 * translucency mantığı). Her piksel hedefe bağlı olduğundan (arka planla
 * karışıyor) O(w*h) olmak zorunda, ama yine de piksel başına clip testi
 * ve fonksiyon-call overhead'i yok. */
void graphics_fill_rect_alpha(int x, int y, int width, int height, uint32_t color, uint8_t alpha) {
    if (!fb_ready || width <= 0 || height <= 0 || alpha == 0) return;
    if (alpha == 255) {
        graphics_fill_rect(x, y, width, height, color);
        return;
    }

    int x0 = x, y0 = y;
    int x1 = x + width, y1 = y + height;
    if (x0 < clip_x0) x0 = clip_x0;
    if (y0 < clip_y0) y0 = clip_y0;
    if (x1 > clip_x1) x1 = clip_x1;
    if (y1 > clip_y1) y1 = clip_y1;
    if (x0 >= x1 || y0 >= y1) return;

    uint8_t *target = backbuffer_enabled ? back_buffer : framebuffer;
    uint32_t bypp = fb_bpp / 8;

    for (int row = y0; row < y1; row++) {
        uint8_t *p = target + (uint32_t)row * fb_pitch + (uint32_t)x0 * bypp;
        for (int col = x0; col < x1; col++) {
            uint32_t bg = unpack_color(read_native_pixel(p));
            uint32_t blended = blend_rgb(bg, color, alpha);
            write_native_pixel(p, pack_color(blended));
            p += bypp;
        }
    }

    if (backbuffer_enabled) {
        mark_dirty(x0, y0, x1, y1);
    }
}

void graphics_draw_rect(int x, int y, int width, int height, uint32_t color) {
    graphics_fill_rect(x, y, width, 1, color);
    graphics_fill_rect(x, y + height - 1, width, 1, color);
    graphics_fill_rect(x, y, 1, height, color);
    graphics_fill_rect(x + width - 1, y, 1, height, color);
}

void graphics_draw_line(int x0, int y0, int x1, int y1, uint32_t color) {
    /* Yatay/dikey çizgiler çok sık kullanılır (kenarlıklar, ayraçlar) -
     * bunları piksel piksel Bresenham yerine doğrudan fast fill'e yönlendir. */
    if (y0 == y1) {
        int left = (x0 < x1) ? x0 : x1;
        int w = (x0 < x1) ? (x1 - x0 + 1) : (x0 - x1 + 1);
        graphics_fill_rect(left, y0, w, 1, color);
        return;
    }
    if (x0 == x1) {
        int top = (y0 < y1) ? y0 : y1;
        int h = (y0 < y1) ? (y1 - y0 + 1) : (y0 - y1 + 1);
        graphics_fill_rect(x0, top, 1, h, color);
        return;
    }

    int dx = (x1 - x0 >= 0) ? x1 - x0 : x0 - x1;
    int sx = (x0 < x1) ? 1 : -1;
    int dy = (y1 - y0 >= 0) ? y0 - y1 : y1 - y0;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx + dy;

    while (1) {
        graphics_draw_pixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void graphics_draw_triangle(int x0, int y0, int x1, int y1, int x2, int y2, uint32_t color) {
    graphics_draw_line(x0, y0, x1, y1, color);
    graphics_draw_line(x1, y1, x2, y2, color);
    graphics_draw_line(x2, y2, x0, y0, color);
}

/* REIS FIX (AA): tek bir pikseli arka planla "coverage" (0.0-1.0) oraninda
 * karistirarak yazar. coverage>=1 ise dogrudan (blend'siz) yazar -- yani
 * govde/kenar gibi tam-kapsama pikselleri hicbir ekstra maliyete girmiyor,
 * sadece kose kavis siniri (~1 piksellik bant) bu blend yolunu kullaniyor. */
static void draw_pixel_aa(int x, int y, uint32_t color, float coverage) {
    if (coverage <= 0.0f) return;
    if (coverage >= 1.0f) {
        graphics_draw_pixel(x, y, color);
        return;
    }
    if (!fb_ready || x < clip_x0 || x >= clip_x1 || y < clip_y0 || y >= clip_y1) return;

    uint8_t *target = backbuffer_enabled ? back_buffer : framebuffer;
    uint32_t bypp = fb_bpp / 8;
    uint8_t *p = target + (uint32_t)y * fb_pitch + (uint32_t)x * bypp;

    uint32_t bg = unpack_color(read_native_pixel(p));
    uint32_t blended = blend_rgb(bg, color, (uint32_t)(coverage * 255.0f));
    write_native_pixel(p, pack_color(blended));

    if (backbuffer_enabled) {
        mark_dirty(x, y, x + 1, y + 1);
    }
}

/* REIS FIX (AA): is_inside_rounded_rect'in bool halinin yerine, kose
 * bolgelerinde 0.0-1.0 arasi yumusak bir "coverage" dondurur -- boylece
 * kose kenarlari sert "merdiven" gibi degil, anti-aliased gorunur. Duz
 * kenar/govde pikselleri her zaman 1.0 dondurur. */
static float rounded_rect_coverage(int px, int py, int x, int y, int w, int h, int r) {
    if (px < x || px >= x + w || py < y || py >= y + h) return 0.0f;

    int ccx = 0, ccy = 0;
    bool in_corner = true;

    if (px < x + r && py < y + r)                { ccx = x + r;     ccy = y + r; }
    else if (px >= x + w - r && py < y + r)       { ccx = x + w - r; ccy = y + r; }
    else if (px < x + r && py >= y + h - r)       { ccx = x + r;     ccy = y + h - r; }
    else if (px >= x + w - r && py >= y + h - r)  { ccx = x + w - r; ccy = y + h - r; }
    else { in_corner = false; }

    if (!in_corner) return 1.0f;

    int dx = px - ccx;
    int dy = py - ccy;
    float dist = k_sqrt((float)(dx * dx + dy * dy));
    float coverage = (float)r - dist + 0.5f;
    if (coverage < 0.0f) coverage = 0.0f;
    if (coverage > 1.0f) coverage = 1.0f;
    return coverage;
}

/* REİS FIX: Eskiden bu fonksiyon w*h piksel için TEK TEK graphics_draw_pixel
 * çağırıyordu (köşe olsun olmasın!). Şimdi: orta düz gövde TEK fill_rect
 * çağrısıyla geçiliyor, üst/alt köşe bantlarında da sadece köşe yayının
 * bulunduğu dar r-genişliğindeki sütunlar piksel piksel test ediliyor,
 * aradaki düz kısım yine bulk fill. Büyük pencere başlık çubukları gibi
 * w >> r olan durumlarda bu onlarca kat daha az iş demek. */
void graphics_draw_rounded_rect(int x, int y, int w, int h, int r, uint32_t color) {
    if (!fb_ready || w <= 0 || h <= 0) return;
    if (r < 0) r = 0;
    if (r > w / 2) r = w / 2;
    if (r > h / 2) r = h / 2;

    if (r == 0) {
        graphics_fill_rect(x, y, w, h, color);
        return;
    }

    /* Orta düz gövde: köşe bantları dışında kalan tüm satırlar tek seferde */
    if (h > 2 * r) {
        graphics_fill_rect(x, y + r, w, h - 2 * r, color);
    }

    /* Üst ve alt köşe bantları (REİS FIX: artık anti-aliased) */
    for (int band = 0; band < 2; band++) {
        for (int row = 0; row < r; row++) {
            int cy = (band == 0) ? row : (h - r + row);
            int py = y + cy;

            for (int cx = 0; cx < r; cx++) {
                float coverage = rounded_rect_coverage(x + cx, py, x, y, w, h, r);
                if (coverage > 0.0f) {
                    draw_pixel_aa(x + cx, py, color, coverage);              /* sol köşe */
                    draw_pixel_aa(x + w - 1 - cx, py, color, coverage);      /* sağ köşe (ayna) */
                }
            }

            if (w - 2 * r > 0) {
                graphics_fill_rect(x + r, py, w - 2 * r, 1, color);      /* ortadaki düz kısım */
            }
        }
    }
}

void graphics_clear(uint32_t color) {
    graphics_fill_rect(0, 0, (int)fb_width, (int)fb_height, color);
}

/* Tek renkli dikey gradyan - duvar kağıdı / başlık çubuğu / buton parlaklığı
 * gibi "pro OS" efektleri için. Renk PİKSEL BAŞINA değil SATIR BAŞINA
 * hesaplanıyor: 1024 piksel genişlikte bir gradyan için 1024 kat daha az
 * renk hesabı, ve iç döngü yine fill_scanline_fast kullanıyor. */
void graphics_fill_gradient_v(int x, int y, int width, int height, uint32_t color_top, uint32_t color_bottom) {
    if (!fb_ready || width <= 0 || height <= 0) return;

    int x0 = x, y0 = y;
    int x1 = x + width, y1 = y + height;
    if (x0 < clip_x0) x0 = clip_x0;
    if (y0 < clip_y0) y0 = clip_y0;
    if (x1 > clip_x1) x1 = clip_x1;
    if (y1 > clip_y1) y1 = clip_y1;
    if (x0 >= x1 || y0 >= y1) return;

    int tr = (int)((color_top >> 16) & 0xFF);
    int tg = (int)((color_top >> 8) & 0xFF);
    int tb = (int)(color_top & 0xFF);
    int br = (int)((color_bottom >> 16) & 0xFF);
    int bg = (int)((color_bottom >> 8) & 0xFF);
    int bb = (int)(color_bottom & 0xFF);

    uint8_t *target = backbuffer_enabled ? back_buffer : framebuffer;
    uint32_t bypp = fb_bpp / 8;
    int row_width = x1 - x0;

    for (int row = y0; row < y1; row++) {
        int t = (height <= 1) ? 0 : (((row - y) * 255) / (height - 1));
        uint8_t r = (uint8_t)(tr + ((br - tr) * t) / 255);
        uint8_t g = (uint8_t)(tg + ((bg - tg) * t) / 255);
        uint8_t b = (uint8_t)(tb + ((bb - tb) * t) / 255);
        uint32_t packed = pack_color(graphics_rgb(r, g, b));

        uint8_t *row_ptr = target + (uint32_t)row * fb_pitch + (uint32_t)x0 * bypp;
        fill_scanline_fast(row_ptr, row_width, packed);
    }

    if (backbuffer_enabled) {
        mark_dirty(x0, y0, x1, y1);
    }
}

/* Ekran içi dikdörtgen kopyalama (X11'in XCopyArea / Win32'nin BitBlt'i).
 * Pencere sürüklerken ya da bir widget'ı kaydırırken, altındaki pikselleri
 * yeniden hesaplamak yerine doğrudan taşımak için kullanılır. Kaynak ve
 * hedef üst üste binebilir (satır yönü buna göre seçiliyor). */
void graphics_copy_rect(int dst_x, int dst_y, int src_x, int src_y, int w, int h) {
    if (!fb_ready || w <= 0 || h <= 0) return;

    uint8_t *target = backbuffer_enabled ? back_buffer : framebuffer;
    uint32_t bypp = fb_bpp / 8;
    size_t row_bytes = (size_t)w * bypp;

    if (dst_y <= src_y) {
        for (int row = 0; row < h; row++) {
            fast_copy(
                target + (size_t)(dst_y + row) * fb_pitch + (size_t)dst_x * bypp,
                target + (size_t)(src_y + row) * fb_pitch + (size_t)src_x * bypp,
                row_bytes);
        }
    } else {
        for (int row = h - 1; row >= 0; row--) {
            fast_copy(
                target + (size_t)(dst_y + row) * fb_pitch + (size_t)dst_x * bypp,
                target + (size_t)(src_y + row) * fb_pitch + (size_t)src_x * bypp,
                row_bytes);
        }
    }

    if (backbuffer_enabled) {
        mark_dirty(dst_x, dst_y, dst_x + w, dst_y + h);
    }
}

/* Alfa kanallı sprite/ikon çizimi (0xAARRGGBB piksel dizisi). Kürsör,
 * ikonlar, pencere kapatma butonları gibi küçük görseller için. */
void graphics_blit_argb(int x, int y, int w, int h, const uint32_t *pixels) {
    if (!fb_ready || pixels == 0 || w <= 0 || h <= 0) return;

    int sx0 = 0, sy0 = 0, sx1 = w, sy1 = h;
    int dx0 = x, dy0 = y;

    if (dx0 < clip_x0) { sx0 += clip_x0 - dx0; dx0 = clip_x0; }
    if (dy0 < clip_y0) { sy0 += clip_y0 - dy0; dy0 = clip_y0; }
    int dx1 = x + w, dy1 = y + h;
    if (dx1 > clip_x1) { sx1 -= dx1 - clip_x1; dx1 = clip_x1; }
    if (dy1 > clip_y1) { sy1 -= dy1 - clip_y1; dy1 = clip_y1; }
    if (sx0 >= sx1 || sy0 >= sy1) return;

    uint8_t *target = backbuffer_enabled ? back_buffer : framebuffer;
    uint32_t bypp = fb_bpp / 8;

    for (int row = sy0; row < sy1; row++) {
        const uint32_t *src_row = pixels + (size_t)row * w;
        uint8_t *dst = target + (uint32_t)(dy0 + (row - sy0)) * fb_pitch + (uint32_t)dx0 * bypp;

        for (int col = sx0; col < sx1; col++) {
            uint32_t argb = src_row[col];
            uint8_t alpha = (uint8_t)(argb >> 24);

            if (alpha != 0) {
                uint32_t rgb = argb & 0x00FFFFFF;
                if (alpha == 255) {
                    write_native_pixel(dst, pack_color(rgb));
                } else {
                    uint32_t bg = unpack_color(read_native_pixel(dst));
                    write_native_pixel(dst, pack_color(blend_rgb(bg, rgb, alpha)));
                }
            }
            dst += bypp;
        }
    }

    if (backbuffer_enabled) {
        mark_dirty(dx0, dy0, dx1, dy1);
    }
}

/* ---- 🖱️ YAZILIMSAL İMLEÇ (SAVE-UNDER) ----
 * DOS/klasik Mac OS/erken Windows'un donanım imleç katmanı olmadan
 * kullandığı klasik teknik: imlecin altındaki pikselleri bir kenara
 * kaydet, imleci çiz; bir sonraki karede önce eski konumu geri yükle,
 * sonra yeni konumda tekrar kaydet+çiz. Böylece fare her hareket
 * ettiğinde TÜM EKRANI değil, sadece imlecin küçük alanını yeniden
 * çizmiş olursunuz - windowmng_draw()'un her karede tam masaüstü
 * fill_rect yapmasının önündeki en büyük performans kazancı budur. */
#define CURSOR_MAX_DIM 32
static uint32_t cursor_backing[CURSOR_MAX_DIM * CURSOR_MAX_DIM];
static int cursor_saved_x = 0, cursor_saved_y = 0;
static int cursor_saved_w = 0, cursor_saved_h = 0;
static bool cursor_is_saved = false;

static void cursor_save_region(int x, int y, int w, int h) {
    uint8_t *target = backbuffer_enabled ? back_buffer : framebuffer;
    uint32_t bypp = fb_bpp / 8;

    for (int row = 0; row < h; row++) {
        uint8_t *p = target + (uint32_t)(y + row) * fb_pitch + (uint32_t)x * bypp;
        for (int col = 0; col < w; col++) {
            cursor_backing[row * w + col] = unpack_color(read_native_pixel(p));
            p += bypp;
        }
    }

    cursor_saved_x = x; cursor_saved_y = y;
    cursor_saved_w = w; cursor_saved_h = h;
    cursor_is_saved = true;
}

void graphics_cursor_hide(void) {
    if (!fb_ready || !cursor_is_saved) return;

    uint8_t *target = backbuffer_enabled ? back_buffer : framebuffer;
    uint32_t bypp = fb_bpp / 8;

    for (int row = 0; row < cursor_saved_h; row++) {
        uint8_t *p = target + (uint32_t)(cursor_saved_y + row) * fb_pitch + (uint32_t)cursor_saved_x * bypp;
        for (int col = 0; col < cursor_saved_w; col++) {
            write_native_pixel(p, pack_color(cursor_backing[row * cursor_saved_w + col]));
            p += bypp;
        }
    }

    if (backbuffer_enabled) {
        mark_dirty(cursor_saved_x, cursor_saved_y,
                   cursor_saved_x + cursor_saved_w, cursor_saved_y + cursor_saved_h);
    }

    cursor_is_saved = false;
}

uint32_t *graphics_get_backbuffer(void) {
    return back_buffer;
}

/* x,y : imlecin sol-üst köşesi (hotspot değil). argb: w*h boyutunda
 * 0xAARRGGBB dizisi, en fazla 32x32. Her karede: cursor_hide() (eski
 * konumu geri yükle) -> GUI çizimini yap -> cursor_show() (yeni konumda
 * kaydet + çiz) sırasıyla çağırın. */
void graphics_cursor_show(int x, int y, const uint32_t *argb, int w, int h) {
    if (!fb_ready || argb == 0 || w <= 0 || h <= 0) return;
    if (w > CURSOR_MAX_DIM) w = CURSOR_MAX_DIM;
    if (h > CURSOR_MAX_DIM) h = CURSOR_MAX_DIM;

    int cx0 = x, cy0 = y, cx1 = x + w, cy1 = y + h;
    if (cx0 < 0) cx0 = 0;
    if (cy0 < 0) cy0 = 0;
    if (cx1 > (int)fb_width)  cx1 = (int)fb_width;
    if (cy1 > (int)fb_height) cy1 = (int)fb_height;
    if (cx0 >= cx1 || cy0 >= cy1) return;

    cursor_save_region(cx0, cy0, cx1 - cx0, cy1 - cy0);
    graphics_blit_argb(x, y, w, h, argb);
}

void graphics_scroll_up(uint32_t pixels, uint32_t fill_color) {
    if (!fb_ready || pixels == 0 || pixels >= fb_height) {
        graphics_clear(fill_color);
        return;
    }

    uint32_t copy_rows = fb_height - pixels;
    uint8_t *target = backbuffer_enabled ? back_buffer : framebuffer;
    fast_copy(target, target + (size_t)pixels * fb_pitch, (size_t)copy_rows * fb_pitch);
    graphics_fill_rect(0, (int)copy_rows, (int)fb_width, (int)pixels, fill_color);

    if (backbuffer_enabled) {
        mark_dirty_all();
    }
}

/* ---- 🔤 KABA KUVVET (RAW) 8X8 FONT MATRİSİ & ÇİZİCİSİ ---- */
static const uint8_t raw_font8x8[95][8] = {
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, // space (32)
    {0x18,0x18,0x18,0x18,0x18,0x00,0x18,0x00}, // !
    {0x24,0x24,0x24,0x00,0x00,0x00,0x00,0x00}, // "
    {0x24,0x24,0x7E,0x24,0x7E,0x24,0x24,0x00}, // #
    {0x08,0x3E,0x1C,0x08,0x18,0x3E,0x08,0x00}, // $
    {0x62,0x66,0x0C,0x18,0x30,0x66,0x46,0x00}, // %
    {0x38,0x6C,0x38,0x6C,0x6C,0x6C,0x3A,0x00}, // &
    {0x18,0x18,0x08,0x00,0x00,0x00,0x00,0x00}, // '
    {0x0C,0x18,0x30,0x30,0x30,0x18,0x0C,0x00}, // (
    {0x30,0x18,0x0C,0x0C,0x0C,0x18,0x30,0x00}, // )
    {0x00,0x24,0x18,0x7E,0x18,0x24,0x00,0x00}, // *
    {0x00,0x10,0x10,0x7C,0x10,0x10,0x00,0x00}, // +
    {0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x08}, // ,
    {0x00,0x00,0x00,0x3E,0x00,0x00,0x00,0x00}, // -
    {0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x00}, // .
    {0x00,0x04,0x08,0x10,0x20,0x40,0x00,0x00}, // /
    {0x3C,0x46,0x4A,0x52,0x62,0x62,0x3C,0x00}, // 0 (48)
    {0x18,0x28,0x08,0x08,0x08,0x08,0x3E,0x00}, // 1
    {0x3C,0x42,0x02,0x3C,0x40,0x40,0x7E,0x00}, // 2
    {0x3C,0x42,0x02,0x1C,0x02,0x42,0x3C,0x00}, // 3
    {0x08,0x18,0x28,0x48,0x7E,0x08,0x08,0x00}, // 4
    {0x7E,0x40,0x40,0x7C,0x02,0x42,0x3C,0x00}, // 5
    {0x3C,0x40,0x40,0x7C,0x42,0x42,0x3C,0x00}, // 6
    {0x7E,0x02,0x04,0x08,0x10,0x10,0x10,0x00}, // 7
    {0x3C,0x42,0x42,0x3C,0x42,0x42,0x3C,0x00}, // 8
    {0x3C,0x42,0x42,0x3E,0x02,0x02,0x3C,0x00}, // 9
    {0x00,0x18,0x18,0x00,0x00,0x18,0x18,0x00}, // :
    {0x00,0x18,0x18,0x00,0x00,0x18,0x18,0x08}, // ;
    {0x0C,0x18,0x30,0x60,0x30,0x18,0x0C,0x00}, // <
    {0x00,0x00,0x3E,0x00,0x3E,0x00,0x00,0x00}, // =
    {0x30,0x18,0x0C,0x06,0x0C,0x18,0x30,0x00}, // >
    {0x3C,0x42,0x02,0x0C,0x10,0x00,0x10,0x00}, // ?
    {0x3C,0x42,0x5A,0x5A,0x5A,0x40,0x3C,0x00}, // @
    {0x18,0x24,0x42,0x42,0x7E,0x42,0x42,0x00}, // A (65)
    {0x7C,0x42,0x42,0x7C,0x40,0x40,0x40,0x00}, // B
    {0x3C,0x42,0x40,0x40,0x40,0x42,0x3C,0x00}, // C
    {0x78,0x44,0x42,0x42,0x42,0x44,0x78,0x00}, // D
    {0x7E,0x40,0x40,0x78,0x40,0x40,0x7E,0x00}, // E
    {0x7E,0x40,0x40,0x78,0x40,0x40,0x40,0x00}, // F
    {0x3C,0x42,0x40,0x4E,0x42,0x42,0x3C,0x00}, // G
    {0x42,0x42,0x42,0x7E,0x42,0x42,0x42,0x00}, // H
    {0x3E,0x08,0x08,0x08,0x08,0x08,0x3E,0x00}, // I
    {0x02,0x02,0x02,0x02,0x02,0x42,0x3C,0x00}, // J
    {0x44,0x48,0x50,0x60,0x50,0x48,0x44,0x00}, // K
    {0x40,0x40,0x40,0x40,0x40,0x40,0x7E,0x00}, // L
    {0x42,0x66,0x5A,0x42,0x42,0x42,0x42,0x00}, // M
    {0x42,0x62,0x52,0x4A,0x46,0x42,0x42,0x00}, // N
    {0x3C,0x42,0x42,0x42,0x42,0x42,0x3C,0x00}, // O
    {0x7C,0x42,0x42,0x7C,0x40,0x40,0x40,0x00}, // P
    {0x3C,0x42,0x42,0x42,0x42,0x4A,0x3C,0x02}, // Q
    {0x7C,0x42,0x42,0x7C,0x48,0x44,0x42,0x00}, // R
    {0x3C,0x42,0x40,0x3C,0x02,0x42,0x3C,0x00}, // S
    {0x7E,0x08,0x08,0x08,0x08,0x08,0x08,0x00}, // T
    {0x42,0x42,0x42,0x42,0x42,0x42,0x3C,0x00}, // U
    {0x42,0x42,0x42,0x42,0x42,0x24,0x18,0x00}, // V
    {0x42,0x42,0x42,0x4A,0x5A,0x66,0x42,0x00}, // W
    {0x42,0x42,0x24,0x18,0x24,0x42,0x42,0x00}, // X
    {0x42,0x42,0x24,0x18,0x08,0x08,0x08,0x00}, // Y
    {0x7E,0x02,0x04,0x08,0x10,0x20,0x7E,0x00}, // Z
    {0x3C,0x20,0x20,0x20,0x20,0x20,0x3C,0x00}, // [
    {0x00,0x40,0x20,0x10,0x08,0x04,0x00,0x00}, 
    {0x3C,0x02,0x02,0x02,0x02,0x02,0x3C,0x00}, // ]
    {0x08,0x14,0x22,0x00,0x00,0x00,0x00,0x00}, // ^
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFF}, // _
    {0x10,0x08,0x04,0x00,0x00,0x00,0x00,0x00}, // `
    {0x00,0x00,0x3C,0x02,0x3E,0x42,0x3E,0x00}, // a (97)
    {0x40,0x40,0x7C,0x42,0x42,0x42,0x7C,0x00}, // b
    {0x00,0x00,0x3C,0x40,0x40,0x42,0x3C,0x00}, // c
    {0x02,0x02,0x3E,0x42,0x42,0x42,0x3E,0x00}, // d
    {0x00,0x00,0x3C,0x42,0x7E,0x40,0x3C,0x00}, // e
    {0x1C,0x22,0x20,0x78,0x20,0x20,0x20,0x00}, // f
    {0x00,0x00,0x3E,0x42,0x42,0x3E,0x02,0x3C}, // g
    {0x40,0x40,0x7C,0x42,0x42,0x42,0x42,0x00}, // h
    {0x08,0x00,0x18,0x08,0x08,0x08,0x1C,0x00}, // i
    {0x02,0x00,0x06,0x02,0x02,0x42,0x3C,0x00}, // j
    {0x40,0x44,0x48,0x70,0x48,0x44,0x42,0x00}, // k
    {0x18,0x08,0x08,0x08,0x08,0x08,0x1C,0x00}, // l
    {0x00,0x00,0x66,0x5A,0x42,0x42,0x42,0x00}, // m
    {0x00,0x00,0x7C,0x42,0x42,0x42,0x42,0x00}, // n
    {0x00,0x00,0x3C,0x42,0x42,0x42,0x3C,0x00}, // o
    {0x00,0x00,0x7C,0x42,0x42,0x7C,0x40,0x40}, // p
    {0x00,0x00,0x3E,0x42,0x42,0x3E,0x02,0x02}, // q
    {0x00,0x00,0x5C,0x62,0x40,0x40,0x40,0x00}, // r
    {0x00,0x00,0x3E,0x40,0x3C,0x02,0x3E,0x00}, // s
    {0x20,0x20,0x78,0x20,0x20,0x22,0x1C,0x00}, // t
    {0x00,0x00,0x42,0x42,0x42,0x42,0x3E,0x00}, // u
    {0x00,0x00,0x42,0x42,0x42,0x24,0x18,0x00}, // v
    {0x00,0x00,0x42,0x42,0x5A,0x5A,0x24,0x00}, // w
    {0x00,0x00,0x42,0x24,0x18,0x24,0x42,0x00}, // x
    {0x00,0x00,0x42,0x42,0x42,0x3E,0x02,0x3C}, // y
    {0x00,0x00,0x7E,0x04,0x08,0x10,0x7E,0x00}, // z
    {0x0E,0x10,0x10,0x30,0x10,0x10,0x0E,0x00}, // {
    {0x08,0x08,0x08,0x00,0x08,0x08,0x08,0x00}, // |
    {0x70,0x08,0x08,0x0C,0x08,0x08,0x70,0x00}, // }
    {0x00,0x00,0x34,0x58,0x00,0x00,0x00,0x00}  // ~ (126)
};

void graphics_draw_char(int x, int y, char ch, uint32_t foreground, uint32_t background, int scale) {
    if (ch < 32 || ch > 126) ch = ' ';
    int font_index = ch - 32;
    if (scale < 1) scale = 1;

    for (int row = 0; row < 8; row++) {
        uint8_t line = raw_font8x8[font_index][row];
        for (int col = 0; col < 8; col++) {
            int pixel_active = (line & (0x80 >> col)) != 0;
            uint32_t color = pixel_active ? foreground : background;

            if (scale == 1) {
                graphics_draw_pixel(x + col, y + row, color);
            } else {
                graphics_fill_rect(x + col * scale, y + row * scale, scale, scale, color);
            }
        }
    }
}

void graphics_draw_text(int x, int y, const char *text, uint32_t foreground, uint32_t background, int scale) {
    if (!text) return;
    if (scale < 1) scale = 1;

    int cursor_x = x;
    int cursor_y = y;
    /* REİS FIX: Harf genişliği ve yüksekliği 8 pikseldir, scale ile çarpılmalıdır! */
    int font_width  = 8 * scale;
    int font_height = 8 * scale;

    for (size_t i = 0; text[i] != '\0'; i++) {
        if (text[i] == '\n') {
            cursor_x = x;
            cursor_y += font_height;
            continue;
        }

        if (cursor_x + font_width > (int)fb_width) {
            cursor_x = x;
            cursor_y += font_height;
        }

        graphics_draw_char(cursor_x, cursor_y, text[i], foreground, background, scale);
        cursor_x += font_width;
    }
}

/* ---- 🔤 GELİŞMİŞ TRUETYPE (TTF) YAZI MOTORU ---- */
/* REİS FIX: Eskiden HER KARAKTER için HER ÇAĞRIDA stbtt_MakeCodepointBitmap
 * çalıştırılıyor, bir malloc+free yapılıyordu. Bir terminal ekranı saniyede
 * onlarca kere yeniden çiziliyorsa bu, saniyede binlerce gereksiz
 * malloc/free ve bezier-eğrisi rasterizasyonu demekti. Şimdi ilk 128 ASCII
 * karakteri (font+punto boyutu değişmediği sürece) önbelleğe alınıyor;
 * ikinci çizimden itibaren malloc/free ve rasterizasyon SIFIR. */
#define GLYPH_CACHE_SIZE 128
typedef struct {
    bool valid;
    int width, height;
    int x0, y0;
    int advance;
    uint8_t *bitmap;
} glyph_cache_entry_t;

static glyph_cache_entry_t glyph_cache[GLYPH_CACHE_SIZE];
static const uint8_t *glyph_cache_font = 0;
static float glyph_cache_size = 0.0f;

static void glyph_cache_reset(void) {
    for (int i = 0; i < GLYPH_CACHE_SIZE; i++) {
        if (glyph_cache[i].valid && glyph_cache[i].bitmap != 0) {
            STBTT_free(glyph_cache[i].bitmap, NULL);
        }
        glyph_cache[i].valid = false;
        glyph_cache[i].bitmap = 0;
    }
}

void graphics_draw_ttf_text(const uint8_t *ttf_buffer, const char *text, int x, int y, 
                            uint32_t foreground, uint32_t background, float font_size) {
    if (!fb_ready || ttf_buffer == 0 || text == 0) return;

    /* Font ya da punto değiştiyse eski önbellek artık geçersiz */
    if (glyph_cache_font != ttf_buffer || glyph_cache_size != font_size) {
        glyph_cache_reset();
        glyph_cache_font = ttf_buffer;
        glyph_cache_size = font_size;
    }

    stbtt_fontinfo font;
    if (!stbtt_InitFont(&font, ttf_buffer, stbtt_GetFontOffsetForIndex(ttf_buffer, 0))) return;

    float scale = stbtt_ScaleForPixelHeight(&font, font_size);
    int ascent, descent, line_gap;
    stbtt_GetFontVMetrics(&font, &ascent, &descent, &line_gap);
    
    int baseline = y + (int)((float)ascent * scale);
    int cursor_x = x;

    for (size_t i = 0; text[i] != '\0'; i++) {
        char ch = text[i];

        glyph_cache_entry_t *entry = 0;
        if ((unsigned char)ch < GLYPH_CACHE_SIZE) {
            entry = &glyph_cache[(unsigned char)ch];
        }

        int advance, lsb;
        int gx0, gy0, gx1, gy1;
        int glyph_width, glyph_height;
        uint8_t *bitmap;
        bool owns_bitmap = false; /* entry'ye ait değilse biz free etmeliyiz */

        if (entry != 0 && entry->valid) {
            advance = entry->advance;
            gx0 = entry->x0; gy0 = entry->y0;
            glyph_width = entry->width; glyph_height = entry->height;
            bitmap = entry->bitmap;
        } else {
            stbtt_GetCodepointHMetrics(&font, ch, &advance, &lsb);
            stbtt_GetCodepointBitmapBox(&font, ch, scale, scale, &gx0, &gy0, &gx1, &gy1);
            glyph_width = gx1 - gx0;
            glyph_height = gy1 - gy0;
            bitmap = 0;

            if (glyph_width > 0 && glyph_height > 0) {
                bitmap = (uint8_t *)STBTT_malloc(glyph_width * glyph_height, NULL);
                if (bitmap != 0) {
                    stbtt_MakeCodepointBitmap(&font, bitmap, glyph_width, glyph_height,
                                              glyph_width, scale, scale, ch);
                }
            }

            if (entry != 0) {
                entry->valid = true;
                entry->advance = advance;
                entry->x0 = gx0; entry->y0 = gy0;
                entry->width = glyph_width; entry->height = glyph_height;
                entry->bitmap = bitmap; /* artık önbellek sahiplenir */
            } else {
                owns_bitmap = true; /* ASCII dışı: önbelleğe giremiyor, bu çağrıda kullan-ve-at */
            }
        }

        if (bitmap != 0 && glyph_width > 0 && glyph_height > 0) {
            for (int row = 0; row < glyph_height; row++) {
                for (int col = 0; col < glyph_width; col++) {
                    uint8_t coverage = bitmap[row * glyph_width + col];
                    if (coverage > 0) {
                        int pixel_x = cursor_x + gx0 + col;
                        int pixel_y = baseline + gy0 + row;
                        uint32_t final_color = blend_rgb(background, foreground, coverage);
                        graphics_draw_pixel(pixel_x, pixel_y, final_color);
                    }
                }
            }
        }

        if (owns_bitmap && bitmap != 0) {
            STBTT_free(bitmap, NULL);
        }

        cursor_x += (int)((float)advance * scale);

        if (text[i + 1] != '\0') {
            int kern = stbtt_GetCodepointKernAdvance(&font, ch, text[i + 1]);
            cursor_x += (int)((float)kern * scale);
        }
    }
}

/* ---- 🚀 ÇEKİRDEK İLKLENDİRME VE DOUBLE BUFFER TAKAS MOTORU ---- */
bool graphics_initialize(const multiboot_info_t *info) {
    fb_ready = false;

    // AKILLI ADRES SEÇİMİ (HYBRID)
    // Eğer info geçerliyse ve GRUB bir adres bildirmişse onu al (VirtualBox için)
    if (info && (info->flags & (1u << 12)) && info->framebuffer_addr_low != 0) {
        framebuffer = (uint8_t *)(uintptr_t)info->framebuffer_addr_low;
        fb_width    = info->framebuffer_width;
        fb_height   = info->framebuffer_height;
        fb_bpp      = info->framebuffer_bpp;                     
        fb_pitch    = info->framebuffer_pitch; 
    } 
    // Eğer GRUB adres bildirmemişse eski çalışan ayarlara geri dön (QEMU için Fallback)
    else {
        framebuffer = (uint8_t *)(uintptr_t)0xFD000000; /* Eski sabit adresin */
        fb_width    = 1024;
        fb_height   = 768;
        fb_bpp      = 32;                     
        fb_pitch    = 1024 * (32 / 8); 
    }
    
    // Eski çalışan renk sabitlerini aynen çakıyoruz
    red_position = 16;   red_size = 8;
    green_position = 8;  green_size = 8;
    blue_position = 0;   blue_size = 8;

    fb_ready = (framebuffer != 0 && fb_width > 0 && fb_height > 0 && fb_pitch > 0);

    if (fb_ready) {
        uint32_t needed_bytes = fb_pitch * fb_height;
        backbuffer_enabled = (needed_bytes <= sizeof(back_buffer));
        if (backbuffer_enabled) {
            for (uint32_t i = 0; i < needed_bytes; i++) {
                back_buffer[i] = 0;
            }
        }
    } else {
        backbuffer_enabled = false;
    }

    graphics_reset_clip();
    dirty_any = false;
    cursor_is_saved = false;
    glyph_cache_reset();
    glyph_cache_font = 0;
    glyph_cache_size = 0.0f;

    if (fb_ready) {
        mark_dirty_all();
        
        uint32_t* fb32 = (uint32_t*)framebuffer;
        for (uint32_t i = 0; i < fb_width * fb_height; i++) {
         
        }
    }

    return fb_ready;
}


bool graphics_available(void) {
    return fb_ready;
}

uint32_t graphics_width(void) {  return fb_width; }
uint32_t graphics_height(void) { return fb_height; }

/* REİS KABA KUVVET KÖPRÜLERİ */
void put_pixel_fast(int x, int y, uint32_t color) { graphics_draw_pixel(x, y, color); }
void update_screen(void) { graphics_present(); }

/* Rengi arka planla karıştırır (Alpha: 0-255) */
uint32_t blend_colors(uint32_t fg, uint32_t bg, uint8_t alpha) {
    uint32_t inv_alpha = 255 - alpha;
    uint8_t r = (( (fg >> 16 & 0xFF) * alpha ) + ( (bg >> 16 & 0xFF) * inv_alpha )) >> 8;
    uint8_t g = (( (fg >> 8 & 0xFF) * alpha ) + ( (bg >> 8 & 0xFF) * inv_alpha )) >> 8;
    uint8_t b = (( (fg & 0xFF) * alpha ) + ( (bg & 0xFF) * inv_alpha )) >> 8;
    return (r << 16) | (g << 8) | b;
}

uint32_t graphics_get_pixel(int x, int y) {
    // Burası senin framebuffer yapına göre değişebilir
    // Genelde: framebuffer_ptr + (y * screen_width + x) * (bytes_per_pixel)
    return *(uint32_t*)((uintptr_t)framebuffer + (y * (int)graphics_width() + x) * 4);
}

/* Rounded Rect içinde olup olmadığımızı kontrol eden yardımcı */
bool is_inside_rounded_rect(int px, int py, int x, int y, int w, int h, int r) {
    // Orta dikdörtgen ve 4 köşe kontrolü (basit bounding box mantığı)
    if (px < x || px >= x + w || py < y || py >= y + h) return false;
    
    // Köşeler (r yarıçaplı daire kontrolü)
    if (px < x + r && py < y + r) return ((px - (x + r)) * (px - (x + r)) + (py - (y + r)) * (py - (y + r))) <= r * r;
    if (px >= x + w - r && py < y + r) return ((px - (x + w - r)) * (px - (x + w - r)) + (py - (y + r)) * (py - (y + r))) <= r * r;
    if (px < x + r && py >= y + h - r) return ((px - (x + r)) * (px - (x + r)) + (py - (y + h - r)) * (py - (y + h - r))) <= r * r;
    if (px >= x + w - r && py >= y + h - r) return ((px - (x + w - r)) * (px - (x + w - r)) + (py - (y + h - r)) * (py - (y + h - r))) <= r * r;
    
    return true;
}

// graphics.h veya graphics.c içine
#define graphics_argb(a, r, g, b) (((uint32_t)(a) << 24) | ((uint32_t)(r) << 16) | ((uint32_t)(g) << 8) | (uint32_t)(b))

void graphics_draw_pixel_alpha(int x, int y, uint32_t src_color) {
    if (x < 0 || x >= (int)graphics_width() || y < 0 || y >= (int)graphics_height()) {
        return;
    }

    uint8_t alpha = (src_color >> 24) & 0xFF;

    // Tamamen saydam ise hiçbir şey yapma
    if (alpha == 0) return;

    // Tamamen opak ise doğrudan üzerine yaz (arka planı okumaya gerek yok, hız kazandırır)
    if (alpha == 255) {
        graphics_draw_pixel(x, y, src_color & 0x00FFFFFF); 
        return;
    }

    // Ekrandaki mevcut rengi (hedef rengi) framebuffer'dan oku
    // NOT: graphics_get_pixel fonksiyonun yoksa arka plan belleğinden (backbuffer) okumalısın.
    uint32_t dst_color = graphics_get_pixel(x, y); 

    uint8_t src_r = (src_color >> 16) & 0xFF;
    uint8_t src_g = (src_color >> 8) & 0xFF;
    uint8_t src_b = src_color & 0xFF;

    uint8_t dst_r = (dst_color >> 16) & 0xFF;
    uint8_t dst_g = (dst_color >> 8) & 0xFF;
    uint8_t dst_b = dst_color & 0xFF;

    // Hızlı Alpha Blending Algoritması (Tam sayı bölmesiyle)
    uint8_t out_r = (uint8_t)(((src_r - dst_r) * alpha) >> 8) + dst_r;
    uint8_t out_g = (uint8_t)(((src_g - dst_g) * alpha) >> 8) + dst_g;
    uint8_t out_b = (uint8_t)(((src_b - dst_b) * alpha) >> 8) + dst_b;

    // Yeni rengi ekrana bas
    graphics_draw_pixel(x, y, graphics_rgb(out_r, out_g, out_b));
}

/* Alpha destekli Rounded Rect çizici (REİS FIX: artık köşe kenarlarında
 * anti-aliased -- coverage, verilen alpha ile çarpılıyor, yani hem
 * saydamlık hem yumuşak kenar aynı anda çalışıyor). */
void fill_rounded_rect_alpha(int x, int y, int w, int h, int r, uint32_t color, uint8_t alpha) {
    for (int iy = y; iy < y + h; iy++) {
        for (int ix = x; ix < x + w; ix++) {
            float coverage = rounded_rect_coverage(ix, iy, x, y, w, h, r);
            if (coverage <= 0.0f) continue;
            uint32_t bg = graphics_get_pixel(ix, iy); // Arka planı oku
            uint8_t effective_alpha = (uint8_t)((float)alpha * coverage);
            uint32_t mixed = blend_colors(color, bg, effective_alpha); // Karıştır
            graphics_draw_pixel(ix, iy, mixed);
        }
    }
}

void graphics_present(void) {
    if (!fb_ready || !backbuffer_enabled || !dirty_any) {
        return;
    }

    int x0 = dirty_min_x;
    int y0 = dirty_min_y;
    int x1 = dirty_max_x;
    int y1 = dirty_max_y;

    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > (int)fb_width)  x1 = (int)fb_width;
    if (y1 > (int)fb_height) y1 = (int)fb_height;

    dirty_any = false;

    if (x0 >= x1 || y0 >= y1) return;

    uint32_t bytes_per_pixel = fb_bpp / 8;
    size_t row_bytes = (size_t)(x1 - x0) * bytes_per_pixel;

    /* Tam ekran kirliyse tek seferde kopyala */
    if (x0 == 0 && x1 == (int)fb_width) {
        size_t offset = (size_t)y0 * fb_pitch;
        size_t bytes  = (size_t)(y1 - y0) * fb_pitch;
        fast_copy(framebuffer + offset, back_buffer + offset, bytes);
        return;
    }

    /* Sadece değişen satır aralıklarını VRAM'e fırlat */
    for (int row = y0; row < y1; row++) {
        size_t offset = (size_t)row * fb_pitch + (size_t)x0 * bytes_per_pixel;
        fast_copy(framebuffer + offset, back_buffer + offset, row_bytes);
    }
}