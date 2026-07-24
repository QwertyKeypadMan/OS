# FreeType -> KayaOS Portu

Bu paket, gercek FreeType 2.13.3 kaynagindan (resmi `freetype/freetype` deposu,
tag `VER-2-13-3`) turetilmis, **KayaOS icin minimal, freestanding bir
TrueType + smooth-AA rasterizer** derlemesidir. Halisünasyon degil: kaynak
kod `git clone` ile cekildi, sadece gereksiz modul/ozellikler cikarildi ve
KayaOS'a ozel 3 "yapistirici" dosya eklendi.

## Neden bu kadar kucuk bir alt kume?

Tam FreeType; CFF/Type1/PCF/BDF/WOFF2/SVG/renkli-emoji gibi bircok surucu
icerir. Sizin ihtiyaciniz (GUI/terminal metni icin TTF render) icin sadece
3 "single object" derleme birimi yeterli:

| Modul                          | Ne ise yarar                                   |
|---------------------------------|-------------------------------------------------|
| `src/sfnt/sfnt.c`               | TTF/OTF konteyner format cozucu (tablolar, cmap) |
| `src/truetype/truetype.c`       | TrueType glyph yukleyici/surucu                  |
| `src/smooth/smooth.c`           | Anti-aliased (gri tonlamali) tarayici (ftgrays)  |

Artı `src/base/*` (nesne yonetimi, akis, hata tablolari — hepsi
`ftbase.c` icine `#include` edilen tek parca).

## Devre disi biraktigim ozellikler (ftoption_kayaos.h icinde)

- `FT_CONFIG_OPTION_USE_ZLIB` — WOFF sikistirma; zlib portlanmadi.
- `FT_CONFIG_OPTION_MAC_FONTS` — Klasik Mac resource fork destegi.
- `FT_CONFIG_OPTION_SVG` — OT-SVG renderer (harici SVG kutuphanesi ister).
- `FT_CONFIG_OPTION_ENVIRONMENT_PROPERTIES` — `getenv()` kullanir.
- `TT_CONFIG_OPTION_BYTECODE_INTERPRETER` — TrueType "hinting" sanal
  makinesi. **v1'de kapali**: `setjmp/longjmp` tabanli istisna mekanizmasi
  kullanir ve ilk portta gereksiz risk/karmasiklik katar. Kapaliyken
  FreeType otomatik olarak unhinted (hint'siz) glyph outline'lari uretir
  ve `smooth` rasterizer bunlari gri-tonlamali (anti-aliased) tarar --
  yani font kalitesi, mevcut `graphics_draw_ttf_text()` (stb_truetype,
  o da hint'siz) ile ASAGI YUKARI AYNI olur, sadece FreeType'in çok daha
  saglam/eksiksiz TTF ayristiricisini (kerning, gercek cmap formatlari,
  bilesik glifler, degisken font destegi vb.) kazanmis olursunuz.
  Ileride isterseniz bu macroyu acip `ttinterp.c`'nin `setjmp` gereksinimini
  (Newlib zaten saglar, i386 icin) test edip hinting'i devreye alabilirsiniz.

**FreeType, tasarimi geregi float/double KULLANMAZ** (26.6 / 16.16 sabit
nokta aritmetigi, `fttrigon.c` icinde CORDIC tabanli integer sin/cos) --
bu yuzden "kayan nokta donanimi yok" kisitiniz sorun degil.

## Bellek stratejisi -- ONEMLI

`kernel_heap`'iniz (`kstring.c`) bir **arena allocator**: `k_free()` no-op.
FreeType bunun aksine surekli alloc+free yapar (glyph outline'lari, hint
context'leri...). Bu yuzden:

- **FreeType'in kendi ic tahsisleri** -> `ftsystem_kayaos.c` icinde
  **Newlib `malloc/realloc/free`**'e baglandi (talimatiniza uygun: "Heap
  icin Newlib malloc kullanilabilir").
- **Sizin render edip sakladiginiz glyph bitmap onbellegi**
  (`kayaos_freetype.c` icindeki `kft_glyph_t.coverage`) -> kalici oldugu
  icin bilerek `k_malloc()` (arena) kullaniyor; bu tamamen mevcut
  `graphics_draw_ttf_text()`'teki 128 girisli ASCII cache deseniyle ayni
  mantik (tek seferlik allocate, hic free edilmez).

Eger Newlib malloc'unuz gercekten calisan bir `sbrk()`/heap alanina sahip
degilse (yani sadece "linklendi ama arkasinda gercek bellek yok"),
`ftsystem_kayaos.c`'yi `k_malloc`'a yonlendirmeniz gerekir -- ama o zaman
arena tukenmesi riskini goze almis olursunuz; test ederken `mem` benzeri
bir Newlib-heap-kullanimi komutu eklemenizi oneririm.

## Dosya haritasi

```
vendor/include/          -> FreeType public + internal header'lari (degistirilmedi)
vendor/src/base/         -> ftbase.c, ftinit.c, ftbbox.c, ftglyph.c (+ dahili include'lar)
                             ftsystem.c.reference ve ftdebug.c.reference SADECE REFERANS,
                             DERLEMEYE DAHIL ETMEYIN (yerlerini kayaos/*.c aliyor)
vendor/src/truetype/     -> truetype.c (single-object, ttdriver/ttgload/ttinterp/... icerir)
vendor/src/sfnt/         -> sfnt.c (single-object)
vendor/src/smooth/       -> smooth.c (single-object, ftgrays.c + ftsmooth.c icerir)

kayaos/ftoption_kayaos.h -> ozellik anahtarlari (yukarida aciklandi)
kayaos/kayaos_ftmodule.h -> sadece 3 modul kayitli (tt+sfnt+smooth)
kayaos/ftsystem_kayaos.c -> Newlib malloc/realloc/free koprusu (ftsystem.c yerine)
kayaos/ftdebug_kayaos.c  -> FT_Message/FT_Panic stub'lari (ftdebug.c yerine)
kayaos/kayaos_freetype.h -> sizin cagiracaginiz public API
kayaos/kayaos_freetype.c -> framebuffer'a glyph cizimi + kalici glyph cache
```

## Derleme (Makefile'iniza eklenecek parca)

```makefile
FT_DIR      := vendor
FT_KAYAOS   := kayaos

FT_CFLAGS   := -I$(FT_DIR)/include \
               -DFT2_BUILD_LIBRARY \
               -DFT_CONFIG_OPTIONS_H="\"$(FT_KAYAOS)/ftoption_kayaos.h\"" \
               -DFT_CONFIG_MODULES_H="\"$(FT_KAYAOS)/kayaos_ftmodule.h\""

FT_SOURCES  := $(FT_DIR)/src/base/ftbase.c \
               $(FT_DIR)/src/base/ftinit.c \
               $(FT_DIR)/src/base/ftbbox.c \
               $(FT_DIR)/src/base/ftglyph.c \
               $(FT_DIR)/src/truetype/truetype.c \
               $(FT_DIR)/src/sfnt/sfnt.c \
               $(FT_DIR)/src/smooth/smooth.c \
               $(FT_KAYAOS)/ftsystem_kayaos.c \
               $(FT_KAYAOS)/ftdebug_kayaos.c \
               $(FT_KAYAOS)/kayaos_freetype.c

# Mevcut CFLAGS'inize (i686-elf-gcc, -ffreestanding vb.) FT_CFLAGS'i ekleyin:
CFLAGS += $(FT_CFLAGS)

# FT_SOURCES'i mevcut C_SOURCES / OBJS listenize dahil edin.
```

`-DFT_CONFIG_OPTIONS_H="\"...\""` ve `-DFT_CONFIG_MODULES_H="\"...\""`
kullanimina dikkat: FreeType bu makrolari `#include FT_CONFIG_OPTIONS_H`
seklinde kullaniyor, yani deger TIRNAKLI bir dosya yolu olmali (yukaridaki
kacis karakterleri Makefile icin dogru).

**Onemli:** `vendor/include` yolunuzu include path'inize eklerken, kendi
`kernel/*.h` başlıklarınızla isim çakışması olmadığından emin olun
(FreeType kendi ic başlıklarını `freetype/internal/...` altında tutar,
çakışma ihtimali düşük).

## Kullanim (gui.c / windowmng.c / terminal.c icinde)

```c
#include "kayaos_freetype.h"

/* Bir kere, acilista (gui_init benzeri bir yerde): */
kayaos_ft_init();
int ui_font = kayaos_ft_load_font_from_memory(asset->data, asset->size);
/* asset, mevcut assets.c/asset_find() ile ayni kaynaktan -- stb_truetype'a
 * ne veriyorsaniz onu verin. */

/* Her cizimde (mevcut graphics_draw_ttf_text cagrinizin birebir yerine): */
kayaos_ft_draw_text(ui_font, "Merhaba KayaOS", 100, 100,
                     graphics_rgb(255,255,255), 0x1A1A24, 18.0f);
```

## Test etmediklerim / sizin dogrulamaniz gerekenler

Bu ortamda sizin capa capraz-derleyicinizle (i686-elf-gcc + ozel
linker script + ozel Newlib config) gercek bir derleme deneyemedim --
sadece FreeType'in resmi kaynagini inceleyip minimal-modul secimini ve
sembol bagimliliklarini (grep ile) dogruladim. Ilk derlemede olasi
sorunlar:

1. **`<stdlib.h>`/`<string.h>`/`<stdarg.h>`/`<limits.h>` gercekten
   Newlib'den freestanding olarak geliyor mu?** `ftstdlib.h` bunlara
   guveniyor. Sizin Newlib portunuz bunlari sagliyorsa sorun yok.
2. **`ft2build.h` include path'i.** `-I$(FT_DIR)/include` sart.
3. **`FT_UInt`/`FT_Long` boyutlari.** `ftconfig.h` `<stdint.h>` tabanli,
   32-bit x86'da sorunsuz olmali.
4. Font boyutu buyukse (ozellikle CJK degil ama genis Latin+sembol
   font'lari) `sfnt.c` tablo ayristirma asamasinda Newlib heap'inizin
   yeterli buyuklukte olmasi gerekir -- kucuk bir test fontuyla
   (ornegin mevcut projede zaten gomulu olan TTF) baslamanizi oneririm.
5. `kayaos_freetype.c` icindeki `k_malloc`/`graphics_*` extern
   bildirimlerini, isterseniz doğrudan `#include "kernel/graphics.h"`
   ve `#include "kernel/kstring.h"` ile degistirebilirsiniz (ben bilerek
   header bagimliligini azaltmak icin extern birakti).

## Sonraki adimlar (istege bagli)

- `TT_CONFIG_OPTION_BYTECODE_INTERPRETER`'i acip gercek TT hinting'i
  denemek (daha keskin kucuk-boyut metin, ama `setjmp` + VM karmasikligi
  gelir).
- `FT_Outline_Decompose` ile glyph konturlarini dogrudan kendi
  `graphics_draw_rounded_rect`/vektor primitifleriniz uzerinden cizip
  smooth.c'ye hic ihtiyac duymamak (daha fazla is, ama daha az kod).
- Kerning icin `FT_Get_Kerning()` -- `graphics_draw_ttf_text`'teki
  `stbtt_GetCodepointKernAdvance` karsiligi, `kayaos_ft_draw_text`
  icine kolayca eklenebilir.
