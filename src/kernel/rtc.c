#include <stdint.h>
#include "kernel/io.h"
#include "kernel/rtc.h"

#define CMOS_ADDRESS 0x70
#define CMOS_DATA    0x71

/* RTC Register Indexleri */
#define RTC_SEC  0x00
#define RTC_MIN  0x02
#define RTC_HOUR 0x04
#define RTC_DAY  0x07
#define RTC_MON  0x08
#define RTC_YEAR 0x09
#define RTC_STATUS_A 0x0A
#define RTC_STATUS_B 0x0B


/* BCD'den Binary'ye donusturucu (Profesyonel kernel'larda olmazsa olmaz) */
static inline uint8_t bcd_to_binary(uint8_t bcd) {
    return (bcd & 0x0F) + ((bcd >> 4) * 10);
}

/* Update devam ediyor mu kontrolu */
int rtc_is_updating(void) {
    outb(CMOS_ADDRESS, RTC_STATUS_A);
    return (inb(CMOS_DATA) & 0x80);
}

void rtc_get_time(rtc_time_t *time) {
    /* Donanimin veriyi guncellemesini bekle */
    while (rtc_is_updating());

    outb(CMOS_ADDRESS, RTC_SEC);  time->second = bcd_to_binary(inb(CMOS_DATA));
    outb(CMOS_ADDRESS, RTC_MIN);  time->minute = bcd_to_binary(inb(CMOS_DATA));
    outb(CMOS_ADDRESS, RTC_HOUR); time->hour   = bcd_to_binary(inb(CMOS_DATA));
    outb(CMOS_ADDRESS, RTC_DAY);  time->day    = bcd_to_binary(inb(CMOS_DATA));
    outb(CMOS_ADDRESS, RTC_MON);  time->month  = bcd_to_binary(inb(CMOS_DATA));
    outb(CMOS_ADDRESS, RTC_YEAR); time->year   = bcd_to_binary(inb(CMOS_DATA));
    
    /* 
     * Not: Yil genellikle 2 basamaklidir (orn: 26). 
     * Gercek bir OS icin burada 'Century' register'ini 
     * veya basit bir 'if (year < 70) year += 2000;' mantigini kurman sart.
     */
}