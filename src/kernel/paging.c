#include "kernel/paging.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "kernel/io.h"

/* =========================================================================
 * KayaOS Paging — Ring3/User-Mode Temeli
 * -------------------------------------------------------------------------
 * ESKI HATA: allocate_page_table() sabit placement_address=0x800000'dan
 * (8MB) itibaren bump-allocate yapiyordu. Ama kstring.c'deki 8MB'lik
 * kernel_heap[] ve graphics.c'deki 4MB'lik back_buffer[] gibi statik
 * tamponlar kernelin BSS'inde -- toplam kernel imaji cok rahat 8MB'i
 * asiyor. Yani eski kod sayfa tablolarini KENDI HEAP'ININ UZERINE
 * yaziyordu (sessiz bellek bozulmasi).
 *
 * BU DOSYA:
 *  1) Kernel icin yeterince genis (64MB), guvenli bir identity-map alani
 *     ayirir (kod+data+heap+backbuffer+her sey buraya sigar).
 *  2) Sayfa TABLOLARI/DIZINLERI icin static "havuz" (bump, hic free
 *     edilmez -- k_free'nin no-op olmasiyla ayni felsefe) kullanir; bu
 *     havuz identity-map alaninin icinde oldugu icin kernel HER ZAMAN
 *     dogrudan (fiziksel=sanal) erisebilir.
 *  3) Kullanici PROGRAM verisi icin ayri, gercek bir bitmap tabanli
 *     fiziksel cerceve (frame) ayiricisi kullanir -- bu ALLOC/FREE
 *     yapilabilir, cunku process'ler acilip kapanacak.
 *  4) Her process kendi page directory'sine sahip olabilir
 *     (paging_create_address_space) -- kernel kismi HER ZAMAN ayni
 *     (paylasilan) sayfa tablolarina isaret eder, boylece bir interrupt
 *     geldiginde CPU hangi process'in CR3'u yuklu olursa olsun kernel
 *     kodunu calistirmaya devam edebilir. Kullanici kismi PAGE_USER
 *     bayrakli ve izoledir.
 *
 * NOT (paging.h'a eklemen gerekenler): asagidaki public fonksiyonlarin
 * prototiplerini kernel/paging.h'a ekle:
 *   bool paging_init(multiboot_info_t *mboot_ptr);
 *   uint32_t paging_create_address_space(void);
 *   void paging_destroy_address_space(uint32_t dir_phys);
 *   bool paging_map_user_page(uint32_t dir_phys, uint32_t virt, bool writable);
 *   void paging_unmap_user_page(uint32_t dir_phys, uint32_t virt);
 *   void paging_switch_directory(uint32_t dir_phys);
 *   uint32_t paging_kernel_directory_phys(void);
 *   void paging_page_fault(uint32_t error_code, uint32_t faulting_addr);
 * Eski isimler (init_paging, map_page, map_range) geriye-uyumluluk icin
 * korunuyor, kernel_main.c'yi degistirmene gerek yok.
 * ========================================================================= */

/* ---- Bagimsiz seri port teshis logu (idt.c/gui.c'deki ile ayni desen) ---- */
#define PAGING_COM1 0x3F8

static void pg_log(const char *s) {
    while (*s) {
        if (*s == '\n') {
            while ((inb(PAGING_COM1 + 5) & 0x20) == 0) { }
            outb(PAGING_COM1, '\r');
        }
        while ((inb(PAGING_COM1 + 5) & 0x20) == 0) { }
        outb(PAGING_COM1, (uint8_t)*s++);
    }
}

static void pg_log_hex(uint32_t v) {
    static const char digits[] = "0123456789ABCDEF";
    char buf[11] = "0x00000000";
    for (int i = 0; i < 8; i++) {
        buf[9 - i] = digits[v & 0xF];
        v >>= 4;
    }
    pg_log(buf);
}

/* ---- 📦 SABITLER VE BAYRAKLAR ---- */
#define PAGE_SIZE            4096u
#define PAGE_ENTRIES         1024u

#define PAGE_PRESENT         (1u << 0)
#define PAGE_RW              (1u << 1)
#define PAGE_USER            (1u << 2)
#define PAGE_PWT             (1u << 3)
#define PAGE_PCD             (1u << 4)   /* Framebuffer icin sart */

#define PAGE_ALIGN_DOWN(x)   ((x) & ~(PAGE_SIZE - 1))
#define PAGE_ALIGN_UP(x)     (((x) + (PAGE_SIZE - 1)) & ~(PAGE_SIZE - 1))

/* Kernelin (kod+data+heap+backbuffer+her seyi) sigdigi, HER ZAMAN
 * supervisor-only (PAGE_USER YOK) identity-map bolgesi. 8MB heap +
 * 4MB backbuffer + font/glyph cache + yigin + kod/data icin 64MB bol
 * bir pay birakiyoruz. QEMU'ya en az bu kadar RAM verildiginden emin ol
 * (-m 128 veya daha fazla). */
#define IDENTITY_MAP_MB       160u
#define IDENTITY_MAP_LIMIT    (IDENTITY_MAP_MB * 1024u * 1024u)

/* Kullanici adres alani identity bolgesinin hemen ustunden baslar */
#define USER_SPACE_BASE        IDENTITY_MAP_LIMIT
#define USER_SPACE_TOP          0xF0000000u  /* recursive/ozel alanlara yer birak */

/* ---- 🗂️ SAYFA TABLOSU / DIZINI HAVUZU (bump, identity alaninda) ----
 * k_free'nin no-op olmasi gibi: sayfa tablolari/dizinleri tek tek
 * geri verilmez, ama bu hobi OS icin sorun degil -- havuz yeterince
 * genis. ONEMLI: bu dizi identity-map bolgesinde (dolayisiyla
 * fiziksel==sanal) oldugu icin kernel her zaman dogrudan yazabilir. */
#define TABLE_POOL_COUNT      256   /* 256 * 4KB = 1MB, ~1024MB'lik mapping'e yeter */
#define DIR_POOL_COUNT        16    /* 16 process'e kadar ayri adres alani */

static uint32_t table_pool[TABLE_POOL_COUNT][PAGE_ENTRIES] __attribute__((aligned(4096)));
static bool     table_pool_used[TABLE_POOL_COUNT];
static uint32_t table_pool_next = 0;

static uint32_t dir_pool[DIR_POOL_COUNT][PAGE_ENTRIES] __attribute__((aligned(4096)));
static bool     dir_pool_used[DIR_POOL_COUNT];

static uint32_t *alloc_table_frame(void) {
    for (uint32_t i = 0; i < TABLE_POOL_COUNT; i++) {
        uint32_t idx = (table_pool_next + i) % TABLE_POOL_COUNT;
        if (!table_pool_used[idx]) {
            table_pool_used[idx] = true;
            table_pool_next = idx + 1;
            for (uint32_t j = 0; j < PAGE_ENTRIES; j++) table_pool[idx][j] = 0;
            return table_pool[idx];
        }
    }
    pg_log("[PAGING] HATA: table_pool tukendi!\n");
    return 0;
}

static uint32_t *alloc_dir_frame(void) {
    for (uint32_t i = 0; i < DIR_POOL_COUNT; i++) {
        if (!dir_pool_used[i]) {
            dir_pool_used[i] = true;
            for (uint32_t j = 0; j < PAGE_ENTRIES; j++) dir_pool[i][j] = 0;
            return dir_pool[i];
        }
    }
    pg_log("[PAGING] HATA: dir_pool tukendi!\n");
    return 0;
}

static void free_dir_frame(uint32_t *dir) {
    for (uint32_t i = 0; i < DIR_POOL_COUNT; i++) {
        if (dir_pool[i] == dir) {
            dir_pool_used[i] = false;
            return;
        }
    }
}

/* ---- 🧮 FIZIKSEL CERCEVE (FRAME) BITMAP AYIRICISI ----
 * SADECE kullanici PROGRAM verisi (kod/veri/yigin sayfalari) icin.
 * Tablo/dizin havuzunun aksine bunlar gercekten alloc/free edilir --
 * bir process kapaninca bellegi geri almamiz lazim. */
#define MAX_PHYS_MB    512u
#define MAX_FRAMES     ((MAX_PHYS_MB * 1024u * 1024u) / PAGE_SIZE)   /* 32768 */

static uint8_t frame_bitmap[MAX_FRAMES / 8];

static inline void bitmap_set(uint32_t idx)   { frame_bitmap[idx / 8] |= (uint8_t)(1u << (idx % 8)); }
static inline void bitmap_clear(uint32_t idx) { frame_bitmap[idx / 8] &= (uint8_t)~(1u << (idx % 8)); }
static inline bool bitmap_test(uint32_t idx)  { return (frame_bitmap[idx / 8] & (1u << (idx % 8))) != 0; }

/* Identity bolgesindeki her seyi (kernel imaji, heap, backbuffer, tablo/
 * dizin havuzlari) frame havuzundan HARIC tutuyoruz -- boylece user
 * frame allocator kernelin uzerine asla frame veremez. */
static void frame_bitmap_init(void) {
    for (uint32_t i = 0; i < MAX_FRAMES / 8; i++) frame_bitmap[i] = 0;

    uint32_t reserved_frames = IDENTITY_MAP_LIMIT / PAGE_SIZE;
    if (reserved_frames > MAX_FRAMES) reserved_frames = MAX_FRAMES;
    for (uint32_t i = 0; i < reserved_frames; i++) bitmap_set(i);
}

static uint32_t frame_alloc(void) {
    for (uint32_t i = 0; i < MAX_FRAMES; i++) {
        if (!bitmap_test(i)) {
            bitmap_set(i);
            return i * PAGE_SIZE;
        }
    }
    pg_log("[PAGING] HATA: fiziksel bellek tukendi (frame_alloc basarisiz)\n");
    return 0;
}

static void frame_free(uint32_t phys) {
    uint32_t idx = phys / PAGE_SIZE;
    if (idx < MAX_FRAMES) bitmap_clear(idx);
}

/* ---- 🌍 CEKIRDEK PAGE DIRECTORY ---- */
static uint32_t kernel_page_directory[PAGE_ENTRIES] __attribute__((aligned(4096)));

/* ---- 🔧 ALT SEVIYE MAP/UNMAP YARDIMCILARI ----
 * dir dogrudan yazilabilir bir page directory pointer'i (identity
 * alaninda oldugu icin fiziksel==sanal varsayimi guvenli). */
static uint32_t *get_or_create_table(uint32_t *dir, uint32_t pd_index, bool user) {
    if (dir[pd_index] & PAGE_PRESENT) {
        return (uint32_t *)(dir[pd_index] & 0xFFFFF000u);
    }

    uint32_t *table = alloc_table_frame();
    if (table == 0) return 0;

    uint32_t flags = PAGE_PRESENT | PAGE_RW;
    if (user) flags |= PAGE_USER;
    dir[pd_index] = ((uint32_t)table) | flags;
    return table;
}

static void map_page_in(uint32_t *dir, uint32_t virt, uint32_t phys, uint32_t flags) {
    uint32_t pd_index = virt >> 22;
    uint32_t pt_index = (virt >> 12) & 0x3FFu;
    bool user = (flags & PAGE_USER) != 0;

    uint32_t *table = get_or_create_table(dir, pd_index, user);
    if (table == 0) return;

    table[pt_index] = PAGE_ALIGN_DOWN(phys) | flags;

    if (dir == kernel_page_directory) {
        __asm__ __volatile__("invlpg (%0)" : : "r"(virt) : "memory");
    }
}

static void unmap_page_in(uint32_t *dir, uint32_t virt) {
    uint32_t pd_index = virt >> 22;
    uint32_t pt_index = (virt >> 12) & 0x3FFu;

    if (!(dir[pd_index] & PAGE_PRESENT)) return;
    uint32_t *table = (uint32_t *)(dir[pd_index] & 0xFFFFF000u);
    table[pt_index] = 0;

    if (dir == kernel_page_directory) {
        __asm__ __volatile__("invlpg (%0)" : : "r"(virt) : "memory");
    }
}

static void map_range_in(uint32_t *dir, uint32_t start_addr, uint32_t size, uint32_t flags) {
    uint32_t end_addr = start_addr + size;
    for (uint32_t addr = PAGE_ALIGN_DOWN(start_addr); addr < end_addr; addr += PAGE_SIZE) {
        map_page_in(dir, addr, addr, flags);
    }
}

/* ---- ESKI API ISIMLERI (geriye uyumluluk -- kernel_main.c'yi bozma) ---- */
void map_page(uint32_t virt, uint32_t phys, uint32_t flags) {
    map_page_in(kernel_page_directory, virt, phys, flags);
}

void map_range(uint32_t start_addr, uint32_t size, uint32_t flags) {
    map_range_in(kernel_page_directory, start_addr, size, flags);
}

/* ---- 🌍 CEKIRDEK IDENTITY-MAP KURULUMU ---- */
bool init_paging(multiboot_info_t *mboot_ptr) {
    pg_log("\n[PAGING] init_paging basladi\n");

    for (uint32_t i = 0; i < TABLE_POOL_COUNT; i++) table_pool_used[i] = false;
    for (uint32_t i = 0; i < DIR_POOL_COUNT; i++)   dir_pool_used[i] = false;
    table_pool_next = 0;

    for (uint32_t i = 0; i < PAGE_ENTRIES; i++) kernel_page_directory[i] = 0;

    frame_bitmap_init();

    /* 1) Kernelin TAMAMINI (kod+data+8MB heap+4MB backbuffer+her sey)
     *    kapsayan genis, supervisor-only identity map. ESKI HATA burada
     *    duzeldi: eskiden 16MB'di ve heap/backbuffer bunun disina
     *    tasabiliyordu. */
    map_range_in(kernel_page_directory, 0x0, IDENTITY_MAP_LIMIT, PAGE_PRESENT | PAGE_RW);
    pg_log("[PAGING] Identity map: 0x0 - ");
    pg_log_hex(IDENTITY_MAP_LIMIT);
    pg_log("\n");

    /* 2) Framebuffer'i haritala (PCD sart -- VRAM cache'lenmemeli) */
    bool fb_mapped = false;
    if (mboot_ptr != NULL && (mboot_ptr->flags & (1u << 12))) {
        uint32_t fb_addr = (uint32_t)mboot_ptr->framebuffer_addr_low;
        uint32_t fb_size = mboot_ptr->framebuffer_pitch * mboot_ptr->framebuffer_height;
        map_range_in(kernel_page_directory, fb_addr, fb_size, PAGE_PRESENT | PAGE_RW | PAGE_PCD);
        pg_log("[PAGING] Framebuffer (multiboot) haritalandi: ");
        pg_log_hex(fb_addr);
        pg_log("\n");
        fb_mapped = true;
    }
    if (!fb_mapped) {
        /* graphics.c framebuffer'i hep 0xFD000000'a hardcode ediyor --
         * multiboot bayragi yoksa yine de garanti altina al. */
        map_range_in(kernel_page_directory, 0xFD000000u, 1024u * 768u * 4u,
                     PAGE_PRESENT | PAGE_RW | PAGE_PCD);
        pg_log("[PAGING] Framebuffer (fallback 0xFD000000) haritalandi\n");
    }
	 map_range_in(kernel_page_directory, 0xFEE00000u, PAGE_SIZE,
                 PAGE_PRESENT | PAGE_RW | PAGE_PCD);
     pg_log("[PAGING] LAPIC MMIO (0xFEE00000) haritalandi\n");

    /* 3) Kendi kendine isaret eden PDE (0xFFC00000+): boylece istersen
     *    aktif page directory/tablolari sanal adresten inceleyebilirsin. */
    kernel_page_directory[1023] = ((uint32_t)kernel_page_directory) | PAGE_PRESENT | PAGE_RW;

    /* 4) CR3 ve CR0 yukle */
    __asm__ __volatile__("mov %0, %%cr3" : : "r"(kernel_page_directory) : "memory");

    uint32_t cr0;
    __asm__ __volatile__("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000u; /* PG biti */
    __asm__ __volatile__("mov %0, %%cr0" : : "r"(cr0) : "memory");

    pg_log("[PAGING] Paging aktif (CR0.PG=1)\n");
    return true;
}

uint32_t paging_kernel_directory_phys(void) {
    return (uint32_t)kernel_page_directory;
}

/* ---- 👤 RING3/USER-MODE: PER-PROCESS ADRES ALANI ----
 * Kernelin PDE'lerini (identity bolgesi) YENI dizine kopyalar --
 * boylece bu process'e gecildiginde bile kernel kodu/veri sanki hep
 * oradaymis gibi calismaya devam eder (interrupt/syscall handling icin
 * SART). Kullanici sayfalari ise bu fonksiyondan SONRA
 * paging_map_user_page() ile eklenir. */
uint32_t paging_create_address_space(void) {
    uint32_t *dir = alloc_dir_frame();
    if (dir == 0) return 0;

    /* Identity bolgesini kapsayan PDE'leri paylas (ayni fiziksel
     * tablolara isaret eder -- kernel guncellemesi otomatik her
     * process'e yansir). */
    uint32_t kernel_pdes = PAGE_ALIGN_UP(IDENTITY_MAP_LIMIT) >> 22;
    for (uint32_t i = 0; i <= kernel_pdes && i < PAGE_ENTRIES - 1; i++) {
        dir[i] = kernel_page_directory[i];
    }

    /* Kendi kendine isaret eden PDE, bu dizin icin */
    dir[1023] = ((uint32_t)dir) | PAGE_PRESENT | PAGE_RW;

    pg_log("[PAGING] Yeni adres alani olusturuldu: ");
    pg_log_hex((uint32_t)dir);
    pg_log("\n");
    return (uint32_t)dir;
}

void paging_destroy_address_space(uint32_t dir_phys) {
    uint32_t *dir = (uint32_t *)dir_phys;
    if (dir == 0 || dir == kernel_page_directory) return;

    /* Sadece USER_SPACE_BASE ustundeki (kullaniciya ait) frame'leri geri
     * ver -- kernel tablolari paylasimli oldugu icin dokunulmuyor. */
    uint32_t kernel_pdes = PAGE_ALIGN_UP(IDENTITY_MAP_LIMIT) >> 22;
    for (uint32_t pd = kernel_pdes + 1; pd < 1023; pd++) {
        if (!(dir[pd] & PAGE_PRESENT)) continue;
        uint32_t *table = (uint32_t *)(dir[pd] & 0xFFFFF000u);
        for (uint32_t pt = 0; pt < PAGE_ENTRIES; pt++) {
            if (table[pt] & PAGE_PRESENT) {
                frame_free(table[pt] & 0xFFFFF000u);
            }
        }
        /* NOT: table_pool'daki tablonun kendisi geri verilmiyor (havuz
         * felsefesi -- k_free gibi tek yonlu). Bu hobi OS'te sorun
         * degil; TABLE_POOL_COUNT'u yeterince genis tuttugumuz surece. */
    }

    free_dir_frame(dir);
    pg_log("[PAGING] Adres alani yok edildi\n");
}

/* virt: USER_SPACE_BASE ile USER_SPACE_TOP arasinda, sayfa hizali
 * olmasa da olur (fonksiyon kendisi hizaliyor). Basarili olursa fiziksel
 * bir frame ayirir, kullanici-erisebilir olarak haritalar. */
bool paging_map_user_page(uint32_t dir_phys, uint32_t virt, bool writable) {
    uint32_t *dir = (uint32_t *)dir_phys;
    if (dir == 0) return false;
    if (virt < USER_SPACE_BASE || virt >= USER_SPACE_TOP) {
        pg_log("[PAGING] HATA: user mapping istegi kernel/rezerve alaninda: ");
        pg_log_hex(virt);
        pg_log("\n");
        return false;
    }

    uint32_t phys = frame_alloc();
    if (phys == 0) return false;

    uint32_t flags = PAGE_PRESENT | PAGE_USER;
    if (writable) flags |= PAGE_RW;

    map_page_in(dir, PAGE_ALIGN_DOWN(virt), phys, flags);
    return true;
}

void paging_unmap_user_page(uint32_t dir_phys, uint32_t virt) {
    uint32_t *dir = (uint32_t *)dir_phys;
    if (dir == 0) return;

    uint32_t pd_index = virt >> 22;
    uint32_t pt_index = (virt >> 12) & 0x3FFu;
    if (!(dir[pd_index] & PAGE_PRESENT)) return;

    uint32_t *table = (uint32_t *)(dir[pd_index] & 0xFFFFF000u);
    if (table[pt_index] & PAGE_PRESENT) {
        frame_free(table[pt_index] & 0xFFFFF000u);
    }
    unmap_page_in(dir, virt);
}

/* Context switch'in kalbi: CR3'u degistir. TSS.esp0 ayari (ring3'ten
 * ring0'a donuste kullanilacak kernel yigini) BU DOSYANIN KAPSAMI
 * DISINDA -- gdt.c'ye bir TSS girisi eklemen lazim, ring3'e ilk iret
 * yapmadan once. */
void paging_switch_directory(uint32_t dir_phys) {
    if (dir_phys == 0) return;
    __asm__ __volatile__("mov %0, %%cr3" : : "r"(dir_phys) : "memory");
}

/* ---- 🚨 PAGE FAULT TESHISI ----
 * idt.c'deki isr_14 su an her page fault'ta direkt HALT ediyor -- bu
 * fonksiyonu istersen fault_panic()'ten ONCE cagirip daha detayli
 * (user/kernel, read/write, present/not-present) bilgi alabilirsin:
 *
 *   DEFINE_ISR_ERR(14) icindeki cagriyi degistirmek yerine, en kolay
 *   yontem idt.c'de isr_14'un govdesine tek satir eklemek:
 *       paging_page_fault(error_code, cr2);
 *   (fault_panic'ten hemen once) */
void paging_page_fault(uint32_t error_code, uint32_t faulting_addr) {
    bool present  = (error_code & 0x1) != 0;
    bool write    = (error_code & 0x2) != 0;
    bool user     = (error_code & 0x4) != 0;
    bool reserved = (error_code & 0x8) != 0;

    pg_log("\n[PAGING] PAGE FAULT @ ");
    pg_log_hex(faulting_addr);
    pg_log("\n  Sebep     : ");
    pg_log(present ? "korumali erisim ihlali (present)\n" : "sayfa mevcut degil (not-present)\n");
    pg_log("  Islem     : ");
    pg_log(write ? "yazma\n" : "okuma\n");
    pg_log("  Mod       : ");
    pg_log(user ? "USER (ring3)\n" : "KERNEL (ring0)\n");
    if (reserved) {
        pg_log("  UYARI     : sayfa tablosunda rezerve bit ihlali (bozuk tablo?)\n");
    }
}

static uint8_t sample_user_code[] = {
    0x90,       // nop
    0xEB, 0xFE  // jmp $
};

uint32_t create_user_process(void) {
    // 1. Yeni bir adres alanı (page directory) oluştur
    uint32_t dir_phys = paging_create_address_space();
    if (dir_phys == 0) {
        return 0; // Başarısız
    }

    // 2. Kod alanı için bir sayfa haritala (Örn: USER_SPACE_BASE adresi)
    // Yazılabilir (writable=true) yapıyoruz ki içine kod kopyalayabilelim.
    uint32_t code_virt = USER_SPACE_BASE; 
    if (!paging_map_user_page(dir_phys, code_virt, true)) {
        paging_destroy_address_space(dir_phys);
        return 0;
    }

    // 3. Stack (Yığın) alanı için bir sayfa haritala (Kodun bir sayfa üstü)
    uint32_t stack_virt = USER_SPACE_BASE + 4096;
    if (!paging_map_user_page(dir_phys, stack_virt, true)) {
        paging_destroy_address_space(dir_phys);
        return 0;
    }

    // 4. Kodu kullanıcı alanına kopyalamak için 
    // Dikkat: code_virt adresi şu an sadece yeni oluşturduğumuz 'dir_phys' dizininde haritalı.
    // Güvenli kopyalama için CR3'ü geçici olarak yeni dizine çevirebiliriz 
    // çünkü kernel alanları (identity map) her iki dizinde de ortaktır!
    uint32_t old_cr3;
    __asm__ __volatile__("mov %%cr3, %0" : "=r"(old_cr3));
    
    paging_switch_directory(dir_phys);

    // Artık user uzayındayız (ama hala ring 0'dayız), kodu yazabiliriz:
    volatile uint8_t *dest = (volatile uint8_t *)code_virt;
    for (unsigned int i = 0; i < sizeof(sample_user_code); i++) {
        dest[i] = sample_user_code[i];
    }

    // Eski kernel directory'sine geri dön (isteğe bağlı, zıplayana kadar kalabilir de)
    paging_switch_directory(old_cr3);

    return dir_phys; // Oluşturduğumuz process'in dizin adresini döndür
}
uint32_t paging_identity_map_limit(void) {
    return IDENTITY_MAP_LIMIT;
}
