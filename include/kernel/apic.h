#ifndef KERNEL_APIC_H
#define KERNEL_APIC_H

#include <stdint.h>

/* 
 * Diğer dosyaların kullanabileceği fonksiyon prototipleri.
 * Bunlar, apic.c içinde yazdığın ana işlevlerdir.
 */

// Sistemin APIC donanımını ve Timer'ını hazırlayan ana kurulum fonksiyonu
void apic_timer_init(void);

// Eğer ileride başka çekirdekleri (core) uyandırırsan kullanacağın 
// SIVR (Spurious Interrupt) yapılandırması için tetikleyici
void lapic_init(void);

// Eski PIC'i susturmak için kmain içinde çağrılacak fonksiyon
void disable_legacy_pic(void);

/* 
 * Eğer başka dosyalarda 'timer_ticks' gibi bir sayaç tutuyorsan, 
 * 'extern' ile tanımlayarak diğer dosyaların bu değişkene 
 * erişmesini sağlayabilirsin.
 */
extern volatile uint64_t timer_ticks;

#endif /* KERNEL_APIC_H */