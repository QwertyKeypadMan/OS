/*
 * ftsystem_kayaos.c
 * -----------------
 * FreeType'in "src/base/ftsystem.c" dosyasinin yerini alan KayaOS portu.
 *
 * ONEMLI - REIS OKU:
 *   FreeType, bir face acildiginda / glif rasterize ettiginde SIK SIK
 *   alloc + free yapar (glyph outline'lari, hint context'leri, geçici
 *   tablolar...). kstring.c'deki k_malloc/k_free arena modelini
 *   KULLANMIYORUZ, cunku k_free bilerek no-op (arena allocator) ve
 *   FreeType bu modelle calisirsa 8 MB'lik kernel heap'i birkac saniye
 *   icinde tuketip cokecektir (aynen gui.c/graphics.c gecmisindeki
 *   glyph-cache olmadan yasadigimiz TTF memory leak sorunu gibi, ama
 *   bu sefer cache ile cozulemez cunku her farkli glif/boyut
 *   kombinasyonu gercekten allocate+free gerektirir).
 *
 *   Bu yuzden burada projenizin GERCEK free() destekleyen ayiricisini
 *   (Newlib malloc/realloc/free) kullaniyoruz. Kullanicinin talimati:
 *   "Heap icin Newlib malloc kullanilabilir" -- tam da bunun icin.
 *
 *   Eger ileride kendi gercek free() destekleyen bir kernel heap'iniz
 *   olursa (k_malloc'un yanina bir k_free_real() eklerseniz), asagida
 *   sadece 3 fonksiyonu (kayaos_ft_alloc/free/realloc) degistirmeniz
 *   yeterli olur.
 */

#include <ft2build.h>
#include FT_CONFIG_CONFIG_H
#include <freetype/internal/ftdebug.h>   /* FT_THROW burada tanimli */
#include <freetype/internal/ftstream.h>  /* FT_BASE_DEF/FT_Stream icin */
#include FT_SYSTEM_H
#include FT_TYPES_H
#include FT_ERRORS_H

#include <stdlib.h>   /* Newlib: malloc / realloc / free */

/* ---- Bellek ayirici koprusu (Newlib malloc ailesine) ---- */

static void *
kayaos_ft_alloc( FT_Memory  memory,
                  long       size )
{
    (void)memory;
    if ( size <= 0 )
        return NULL;
    return malloc( (size_t)size );
}

static void
kayaos_ft_free( FT_Memory  memory,
                 void*      block )
{
    (void)memory;
    if ( block != NULL )
        free( block );
}

static void *
kayaos_ft_realloc( FT_Memory  memory,
                    long       cur_size,
                    long       new_size,
                    void*      block )
{
    (void)memory;
    (void)cur_size;
    if ( new_size <= 0 )
    {
        if ( block != NULL )
            free( block );
        return NULL;
    }
    return realloc( block, (size_t)new_size );
}

static struct FT_MemoryRec_  kayaos_ft_memory =
{
    NULL,               /* user           */
    kayaos_ft_alloc,
    kayaos_ft_free,
    kayaos_ft_realloc
};

/*
 * FT_New_Memory() -- FT_Init_FreeType() bu fonksiyonu cagirir.
 * Orijinal ftsystem.c'deki davranisin ayni imzali karsiligi.
 */
FT_BASE_DEF( FT_Memory )
FT_New_Memory( void )
{
    return &kayaos_ft_memory;
}

FT_BASE_DEF( void )
FT_Done_Memory( FT_Memory  memory )
{
    (void)memory;
    /* Statik/global memory nesnesi; serbest birakilacak bir sey yok. */
}

/*
 * Dosya tabanli stream acma (FT_New_Face / FT_Stream_Open).
 *
 * KayaOS'ta font dosyalari RAMFS uzerinden BYTE BUFFER olarak zaten
 * bellekte (assets.c / ramfs.c). Bu yuzden bu portta font yuklemesi
 * icin FT_New_Face (dosya yolu) DEGIL, FT_New_Memory_Face (hazir
 * buffer) kullanilmasi bekleniyor -- tipki mevcut
 * graphics_draw_ttf_text()'in stb_truetype'a ttf_buffer'i dogrudan
 * vermesi gibi. FT_New_Memory_Face, FT_Stream_OpenMemory() uzerinden
 * calisir (ftobjs.c icinde), yani BU fonksiyona hic ugramaz.
 *
 * Yine de linker FT_Stream_Open sembolunu (bazi ic bagimlilar
 * yuzunden) arayabilir; guvenlik icin burada "desteklenmiyor" hatasi
 * donen bir stub birakiyoruz. Eger ileride RAMFS/VFS uzerinden
 * "diskten" font akitmak isterseniz, ramfs_get()/ramfs bulunan
 * dosyanin data/size alanlarini kullanarak burayi gercek bir
 * implementasyona cevirebilirsiniz.
 */
FT_BASE_DEF( FT_Error )
FT_Stream_Open( FT_Stream    stream,
                 const char*  filepathname )
{
    (void)stream;
    (void)filepathname;
    return FT_THROW( Unimplemented_Feature );
}