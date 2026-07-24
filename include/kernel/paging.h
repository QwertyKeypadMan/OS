#ifndef PAGING_H
#define PAGING_H
#include <stdint.h>
#include <stdbool.h>
#include "kernel/multiboot.h" // Multiboot yapının olduğu dosya yolu

/* ---- Eski API (geriye uyumluluk) ---- */
void map_page(uint32_t virt, uint32_t phys, uint32_t flags);
void map_range(uint32_t start_addr, uint32_t size, uint32_t flags);

/* ---- Yeni ring3/user-mode paging API'si ---- */
bool init_paging(multiboot_info_t *mboot_ptr);
uint32_t paging_create_address_space(void);
void paging_destroy_address_space(uint32_t dir_phys);
bool paging_map_user_page(uint32_t dir_phys, uint32_t virt, bool writable);
void paging_unmap_user_page(uint32_t dir_phys, uint32_t virt);
void paging_switch_directory(uint32_t dir_phys);
uint32_t paging_kernel_directory_phys(void);
void paging_page_fault(uint32_t error_code, uint32_t faulting_addr);

#endif