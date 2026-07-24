#ifndef KAYAOS_FREETYPE_H
#define KAYAOS_FREETYPE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/*
 * KayaOS FreeType Katmani
 * ------------------------
 * graphics.c'deki mevcut graphics_draw_ttf_text() (stb_truetype tabanli)
 * ile AYNI CAGRI SEKLINE sahip, drop-in bir alternatif sunar. Boylece
 * gui.c / windowmng.c / terminal.c tarafinda tek satir degisikligiyle
 * (fonksiyon adini degistirerek) FreeType motoruna gecebilirsiniz.
 *
 * Font, RAMFS/assets.c uzerinden bellekte hazir bir byte buffer olarak
 * gelir (stb_truetype ile ayni varsayim) -- diskten akan bir stream
 * DEGILDIR.
 */

/* Cekirdek acilisinda BIR KERE cagrilir (gui_init() gibi bir yerden). */
bool kayaos_ft_init( void );

/*
 * ttf_buffer: RAMFS/assets icindeki .ttf dosyasinin ham byte'lari
 *             (kalici olmali; FreeType face acikken bu bellegi
 *             ELLEMEYIN/free etmeyin -- stb_truetype ile ayni kural).
 * buffer_size: buffer uzunlugu (byte).
 *
 * Basarili olursa >= 0 bir "font handle" (face index) doner, hata
 * durumunda -1 doner. Ayni fontu (ayni buffer) tekrar tekrar
 * yuklemeyin; bir kere yukleyip handle'i saklayin (aynen stb_truetype
 * kullaniminda oldugu gibi tek font_size/glyph cache mantigi burada
 * da gecerli: her (font, boyut, karakter) kombinasyonu ilk cizimde
 * rasterize edilip Newlib heap'inde onbelleklenir).
 */
int kayaos_ft_load_font_from_memory( const uint8_t *ttf_buffer, size_t buffer_size );

/*
 * graphics_draw_ttf_text() ile birebir ayni imza + davranis:
 *   - (x,y): metnin sol-ust kosesi (baseline degil!)
 *   - foreground/background: graphics_rgb() ile paketlenmis 0xRRGGBB
 *   - font_size: piksel cinsinden yaklasik glif yuksekligi
 *
 * font_handle: kayaos_ft_load_font_from_memory()'den donen deger.
 */
void kayaos_ft_draw_text( int font_handle,
                           const char *text,
                           int x, int y,
                           uint32_t foreground, uint32_t background,
                           float font_size );

/* Onbellek/bellek durumunu terminal'e yazdirmak icin (mem komutuna
 * benzer, teshis amacli). */
void kayaos_ft_debug_stats( void );

#endif /* KAYAOS_FREETYPE_H */
