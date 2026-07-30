#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "kernel/graphics.h"

#include "kernel/multiboot.h"
#include "kernel/ramfs.h"
#include "kernel/shell.h"
#include "kernel/terminal.h"
#include "kernel/version.h"
#include "kernel/paging.h"
#include "kernel/gdt.h"
#include "kernel/mouse.h"
#include "kernel/keyboard.h"
#include "kernel/apic.h"
#include "kernel/rtc.h"
#include "kernel/elf_loader.h"
#include "kernel/pci/pci.h"
#include "kernel/driver/storage/ahci.h"
#include "kernel/driver/driver.h"
#include "../src/kernel/driver/gpu/virtio_gpu/virtio_gpu.h"
#include "../src/kernel/net/virtio_net.h"
#include "../src/kernel/net/ethernet.h"

/*  GRAFİK ARAYÜZ KATMANI BAĞLANTISI */
#include "kernel/gui.h"
#include "kernel/kstring.h"

#define MULTIBOOT_BOOTLOADER_MAGIC 0x2BADB002

extern unsigned char fontum_psf[];
extern unsigned int fontum_psf_len;
extern bool gui_needs_redraw(void);

/* Global Mouse Koordinatları */
static int mouse_x = 400;
static int mouse_y = 300;

void abort(void) {
    terminal_setcolor(VGA_COLOR_WHITE, VGA_COLOR_RED);
    terminal_writestring("\n!!! KERNEL PANIC: abort() invoked by internal runtime !!!\n");
    
    while (1) {
        __asm__ __volatile__("cli; hlt");
    }
}

void init_network(void) {
    pci_device_t* dev = pci_find_device(VIRTIO_VENDOR_ID, VIRTIO_NET_DEVICE_ID_LEGACY);
    if (!dev) {
        dev = pci_find_device(VIRTIO_VENDOR_ID, VIRTIO_NET_DEVICE_ID_MODERN);
    }

    if (dev) {
        pci_bar_t bar0;
        if (pci_get_bar(dev, 0, &bar0)) {
            uint16_t io_base = (uint16_t)(bar0.base_address & ~0x3);

            if (virtio_net_init(io_base) == 0) {
                /* Ethernet katmanını ilklendir */
                ethernet_init();
            }
        }
    }
}

void kernel_main(uint32_t magic, uint32_t mb_addr)

{
	vfs_init();

    // 2. RAMFS Sürücüsünü Kök Dizin ("/") Olarak Bağla
    vfs_mount("/", ramfs_get_driver());
    // 1. Multiboot değişkenini tanımlıyoruz
    multiboot_info_t *mbi = (multiboot_info_t *)mb_addr;

    // 2. TEMEL CPU VE BELLEK YAPILARI
    init_gdt();
    idt_init();
    
    init_paging(mbi); 

    // 3. GRAFİK VE TERMİNAL BAŞLATMA
    if (magic == MULTIBOOT_BOOTLOADER_MAGIC && mb_addr != 0) {
        multiboot_info_t* r_mbi = (multiboot_info_t*)(uintptr_t)mb_addr;
        graphics_initialize(r_mbi);
    } else {
        graphics_initialize(NULL);
    }

    terminal_initialize();
    terminal_setcolor(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);

    terminal_writestring(KAYAOS_NAME " " KAYAOS_VERSION "\n");
    terminal_writestring("Freestanding C kernel with shell and RAMFS\n\n");

    if (magic != MULTIBOOT_BOOTLOADER_MAGIC) {
        terminal_setcolor(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        terminal_writestring("Warning: kernel was not started by a Multiboot loader.\n\n");
        terminal_setcolor(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    }

    // 4. DİĞER MODÜLLERİN BAŞLATILMASI
	ramfs_init();
	pci_init();
	virtio_gpu_driver_register();
	init_network();
	driver_manager_init();
	ahci_driver_register();
	block_init();
    mouse_initialize();
	kprintf("%ux%u\n", graphics_width(), graphics_height());
    

    mouse_x = graphics_width() / 2;
    mouse_y = graphics_height() / 2;

    terminal_clear(); 
    apic_timer_init();
    gui_init();

    
    mouse_packet_t mouse_pkt;
    char kb_char;
    uint8_t kb_scancode;
    bool is_clicked = false;

    // 5. ANA DÖNGÜ
    while (1) {
        /* FARE VERİLERİNİ OKU */
        if (mouse_poll(&mouse_pkt)) {
            mouse_x += mouse_pkt.dx;
            mouse_y -= mouse_pkt.dy;

            if (mouse_x < 0) mouse_x = 0;
            if (mouse_y < 0) mouse_y = 0;
            if (mouse_x >= (int)graphics_width()) mouse_x = graphics_width() - 1;
            if (mouse_y >= (int)graphics_height()) mouse_y = graphics_height() - 1;

            is_clicked = mouse_pkt.left_button;
        }
        
        /* KLAVYE VERİLERİNİ OKU VE GUI KATMANINA İLET */
        if (keyboard_try_read_event(&kb_char, &kb_scancode)) {
            gui_dispatch_key(kb_char, kb_scancode);
        }

        gui_update(mouse_x, mouse_y, is_clicked);

        /* TÜM EKRANI, PENCERELERİ VE İMLECİ ÇİZ */
        gui_draw();
        ethernet_poll();
		
    }

    shell_run();
}