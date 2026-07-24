#ifndef AHCI_HW_H
#define AHCI_HW_H

#include <stdint.h>

/* PCI Konfigürasyon Sabitleri */
#define AHCI_PCI_CLASS_STORAGE    0x01
#define AHCI_PCI_SUBCLASS_SATA    0x06
#define AHCI_PCI_PROG_IF_AHCI     0x01

/* Port Implemented Mask & Limits */
#define AHCI_MAX_PORTS            32
#define AHCI_MAX_CMD_SLOTS        32

/* Global Host Control Registers (GHC) Offsetleri */
#define AHCI_REG_CAP              0x00 /* Host Capabilities */
#define AHCI_REG_GHC              0x04 /* Global Host Control */
#define AHCI_REG_IS               0x08 /* Interrupt Status */
#define AHCI_REG_PI               0x0C /* Ports Implemented */
#define AHCI_REG_VS               0x10 /* Version */

/* GHC Bits */
#define AHCI_GHC_HR               (1U << 0)  /* HBA Reset */
#define AHCI_GHC_IE               (1U << 1)  /* Interrupt Enable */
#define AHCI_GHC_AE               (1U << 31) /* AHCI Enable */

/* Port Register Bits (PxCMD) */
#define AHCI_PxCMD_ST             (1U << 0)  /* Start Engine */
#define AHCI_PxCMD_SUD            (1U << 1)  /* Spin-Up Device */
#define AHCI_PxCMD_POD            (1U << 2)  /* Power On Device */
#define AHCI_PxCMD_FRE            (1U << 4)  /* FIS Receive Enable */
#define AHCI_PxCMD_FR             (1U << 14) /* FIS Receive Running */
#define AHCI_PxCMD_CR             (1U << 15) /* Command List Running */

/* Port Status & Signature Values */
#define AHCI_PORT_SSTS_DET_MASK   0x0F
#define AHCI_PORT_SSTS_DET_PRESENT 0x03
#define AHCI_PORT_SSTS_IPM_ACTIVE  0x01

#define AHCI_SIG_SATA             0x00000101U /* SATA Drive */
#define AHCI_SIG_SATAPI           0xEB140101U /* SATAPI Drive */
#define AHCI_SIG_SEMB             0xC33C0101U /* Enclosure Management Bridge */
#define AHCI_SIG_PM               0x96690101U /* Port Multiplier */

/* ATA Command Codes */
#define ATA_CMD_READ_DMA_EXT      0x25
#define ATA_CMD_WRITE_DMA_EXT     0x35
#define ATA_CMD_IDENTIFY          0xEC
#define ATA_CMD_IDENTIFY_PACKET   0xA1

/* FIS Types */
#define FIS_TYPE_REG_H2D          0x27 /* Register FIS - Host to Device */
#define FIS_TYPE_REG_D2H          0x34 /* Register FIS - Device to Host */
#define FIS_TYPE_DMA_ACT          0x39 /* DMA Activate FIS - Device to Host */
#define FIS_TYPE_DMA_SETUP        0x41 /* DMA Setup FIS - Bidirectional */
#define FIS_TYPE_DATA             0x46 /* Data FIS - Bidirectional */
#define FIS_TYPE_BIST             0x58 /* BIST Activate FIS - Bidirectional */
#define FIS_TYPE_PIO_SETUP        0x5F /* PIO Setup FIS - Device to Host */
#define FIS_TYPE_DEV_BITS         0xA1 /* Set Device Bits FIS - Device to Host */

#endif /* AHCI_HW_H */