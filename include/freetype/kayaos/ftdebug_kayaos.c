/*
 * ftdebug_kayaos.c
 * -----------------
 * FreeType'in "src/base/ftdebug.c" dosyasinin yerini alan minimal stub.
 *
 * ftoption_kayaos.h icinde FT_DEBUG_LEVEL_ERROR ve FT_DEBUG_LEVEL_TRACE
 * KAPALI oldugu icin FT_TRACEx()/FT_ERROR() makrolari derleme sirasinda
 * hicbir koda genislemez ve pratikte hicbir yerden cagrilmazlar. Yine de
 * FT_Message/FT_Panic sembolleri bazi internal header'lardan (ftdebug.h)
 * extern olarak bildirildigi icin, linker guvenligi acisindan burada
 * gercek (ama kernel-dostu) bir govde birakiyoruz.
 *
 * terminal_writestring() zaten kstring.c/terminal.c'de var; burada
 * dogrudan onu kullanmiyoruz ki bu dosya terminal.h'a bagimli olmasin
 * (FreeType katmani, kernel'in geri kalanindan olabildigince izole
 * kalsin). Bunun yerine kucuk bir extern hook birakiyoruz -- isterseniz
 * kayaos_freetype.c icinde bunu terminal_writestring'e baglayabilirsiniz.
 */

#include <ft2build.h>
#include FT_CONFIG_CONFIG_H
#include <freetype/internal/ftdebug.h>   /* FT_BASE_DEF makrosu icin */
#include <stdarg.h>

/* main.c / terminal.c tarafinda tanimlayin (opsiyonel):
 *   void kayaos_ft_log(const char *msg) { terminal_writestring(msg); }
 * Tanimlamazsaniz asagidaki zayif (weak) fallback devreye girer ve
 * mesaji sessizce yutar (kernel çökmez). */
__attribute__((weak)) void
kayaos_ft_log( const char *msg )
{
    (void)msg;
}

void
FT_Message( const char*  fmt, ... )
{
    /* Freestanding ortamda vsnprintf pahali/riskli olabilir; sabit
     * kucuk bir tampon yeterli -- bu sadece teshis amacli, kritik yol
     * degil (TT_CONFIG_OPTION_BYTECODE_INTERPRETER kapali oldugu
     * surece hic tetiklenmez). */
    (void)fmt;
    kayaos_ft_log( "[FreeType] mesaj (detay icin FT_DEBUG_LEVEL_ERROR acin)\n" );
}

void
FT_Panic( const char*  fmt, ... )
{
    (void)fmt;
    kayaos_ft_log( "[FreeType] PANIC!\n" );
    /* abort() Newlib'de mevcut (kernel.c zaten abort() tanimliyor ve
     * cli;hlt dongusune giriyor) -- oraya dusuyoruz. */
    extern void abort( void );
    abort();
}

/*
 * FT_Trace_Enable / FT_Trace_Disable
 * -----------------------------------
 * smooth.c icine #include ile giren ftgrays.c, STANDALONE_ olmayan (yani
 * bizim tam-kutuphane build'imizdeki) dalda bu iki fonksiyonu GERCEK
 * sembol olarak cagiriyor (gray_convert_glyph_inner icinde, rasterize
 * sirasinda trace ciktisini gecici bastırmak icin -- STANDALONE_ modunda
 * bunlar no-op MAKRO'ya donusuyor ama biz standalone degiliz, o yuzden
 * gercek fonksiyon govdesi gerekiyor).
 *
 * FT_DEBUG_LEVEL_TRACE kapali oldugu icin (ftoption_kayaos.h) orijinal
 * ftdebug.c'nin bu durumda kullandigi govdeyle birebir ayni: bos govde.
 */

FT_BASE_DEF( void )
FT_Trace_Disable( void )
{
    /* nothing -- FT_DEBUG_LEVEL_TRACE kapali */
}

FT_BASE_DEF( void )
FT_Trace_Enable( void )
{
    /* nothing -- FT_DEBUG_LEVEL_TRACE kapali */
}