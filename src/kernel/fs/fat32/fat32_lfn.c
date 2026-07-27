#include "kernel/fs/fat32/fat32_lfn.h"

uint8_t fat32_lfn_checksum(const uint8_t* short_name) {
    uint8_t sum = 0;
    for (int i = 11; i > 0; i--) {
        sum = ((sum & 1) ? 0x80 : 0) + (sum >> 1) + *short_name++;
    }
    return sum;
}

void fat32_lfn_to_string(const fat32_lfn_entry_t* lfn, char* out_buf) {
    int idx = 0;
    for (int i = 0; i < 5; i++) {
        if (lfn->name1[i] == 0x0000 || lfn->name1[i] == 0xFFFF) break;
        out_buf[idx++] = (char)(lfn->name1[i] & 0xFF);
    }
    for (int i = 0; i < 6; i++) {
        if (lfn->name2[i] == 0x0000 || lfn->name2[i] == 0xFFFF) break;
        out_buf[idx++] = (char)(lfn->name2[i] & 0xFF);
    }
    for (int i = 0; i < 2; i++) {
        if (lfn->name3[i] == 0x0000 || lfn->name3[i] == 0xFFFF) break;
        out_buf[idx++] = (char)(lfn->name3[i] & 0xFF);
    }
    out_buf[idx] = '\0';
}