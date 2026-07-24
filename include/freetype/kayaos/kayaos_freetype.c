/*
 * kayaos_freetype.c
 * ------------------
 * FreeType'i KayaOS'un framebuffer/graphics.c motoruna baglayan yapistirici
 * katman. graphics.c'deki graphics_draw_ttf_text() + 128 girisli ASCII
 * glyph cache deseniyle BILEREK ayni mimariyi tekrarliyor (bkz. proje
 * gecmisindeki "arena allocator sessiz tuzaktir" ogrenimi): FreeType'in
 * KENDI ic allocation'lari Newlib malloc/free uzerinden gercek serbest
 * birakma ile calisir (bkz. ftsystem_kayaos.c), ama bizim SONUC glyph
 * bitmap'lerimiz kalici oldugu icin kernel'in k_malloc arena'sinda
 * (tek seferlik, hic free edilmeyen) tutuluyor -- tipki mevcut TTF
 * glyph cache'i gibi.
 */

#include "kayaos_freetype.h"

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H

/* ---- Kernel taraflari (baslik yerine extern -- baska header'lara
 * bagimliligi minimumda tutmak icin; isterseniz kernel/graphics.h ve
 * kernel/kstring.h include edip bunlari silebilirsiniz). ---- */
extern bool     graphics_available( void );
extern void     graphics_draw_pixel( int x, int y, uint32_t color );
extern uint32_t graphics_rgb( uint8_t r, uint8_t g, uint8_t b );
extern void    *k_malloc( size_t size );

#define KFT_MAX_FACES        4
#define KFT_MAX_SIZE_SLOTS   6     /* Yuz basina en fazla farkli font boyutu */
#define KFT_GLYPH_CACHE_N    128   /* ASCII 0..127, graphics.c ile ayni sinir */

typedef struct {
    bool     valid;
    int16_t  width, height;   /* bitmap boyutu (0 ise bosluk/gorunmez glif) */
    int16_t  bearing_x, bearing_y;
    int16_t  advance;         /* 1/1 piksel (zaten yuvarlanmis) */
    uint8_t *coverage;        /* width*height byte, 0..255 alfa kaplamasi. k_malloc ile kalici. */
} kft_glyph_t;

typedef struct {
    bool         used;
    float        font_size;
    kft_glyph_t  glyphs[KFT_GLYPH_CACHE_N];
} kft_size_slot_t;

typedef struct {
    bool             used;
    FT_Face          face;
    kft_size_slot_t  slots[KFT_MAX_SIZE_SLOTS];
} kft_face_slot_t;

static FT_Library      s_library;
static bool             s_ready = false;
static kft_face_slot_t  s_faces[KFT_MAX_FACES];

/* Basit istatistikler (kayaos_ft_debug_stats icin) */
static uint32_t s_glyphs_rasterized = 0;
static uint32_t s_glyphs_from_cache = 0;

bool kayaos_ft_init( void )
{
    if ( s_ready )
        return true;

    if ( FT_Init_FreeType( &s_library ) != 0 )
        return false;

    for ( int i = 0; i < KFT_MAX_FACES; i++ )
        s_faces[i].used = false;

    s_ready = true;
    return true;
}

int kayaos_ft_load_font_from_memory( const uint8_t *ttf_buffer, size_t buffer_size )
{
    if ( !s_ready || ttf_buffer == 0 || buffer_size == 0 )
        return -1;

    int slot = -1;
    for ( int i = 0; i < KFT_MAX_FACES; i++ ) {
        if ( !s_faces[i].used ) { slot = i; break; }
    }
    if ( slot < 0 )
        return -1; /* KFT_MAX_FACES doldu; ihtiyaca gore artirin */

    FT_Face face;
    /* face_index = 0: TTC/OTC koleksiyonlarindaki ilk font. Buffer
     * FT_Face acik oldugu surece yasamak zorunda -- cagiran taraf
     * (RAMFS/assets) bunu garanti etmeli. */
    FT_Error err = FT_New_Memory_Face( s_library, ttf_buffer, (FT_Long)buffer_size, 0, &face );
    if ( err != 0 )
        return -1;

    s_faces[slot].used = true;
    s_faces[slot].face = face;
    for ( int i = 0; i < KFT_MAX_SIZE_SLOTS; i++ )
        s_faces[slot].slots[i].used = false;

    return slot;
}

static kft_size_slot_t *get_size_slot( kft_face_slot_t *fs, float font_size )
{
    for ( int i = 0; i < KFT_MAX_SIZE_SLOTS; i++ ) {
        if ( fs->slots[i].used && fs->slots[i].font_size == font_size )
            return &fs->slots[i];
    }
    for ( int i = 0; i < KFT_MAX_SIZE_SLOTS; i++ ) {
        if ( !fs->slots[i].used ) {
            fs->slots[i].used = true;
            fs->slots[i].font_size = font_size;
            for ( int g = 0; g < KFT_GLYPH_CACHE_N; g++ )
                fs->slots[i].glyphs[g].valid = false;
            /* FT_Set_Pixel_Sizes burada DEGIL, cagiran tarafta (asagida)
             * yapiliyor, cunku ayni FT_Face butun boyut slotlari arasinda
             * PAYLASILIYOR -- her cizimden once dogru boyuta geri
             * ayarlanmasi lazim. */
            return &fs->slots[i];
        }
    }
    return 0; /* KFT_MAX_SIZE_SLOTS doldu */
}

/* Bir ASCII karakteri rasterize edip (gerekirse) onbellege yazar. */
static kft_glyph_t *rasterize_glyph( kft_face_slot_t *fs, kft_size_slot_t *slot,
                                      FT_Face face, char ch )
{
    if ( ch < 0 || ch > 127 )
        ch = ' ';

    kft_glyph_t *g = &slot->glyphs[(unsigned char)ch];
    if ( g->valid ) {
        s_glyphs_from_cache++;
        return g;
    }

    (void)fs;

    if ( FT_Load_Char( face, (FT_ULong)(unsigned char)ch,
                        FT_LOAD_RENDER | FT_LOAD_NO_HINTING ) != 0 ) {
        g->valid = true; /* hatali glifi de "bos" olarak isaretle, tekrar denemeyelim */
        g->width = g->height = 0;
        g->advance = 0;
        g->coverage = 0;
        return g;
    }

    FT_GlyphSlot slot_ft = face->glyph;
    FT_Bitmap   *bmp = &slot_ft->bitmap;

    g->width     = (int16_t)bmp->width;
    g->height    = (int16_t)bmp->rows;
    g->bearing_x = (int16_t)slot_ft->bitmap_left;
    g->bearing_y = (int16_t)slot_ft->bitmap_top;
    g->advance   = (int16_t)( slot_ft->advance.x >> 6 ); /* 26.6 -> piksel */

    if ( g->width > 0 && g->height > 0 ) {
        size_t n = (size_t)g->width * (size_t)g->height;
        g->coverage = (uint8_t *)k_malloc( n );
        if ( g->coverage != 0 ) {
            /* FT_PIXEL_MODE_GRAY varsayimi (smooth renderer bunu uretir).
             * pitch negatif olabilir (asagidan yukariya bitmap); genel
             * durumu ele aliyoruz. */
            for ( int row = 0; row < g->height; row++ ) {
                const uint8_t *src = bmp->buffer + (ptrdiff_t)row * bmp->pitch;
                if ( bmp->pitch < 0 )
                    src = bmp->buffer + (ptrdiff_t)( g->height - 1 - row ) * ( -bmp->pitch );
                uint8_t *dst = g->coverage + (size_t)row * (size_t)g->width;
                for ( int col = 0; col < g->width; col++ )
                    dst[col] = src[col];
            }
        } else {
            g->width = g->height = 0; /* heap dolu: bu glifi sessizce atla */
        }
    } else {
        g->coverage = 0;
    }

    g->valid = true;
    s_glyphs_rasterized++;
    return g;
}

static uint32_t blend_coverage( uint32_t background, uint32_t foreground, uint8_t coverage )
{
    uint8_t bg_r = (uint8_t)( ( background >> 16 ) & 0xFF );
    uint8_t bg_g = (uint8_t)( ( background >> 8 ) & 0xFF );
    uint8_t bg_b = (uint8_t)( background & 0xFF );
    uint8_t fg_r = (uint8_t)( ( foreground >> 16 ) & 0xFF );
    uint8_t fg_g = (uint8_t)( ( foreground >> 8 ) & 0xFF );
    uint8_t fg_b = (uint8_t)( foreground & 0xFF );

    uint8_t r = (uint8_t)( ( bg_r * ( 255 - coverage ) + fg_r * coverage ) / 255 );
    uint8_t g = (uint8_t)( ( bg_g * ( 255 - coverage ) + fg_g * coverage ) / 255 );
    uint8_t b = (uint8_t)( ( bg_b * ( 255 - coverage ) + fg_b * coverage ) / 255 );

    return graphics_rgb( r, g, b );
}

void kayaos_ft_draw_text( int font_handle,
                           const char *text,
                           int x, int y,
                           uint32_t foreground, uint32_t background,
                           float font_size )
{
    if ( !s_ready || !graphics_available() || text == 0 )
        return;
    if ( font_handle < 0 || font_handle >= KFT_MAX_FACES || !s_faces[font_handle].used )
        return;
    if ( font_size <= 0.0f )
        return;

    kft_face_slot_t *fs = &s_faces[font_handle];
    FT_Face face = fs->face;

    kft_size_slot_t *slot = get_size_slot( fs, font_size );
    if ( slot == 0 )
        return;

    /* Ayni FT_Face butun boyutlar arasinda paylasildigi icin her cizimden
     * once dogru piksel boyutuna geri ayarlaniyor (ucuz bir islem,
     * FT_Set_Char_Size/Pixel_Sizes tablo tekrar yuklemez, sadece
     * olceklendirme katsayilarini gunceller). */
    FT_Set_Pixel_Sizes( face, 0, (FT_UInt)font_size );

    int ascent = (int)( ( (float)face->size->metrics.ascender ) / 64.0f );
    int baseline = y + ascent;
    int cursor_x = x;

    for ( size_t i = 0; text[i] != '\0'; i++ ) {
        char ch = text[i];

        if ( ch == '\n' ) {
            cursor_x = x;
            baseline += (int)( ( (float)face->size->metrics.height ) / 64.0f );
            continue;
        }

        kft_glyph_t *g = rasterize_glyph( fs, slot, face, ch );

        if ( g->width > 0 && g->height > 0 && g->coverage != 0 ) {
            int draw_x0 = cursor_x + g->bearing_x;
            int draw_y0 = baseline - g->bearing_y;

            for ( int row = 0; row < g->height; row++ ) {
                for ( int col = 0; col < g->width; col++ ) {
                    uint8_t coverage = g->coverage[(size_t)row * (size_t)g->width + col];
                    if ( coverage == 0 )
                        continue;
                    uint32_t final_color = blend_coverage( background, foreground, coverage );
                    graphics_draw_pixel( draw_x0 + col, draw_y0 + row, final_color );
                }
            }
        }

        cursor_x += g->advance;
    }
}

void kayaos_ft_debug_stats( void )
{
    /* terminal_writestring'e baglamak isterseniz burayi kernel/terminal.h
     * include ederek genisletebilirsiniz -- katmani izole tutmak icin
     * simdilik disariya deger dondurmuyoruz, sadece sayaclari tutuyoruz. */
    (void)s_glyphs_rasterized;
    (void)s_glyphs_from_cache;
}
