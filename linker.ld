ENTRY(_start) /* Eğer çalışmazsa burayı 'start' veya 'boot' yaparız */

SECTIONS
{
    /* Çekirdeği tam 1 MB (0x100000) adresine yüklüyoruz */
    . = 1M;

    /* Kod bölümü - Multiboot imzası en başta olmak ZORUNDA */
    .text : ALIGN(4K)
    {
        *(.multiboot)
        *(.multiboot_header) /* Ne olur ne olmaz, iki ismi de yakalayalım */
        *(.text)
    }

    /* Salt okunur veriler (Stringler, sabitler) */
    .rodata : ALIGN(4K)
    {
        *(.rodata*)
    }

    /* Düzenlenebilir veriler (Global değişkenler) */
    .data : ALIGN(4K)
    {
        *(.data)
    }

    /* Başlatılmamış veriler (Stack alanı vs.) */
    .bss :
	{
		bss_start = .;
		*(COMMON)
		*(.bss)
		bss_end = .;
	}
	_end = .;
    end = .;
}