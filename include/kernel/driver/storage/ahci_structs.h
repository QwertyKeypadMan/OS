#ifndef AHCI_STRUCTS_H
#define AHCI_STRUCTS_H

#include <stdint.h>
#include "ahci_hw.h"

#pragma pack(push, 1)

/* Port Memory Register Map */
typedef struct {
    uint32_t clb;       /* Command List Base Address (1 KB aligned) */
    uint32_t clbu;      /* Command List Base Address Upper 32-bits */
    uint32_t fb;        /* FIS Base Address (256 B aligned) */
    uint32_t fbu;       /* FIS Base Address Upper 32-bits */
    uint32_t is;        /* Interrupt Status */
    uint32_t ie;        /* Interrupt Enable */
    uint32_t cmd;       /* Command and Status */
    uint32_t rsv0;      /* Reserved */
    uint32_t tfd;       /* Task File Data */
    uint32_t sig;       /* Signature */
    uint32_t ssts;      /* SATA Status (SCR0: SStatus) */
    uint32_t sctl;      /* SATA Control (SCR2: SControl) */
    uint32_t serr;      /* SATA Error (SCR1: SError) */
    uint32_t sact;      /* SATA Active (SCR3: SActive) */
    uint32_t ci;        /* Command Issue */
    uint32_t sntf;      /* SATA Notification (SCR4: SNotification) */
    uint32_t fbs;       /* FIS-based Switching Control */
    uint32_t rsv1[11];  /* Reserved */
    uint32_t vendor[4]; /* Vendor Specific */
} hba_port_t;

/* Memory Register Map (ABAR) */
typedef struct {
    uint32_t cap;       /* 0x00: Host Capability */
    uint32_t ghc;       /* 0x04: Global Host Control */
    uint32_t is;        /* 0x08: Interrupt Status */
    uint32_t pi;        /* 0x0C: Ports Implemented */
    uint32_t vs;        /* 0x10: Version */
    uint32_t ccc_ctl;   /* 0x14: Command Completion Coalescing Control */
    uint32_t ccc_pts;   /* 0x18: Command Completion Coalescing Ports */
    uint32_t em_loc;    /* 0x1C: Enclosure Management Location */
    uint32_t em_ctl;    /* 0x20: Enclosure Management Control */
    uint32_t cap2;      /* 0x24: Host Capabilities Extended */
    uint32_t bohc;      /* 0x28: BIOS/OS Handoff Control and Status */
    uint8_t  rsv[0xA0 - 0x2C];
    uint8_t  vendor[0x100 - 0xA0];
    hba_port_t ports[32]; /* 0x100 - 0x1100: Port Control Registers */
} hba_mem_t;

/* Register FIS - Host to Device */
typedef struct {
    uint8_t  fis_type;    /* FIS_TYPE_REG_H2D */
    uint8_t  pmport : 4;  /* Port multiplier */
    uint8_t  rsv0 : 3;    /* Reserved */
    uint8_t  c : 1;       /* 1: Command, 0: Control */
    uint8_t  command;     /* ATA Command code */
    uint8_t  featurelow;  /* Feature register low */
    uint8_t  lba0;        /* LBA7..0 */
    uint8_t  lba1;        /* LBA15..8 */
    uint8_t  lba2;        /* LBA23..16 */
    uint8_t  device;      /* Device register */
    uint8_t  lba3;        /* LBA31..24 */
    uint8_t  lba4;        /* LBA39..32 */
    uint8_t  lba5;        /* LBA47..40 */
    uint8_t  featurehigh; /* Feature register high */
    uint8_t  countlow;    /* Sector count low */
    uint8_t  counthigh;   /* Sector count high */
    uint8_t  icc;         /* Isochronous command completion */
    uint8_t  control;     /* Control register */
    uint8_t  rsv1[4];     /* Reserved */
} fis_reg_h2d_t;

/* Physical Region Descriptor Table (PRDT) Entry */
typedef struct {
    uint32_t dba;  /* Data Base Address */
    uint32_t dbau; /* Data Base Address Upper 32-bits */
    uint32_t rsv0; /* Reserved */
    uint32_t dbc : 22; /* Data Byte Count (0-indexed, e.g., 511 for 512 bytes) */
    uint32_t rsv1 : 9; /* Reserved */
    uint32_t i : 1;    /* Interrupt on completion */
} hba_prdt_entry_t;

/* Command Table Structure */
typedef struct {
    uint8_t  cfis[64]; /* Command FIS */
    uint8_t  acmd[16]; /* ATAPI Command, 12 or 16 bytes */
    uint8_t  rsv[48];  /* Reserved */
    hba_prdt_entry_t prdt_entry[1]; /* Physical Region Descriptor Table Entries */
} hba_cmd_tbl_t;

/* Command Header Structure */
typedef struct {
    uint8_t  cfl : 5;  /* Command FIS length in uint32_t units */
    uint8_t  a : 1;    /* ATAPI */
    uint8_t  w : 1;    /* Write (1: Host-to-device, 0: Device-to-host) */
    uint8_t  p : 1;    /* Prefetchable */
    uint8_t  r : 1;    /* Reset */
    uint8_t  b : 1;    /* BIST */
    uint8_t  c : 1;    /* Clear busy upon R_OK */
    uint8_t  rsv0 : 1; /* Reserved */
    uint8_t  pmp : 4;  /* Port multiplier port */
    uint16_t prdtl;    /* Physical Region Descriptor Table length in entries */
    uint32_t prdbc;    /* Physical Region Descriptor Byte Count */
    uint32_t ctba;     /* Command Table Base Address */
    uint32_t ctbau;    /* Command Table Base Address Upper 32-bits */
    uint32_t rsv1[4];  /* Reserved */
} hba_cmd_header_t;

/* Received FIS Area Structure */
typedef struct {
    uint8_t dsfis[0x1C]; /* DMA Setup FIS */
    uint8_t rsv0[0x04];
    uint8_t psfis[0x14]; /* PIO Setup FIS */
    uint8_t rsv1[0x0C];
    uint8_t rfis[0x14];  /* Register D2H FIS */
    uint8_t rsv2[0x04];
    uint8_t sdbfis[0x08];/* Set Device Bits FIS */
    uint8_t ufis[0x40];  /* Unknown FIS */
    uint8_t rsv3[0x60];
} hba_fis_t;

#pragma pack(pop)

/* Device Type Definitions */
typedef enum {
    AHCI_DEV_NULL = 0,
    AHCI_DEV_SATA = 1,
    AHCI_DEV_SATAPI = 2,
    AHCI_DEV_SEMB = 3,
    AHCI_DEV_PM = 4
} ahci_device_type_t;

/* High level Port Descriptor structure for KayaOS */
typedef struct {
    uint8_t            port_number;
    ahci_device_type_t type;
    bool               present;
    uint64_t           capacity_sectors;
    uint32_t           sector_size;
    char               model[41];
    char               serial[21];
    hba_port_t*        hba_port;
    hba_cmd_header_t*  cmd_header;
    hba_cmd_tbl_t*     cmd_tbl;
    hba_fis_t*         fis;
} ahci_port_device_t;

#endif /* AHCI_STRUCTS_H */