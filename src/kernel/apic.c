#include <stdint.h>
#include "kernel/io.h"

/* LAPIC Register Offsetleri */
#define LAPIC_LVT_TIMER      0x320
#define LAPIC_TIMER_ICR      0x380
#define LAPIC_TIMER_CCR      0x390
#define LAPIC_TIMER_DIV      0x3E0

/* LVT Timer Modları */
#define LVT_TIMER_MASKED     0x10000    /* Kesmeleri sustur (Sadece sayac olarak kullan) */
#define LVT_TIMER_PERIODIC   0x20000    /* Surekli mod */
#define TIMER_INTERRUPT_VEC  0x20       /* Kesme vektoru (Ornegin 32) */

#define DIVIDER_16           0x03


/* Her yere erisebilmesi icin global kalibrasyon degiskenimiz */
uint32_t apic_ticks_per_ms = 0;

/* apic.c dosyasının en üstüne ekle */

static inline void lapic_write(uint32_t offset, uint32_t value) {
    *((volatile uint32_t *)(0xFEE00000 + offset)) = value;
}

static inline uint32_t lapic_read(uint32_t offset) {
    return *((volatile uint32_t *)(0xFEE00000 + offset));
}

/* PIT Kanal 2'yi kullanarak IRQ tetiklemeden tam 10ms bekleten fonksiyon */
static void pit_wait_10ms(void) {
    /* 
     * PIT'in temel frekansi: 1193180 Hz. 
     * 10 milisaniye beklemek icin: 1193180 / 100 = 11931 (0x2E9B) tick lazim.
     */
    uint16_t count = 11931;

    /* Port 0x61: Hoparlor ve PIT Kanal 2 kontrol portu */
    uint8_t port61 = inb(0x61);
    
    /* Hoparloru kapali (Bit 1 = 0), Timer Gate'i acik (Bit 0 = 1) yapiyoruz */
    outb(0x61, (port61 & 0xFD) | 0x01);

    /* PIT Komutu: Kanal 2, LSB/MSB erisimi, Mod 0 (Geri sayim bitince sinyal ver) */
    outb(0x43, 0xB0);
    
    /* Geri sayim degerini (11931) yolla: Once alt 8 bit, sonra ust 8 bit */
    outb(0x42, (uint8_t)(count & 0xFF));
    outb(0x42, (uint8_t)((count >> 8) & 0xFF));

    /* 
     * Mod 0'da geri sayim bittiginde, Port 0x61'in 5. Biti (0x20) 1 olur. 
     * O bit 1 olana kadar islemciyi dongude bekletiyoruz (Polling).
     */
    while ((inb(0x61) & 0x20) == 0) {
        __asm__ __volatile__("pause");
    }
    
    /* Isimiz bitti, Port 0x61'i eski haline dondur (Temizlik) */
    outb(0x61, port61);
}

/* APIC Timer'i kalibre edip baslatan Ana Fonksiyon */
void apic_timer_init(void) {
    /* 1. Timer hizini 16'ya bol */
    lapic_write(LAPIC_TIMER_DIV, DIVIDER_16);

    /* 2. Timer'i gecici olarak "Maskeli (Kesmesiz)" moda al. Sadece sayac lazim. */
    lapic_write(LAPIC_LVT_TIMER, LVT_TIMER_MASKED);

    /* 3. Geri sayimi maksimum 32-bit sayidan (0xFFFFFFFF) baslat */
    lapic_write(LAPIC_TIMER_ICR, 0xFFFFFFFF);

    /* 4. PIT yardimiyla tam 10 milisaniye bekle */
    pit_wait_10ms();

    /* 5. 10 milisaniye doldu. Hemen APIC'in anlik degerini oku */
    uint32_t current_count = lapic_read(LAPIC_TIMER_CCR);

    /* 6. APIC Timer'i durdur */
    lapic_write(LAPIC_TIMER_ICR, 0);

    /* 
     * 7. Matematik zamani! Kac tick eksilmis? 
     * Maksimum degerden, okudugumuz degeri cikariyoruz.
     */
    uint32_t ticks_in_10ms = 0xFFFFFFFF - current_count;

    /* 10 milisaniyedeki degeri 10'a bolerek 1 milisaniyelik gercek kalibrasyon degerimizi buluyoruz */
    apic_ticks_per_ms = ticks_in_10ms / 10;

    /* 
     * HARIKA! Artik donanimimizin gercek hizini biliyoruz. 
     * 8. Simdi Timer'i uyanik ve Periodic (Surekli) moda alip 1ms'lik hiziyla atesliyoruz! 
     */
    lapic_write(LAPIC_LVT_TIMER, LVT_TIMER_PERIODIC | TIMER_INTERRUPT_VEC);
    lapic_write(LAPIC_TIMER_ICR, apic_ticks_per_ms);
}