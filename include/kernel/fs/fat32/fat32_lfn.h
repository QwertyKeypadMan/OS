#ifndef KERNEL_FAT32_LFN_H
#define KERNEL_FAT32_LFN_H

#include "kernel/fs/fat32/fat32_structs.h"

uint8_t fat32_lfn_checksum(const uint8_t* short_name);
void fat32_lfn_to_string(const fat32_lfn_entry_t* lfn, char* out_buf);
void fat32_create_short_name(const char* long_name, char* short_name_out);

#endif /* KERNEL_FAT32_LFN_H */