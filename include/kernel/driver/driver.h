#ifndef KAYAOS_DRIVER_H
#define KAYAOS_DRIVER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* Herhangi bir PCI alanını es geçmek için joker (wildcard) değerler */
#define PCI_ANY_ID    0xFFFF
#define PCI_ANY_CLASS 0xFF

/* İleride genişletilebilir cihaz durumu */
typedef enum {
    DRIVER_STATE_UNINITIALIZED = 0,
    DRIVER_STATE_READY,
    DRIVER_STATE_SUSPENDED,
    DRIVER_STATE_FAILED
} driver_state_t;

/* Forward declaration */
struct driver;

/* KayaOS PCI Cihaz Yapısı (Mevcut PCI sisteminizdeki yapıya adapte edebilirsiniz) */
typedef struct pci_device {
    uint8_t  bus;
    uint8_t  slot;
    uint8_t  func;
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t  class_code;
    uint8_t  subclass;
    uint8_t  prog_if;
    uint8_t  revision;
    
    struct driver* driver;   /* Bağlanan sürücü (eşleştiyse) */
    void*          priv_data;/* Driver'ın cihaza özel tutacağı veri */
} pci_device_t;

/* KayaOS Sürücü Tanım Yapısı */
typedef struct driver {
    const char* name;
    const char* version;
    const char* author;

    /* Donanım Eşleşme Bilgileri */
    uint16_t supported_vendor_id;  /* PCI_ANY_ID ise fark etmez */
    uint16_t supported_device_id;  /* PCI_ANY_ID ise fark etmez */
    uint8_t  supported_class_code; /* PCI_ANY_CLASS ise fark etmez */
    uint8_t  supported_subclass;   /* PCI_ANY_CLASS ise fark etmez */

    /* Eşleşme Önceliği (Sayı büyüdükçe öncelik artar) */
    uint32_t priority;

    /* Sürücü Yaşam Döngüsü Fonksiyon İşaretçileri */
    int (*probe)(pci_device_t* dev);
    int (*init)(pci_device_t* dev);
    int (*shutdown)(pci_device_t* dev);
    int (*remove)(pci_device_t* dev);
    
    /* Opsiyonel Güç Yönetimi */
    int (*suspend)(pci_device_t* dev);
    int (*resume)(pci_device_t* dev);

    /* Bağlı Liste İşaretçisi */
    struct driver* next;
} driver_t;

#endif /* KAYAOS_DRIVER_H */