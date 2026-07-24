#include "kernel/driver/storage/ahci_port.h"
#include <stddef.h>

/* Byte Swapping for ATA String Identification */
static void ahci_format_ata_string(char* dest, const uint16_t* src_words, size_t word_count) {
    size_t i;
    for (i = 0; i < word_count; i++) {
        dest[i * 2]     = (char)(src_words[i] >> 8);
        dest[i * 2 + 1] = (char)(src_words[i] & 0xFF);
    }
    dest[word_count * 2] = '\0';

    /* Sondaki boşlukları temizle */
    int idx = (int)(word_count * 2) - 1;
    while (idx >= 0 && (dest[idx] == ' ' || dest[idx] == '\0')) {
        dest[idx] = '\0';
        idx--;
    }
}

ahci_device_type_t ahci_port_get_type(hba_port_t* port) {
    uint32_t ssts = port->ssts;
    uint8_t ipm = (ssts >> 8) & 0x0F;
    uint8_t det = ssts & AHCI_PORT_SSTS_DET_MASK;

    if (det != AHCI_PORT_SSTS_DET_PRESENT || ipm != AHCI_PORT_SSTS_IPM_ACTIVE) {
        return AHCI_DEV_NULL;
    }

    switch (port->sig) {
        case AHCI_SIG_SATA:   return AHCI_DEV_SATA;
        case AHCI_SIG_SATAPI: return AHCI_DEV_SATAPI;
        case AHCI_SIG_SEMB:   return AHCI_DEV_SEMB;
        case AHCI_SIG_PM:     return AHCI_DEV_PM;
        default:              return AHCI_DEV_NULL;
    }
}

void ahci_port_start(hba_port_t* port) {
    while (port->cmd & AHCI_PxCMD_CR);

    port->cmd |= AHCI_PxCMD_FRE;
    port->cmd |= AHCI_PxCMD_ST;
}

void ahci_port_stop(hba_port_t* port) {
    port->cmd &= ~AHCI_PxCMD_ST;
    port->cmd &= ~AHCI_PxCMD_FRE;

    while (1) {
        if (port->cmd & AHCI_PxCMD_FR) continue;
        if (port->cmd & AHCI_PxCMD_CR) continue;
        break;
    }
}

bool ahci_port_reset(hba_port_t* port) {
    ahci_port_stop(port);

    /* COMRESET pulse gönderimi */
    port->sctl = (port->sctl & ~0x0F) | 1;
    for (volatile int i = 0; i < 100000; i++); /* ~1ms gecikme */
    port->sctl = (port->sctl & ~0x0F);

    /* Bağlantı kurulumunu bekle */
    uint32_t timeout = 1000000;
    while (timeout--) {
        if ((port->ssts & AHCI_PORT_SSTS_DET_MASK) == AHCI_PORT_SSTS_DET_PRESENT) {
            port->serr = 0xFFFFFFFF; /* Hataları temizle */
            ahci_port_start(port);
            return true;
        }
    }
    return false;
}

void ahci_port_rebase(ahci_port_device_t* dev, uint32_t port_no, uintptr_t base_phys_addr, uintptr_t base_virt_addr) {
    ahci_port_stop(dev->hba_port);

    /* Bellek Adresleme Hesabı:
     * Command List: 1KB Aligned (32 Header * 32 Bytes = 1024 Bytes)
     * FIS Receive Area: 256B Aligned (256 Bytes)
     * Command Table: 128B Aligned (256 Bytes + PRDTs)
     */
    uintptr_t phys_clb = base_phys_addr;
    uintptr_t virt_clb = base_virt_addr;

    uintptr_t phys_fb  = phys_clb + 1024;
    uintptr_t virt_fb  = virt_clb + 1024;

    uintptr_t phys_ctb = phys_fb + 256;
    uintptr_t virt_ctb = virt_fb + 256;

    dev->hba_port->clb  = (uint32_t)(phys_clb & 0xFFFFFFFF);
    dev->hba_port->clbu = (uint32_t)((uint64_t)phys_clb >> 32);

    dev->hba_port->fb   = (uint32_t)(phys_fb & 0xFFFFFFFF);
    dev->hba_port->fbu  = (uint32_t)((uint64_t)phys_fb >> 32);

    dev->cmd_header = (hba_cmd_header_t*)virt_clb;
    dev->fis        = (hba_fis_t*)virt_fb;

    for (int i = 0; i < AHCI_MAX_CMD_SLOTS; i++) {
        dev->cmd_header[i].prdtl = 8; /* Slot başına 8 PRDT kaydı */
        uintptr_t slot_ctb_phys = phys_ctb + (i * 256);
        uintptr_t slot_ctb_virt = virt_ctb + (i * 256);

        dev->cmd_header[i].ctba  = (uint32_t)(slot_ctb_phys & 0xFFFFFFFF);
        dev->cmd_header[i].ctbau = (uint32_t)((uint64_t)slot_ctb_phys >> 32);
    }

    dev->cmd_tbl = (hba_cmd_tbl_t*)virt_ctb;

    dev->hba_port->serr = 0xFFFFFFFF;
    ahci_port_start(dev->hba_port);
}

static int ahci_find_cmdslot(hba_port_t* port) {
    uint32_t slots = (port->sact | port->ci);
    for (int i = 0; i < AHCI_MAX_CMD_SLOTS; i++) {
        if ((slots & (1U << i)) == 0) return i;
    }
    return -1;
}

bool ahci_port_identify(ahci_port_device_t* dev) {
    hba_port_t* port = dev->hba_port;
    port->is = 0xFFFFFFFF;

    int slot = ahci_find_cmdslot(port);
    if (slot == -1) return false;

    hba_cmd_header_t* cmdhdr = &dev->cmd_header[slot];
    cmdhdr->cfl = sizeof(fis_reg_h2d_t) / sizeof(uint32_t);
    cmdhdr->w = 0; /* Read */
    cmdhdr->prdtl = 1;

    hba_cmd_tbl_t* cmdtbl = (hba_cmd_tbl_t*)((uintptr_t)dev->cmd_tbl + (slot * 256));
    
    /* Memory zero fill */
    uint8_t* p = (uint8_t*)cmdtbl;
    for (size_t i = 0; i < sizeof(hba_cmd_tbl_t); i++) p[i] = 0;

    /* Buffer Aligned Allocation Simulation (512 Bytes Data) */
    uint16_t identify_buf[256];
    uintptr_t phys_buf = (uintptr_t)identify_buf; /* KayaOS MMU phys map kullanılmalıdır */

    cmdtbl->prdt_entry[0].dba  = (uint32_t)(phys_buf & 0xFFFFFFFF);
    cmdtbl->prdt_entry[0].dbau = (uint32_t)((uint64_t)phys_buf >> 32);
    cmdtbl->prdt_entry[0].dbc  = 511; /* 512 Bytes */
    cmdtbl->prdt_entry[0].i    = 1;

    fis_reg_h2d_t* cmdfis = (fis_reg_h2d_t*)(&cmdtbl->cfis);
    cmdfis->fis_type = FIS_TYPE_REG_H2D;
    cmdfis->c = 1;
    cmdfis->command = (dev->type == AHCI_DEV_SATAPI) ? ATA_CMD_IDENTIFY_PACKET : ATA_CMD_IDENTIFY;

    uint32_t spin = 0;
    while ((port->tfd & (0x80 | 0x08)) && spin < 1000000) spin++;
    if (spin == 1000000) return false;

    port->ci = (1U << slot);

    while (1) {
        if ((port->ci & (1U << slot)) == 0) break;
        if (port->is & (1U << 30)) return false; /* Task File Error */
    }

    /* Parse Model and Serial Strings */
    ahci_format_ata_string(dev->serial, &identify_buf[10], 10);
    ahci_format_ata_string(dev->model,  &identify_buf[27], 20);

    /* Capacity Calculation (LBA48 or LBA28) */
    uint32_t lba28_sectors = ((uint32_t)identify_buf[61] << 16) | identify_buf[60];
    uint64_t lba48_sectors = ((uint64_t)identify_buf[103] << 48) |
                             ((uint64_t)identify_buf[102] << 32) |
                             ((uint64_t)identify_buf[101] << 16) |
                             identify_buf[100];

    dev->capacity_sectors = (lba48_sectors > 0) ? lba48_sectors : lba28_sectors;
    dev->sector_size = 512;
    dev->present = true;

    return true;
}

bool ahci_port_read(ahci_port_device_t* dev, uint64_t start_lba, uint32_t count, uint8_t* buffer) {
    if (!dev || !dev->present || count == 0) return false;

    hba_port_t* port = dev->hba_port;
    port->is = 0xFFFFFFFF;

    int slot = ahci_find_cmdslot(port);
    if (slot == -1) return false;

    hba_cmd_header_t* cmdhdr = &dev->cmd_header[slot];
    cmdhdr->cfl = sizeof(fis_reg_h2d_t) / sizeof(uint32_t);
    cmdhdr->w = 0; /* Read */
    cmdhdr->prdtl = (uint16_t)((count - 1) / 16 + 1);

    hba_cmd_tbl_t* cmdtbl = (hba_cmd_tbl_t*)((uintptr_t)dev->cmd_tbl + (slot * 256));
    uint8_t* p = (uint8_t*)cmdtbl;
    for (size_t i = 0; i < sizeof(hba_cmd_tbl_t); i++) p[i] = 0;

    uint32_t bytes_left = count * dev->sector_size;
    uintptr_t cur_phys = (uintptr_t)buffer;

    int i = 0;
    for (i = 0; i < cmdhdr->prdtl; i++) {
        uint32_t chunk_size = (bytes_left > 8192) ? 8192 : bytes_left;
        cmdtbl->prdt_entry[i].dba  = (uint32_t)(cur_phys & 0xFFFFFFFF);
        cmdtbl->prdt_entry[i].dbau = (uint32_t)((uint64_t)cur_phys >> 32);
        cmdtbl->prdt_entry[i].dbc  = chunk_size - 1;
        cmdtbl->prdt_entry[i].i    = 1;

        cur_phys += chunk_size;
        bytes_left -= chunk_size;
    }

    fis_reg_h2d_t* cmdfis = (fis_reg_h2d_t*)(&cmdtbl->cfis);
    cmdfis->fis_type = FIS_TYPE_REG_H2D;
    cmdfis->c = 1;
    cmdfis->command = ATA_CMD_READ_DMA_EXT;

    cmdfis->lba0 = (uint8_t)(start_lba & 0xFF);
    cmdfis->lba1 = (uint8_t)((start_lba >> 8) & 0xFF);
    cmdfis->lba2 = (uint8_t)((start_lba >> 16) & 0xFF);
    cmdfis->device = 1 << 6; /* LBA Mode */

    cmdfis->lba3 = (uint8_t)((start_lba >> 24) & 0xFF);
    cmdfis->lba4 = (uint8_t)((start_lba >> 32) & 0xFF);
    cmdfis->lba5 = (uint8_t)((start_lba >> 40) & 0xFF);

    cmdfis->countlow  = (uint8_t)(count & 0xFF);
    cmdfis->counthigh = (uint8_t)((count >> 8) & 0xFF);

    uint32_t spin = 0;
    while ((port->tfd & (0x80 | 0x08)) && spin < 1000000) spin++;
    if (spin == 1000000) return false;

    port->ci = (1U << slot);

    while (1) {
        if ((port->ci & (1U << slot)) == 0) break;
        if (port->is & (1U << 30)) return false;
    }

    return true;
}

bool ahci_port_write(ahci_port_device_t* dev, uint64_t start_lba, uint32_t count, const uint8_t* buffer) {
    if (!dev || !dev->present || count == 0) return false;

    hba_port_t* port = dev->hba_port;
    port->is = 0xFFFFFFFF;

    int slot = ahci_find_cmdslot(port);
    if (slot == -1) return false;

    hba_cmd_header_t* cmdhdr = &dev->cmd_header[slot];
    cmdhdr->cfl = sizeof(fis_reg_h2d_t) / sizeof(uint32_t);
    cmdhdr->w = 1; /* Write Flag */
    cmdhdr->prdtl = (uint16_t)((count - 1) / 16 + 1);

    hba_cmd_tbl_t* cmdtbl = (hba_cmd_tbl_t*)((uintptr_t)dev->cmd_tbl + (slot * 256));
    uint8_t* p = (uint8_t*)cmdtbl;
    for (size_t i = 0; i < sizeof(hba_cmd_tbl_t); i++) p[i] = 0;

    uint32_t bytes_left = count * dev->sector_size;
    uintptr_t cur_phys = (uintptr_t)buffer;

    for (int i = 0; i < cmdhdr->prdtl; i++) {
        uint32_t chunk_size = (bytes_left > 8192) ? 8192 : bytes_left;
        cmdtbl->prdt_entry[i].dba  = (uint32_t)(cur_phys & 0xFFFFFFFF);
        cmdtbl->prdt_entry[i].dbau = (uint32_t)((uint64_t)cur_phys >> 32);
        cmdtbl->prdt_entry[i].dbc  = chunk_size - 1;
        cmdtbl->prdt_entry[i].i    = 1;

        cur_phys += chunk_size;
        bytes_left -= chunk_size;
    }

    fis_reg_h2d_t* cmdfis = (fis_reg_h2d_t*)(&cmdtbl->cfis);
    cmdfis->fis_type = FIS_TYPE_REG_H2D;
    cmdfis->c = 1;
    cmdfis->command = ATA_CMD_WRITE_DMA_EXT;

    cmdfis->lba0 = (uint8_t)(start_lba & 0xFF);
    cmdfis->lba1 = (uint8_t)((start_lba >> 8) & 0xFF);
    cmdfis->lba2 = (uint8_t)((start_lba >> 16) & 0xFF);
    cmdfis->device = 1 << 6;

    cmdfis->lba3 = (uint8_t)((start_lba >> 24) & 0xFF);
    cmdfis->lba4 = (uint8_t)((start_lba >> 32) & 0xFF);
    cmdfis->lba5 = (uint8_t)((start_lba >> 40) & 0xFF);

    cmdfis->countlow  = (uint8_t)(count & 0xFF);
    cmdfis->counthigh = (uint8_t)((count >> 8) & 0xFF);

    uint32_t spin = 0;
    while ((port->tfd & (0x80 | 0x08)) && spin < 1000000) spin++;
    if (spin == 1000000) return false;

    port->ci = (1U << slot);

    while (1) {
        if ((port->ci & (1U << slot)) == 0) break;
        if (port->is & (1U << 30)) return false;
    }

    return true;
}