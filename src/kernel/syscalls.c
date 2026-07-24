
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>   // O_CREAT, O_RDONLY gibi bayraklar için
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdarg.h>
#include "kernel/io.h"
#include <sys/time.h>
#include "kernel/rtc.h"
#include <stdlib.h>
#define OPT_LOG_COM1 0x3F8

// --- Senin RamFS Başlık Dosyası ---
#include "kernel/ramfs.h"
#include "kernel/paging.h"

// --- Terminal Yazdırma Fonksiyonun ---
extern void graphics_draw_text(const char* text, int x, int y, unsigned int color);
extern uint32_t paging_identity_map_limit(void);

// --- Terminal Ayarları ---
static int cursor_x = 10;
static int cursor_y = 10;
#define FONT_WIDTH      8
#define FONT_HEIGHT     16
#define SCREEN_WIDTH    800
#define SCREEN_HEIGHT   600
#define TEXT_COLOR      0xFFFFFF

// --- KERNEL HEAP SINIRI (PAGE FAULT ENGELLEYİCİ) ---
#define KERNEL_HEAP_MAX_SIZE (64 * 1024 * 1024)

void opt_log(const char *s) {
    if (!s) return;
    while (*s) {
        if (*s == '\\n') {
            while ((inb(OPT_LOG_COM1 + 5) & 0x20) == 0) { }
            outb(OPT_LOG_COM1, '\\r');
        }
        while ((inb(OPT_LOG_COM1 + 5) & 0x20) == 0) { }
        outb(OPT_LOG_COM1, (uint8_t)*s++);
    }
}

// --- Hafif File Descriptor Tablosu ---
typedef struct {
    bool open;
    int node_id;    // RamFS içindeki düğüm numarası
    size_t offset;  // Dosyadaki okuma/yazma imleci konumu
} open_file_t;

#define MAX_OPEN_FILES 16
static open_file_t fd_table[MAX_OPEN_FILES];

// Helper: Boş bir dosya slotu bulur
static int get_free_fd(void) {
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if (!fd_table[i].open) {
            return i;
        }
    }
    return -1;
}

/*
 * 1. _open: FreeType veya TCC dosya açmak istediğinde çalışır.
 */
int _open(const char *name, int flags, ...) {
    if (!name) return -1;

    const char *target_name = name;

    // 1. TCC'nin aradığı yol ne olursa olsun "tccdefs.h" ifadesini yakala
    bool is_tccdefs = false;
    for (const char *p = name; *p; p++) {
        if (p[0]=='t' && p[1]=='c' && p[2]=='c' && p[3]=='d' && 
            p[4]=='e' && p[5]=='f' && p[6]=='s' && p[7]=='.' && p[8]=='h') {
            is_tccdefs = true;
            break;
        }
    }

    if (is_tccdefs) {
        target_name = "tccdefs.h";
    } else {
        // Genel dosya yolları için baştaki klasör takılarını temizle (/desktop/app.opa -> app.opa)
        const char *last_slash = name;
        for (const char *p = name; *p; p++) {
            if (*p == '/') last_slash = p + 1;
        }
        if (*last_slash != '\0') target_name = last_slash;
    }

    // RamFS'te dosyayı ara
    int node_id = ramfs_resolve(0, target_name);

    // 2. KORUMA: Eğer tccdefs.h RamFS'te henüz yoksa ANINDA bellekte oluştur!
    if (node_id < 0 && is_tccdefs) {
        node_id = ramfs_touch(0, "tccdefs.h");
        if (node_id >= 0) {
            ramfs_node_t *node = (ramfs_node_t *)ramfs_get(node_id);
            if (node) {
                const char *default_tccdefs = 
                    "#ifndef _TCCDEFS_H\n"
                    "#define _TCCDEFS_H\n"
                    "typedef unsigned int size_t;\n"
                    "typedef int ptrdiff_t;\n"
                    "typedef int wchar_t;\n"
                    "typedef int intptr_t;\n"
                    "typedef unsigned int uintptr_t;\n"
                    "#define NULL ((void*)0)\n"
                    "#endif\n";
                
                int len = 0;
                while (default_tccdefs[len]) len++;
                for (int i = 0; i < len; i++) {
                    node->data[i] = default_tccdefs[i];
                }
                node->size = len;
                node->type = RAMFS_NODE_FILE;
            }
        }
    }

    if (node_id < 0 && (flags & O_CREAT)) {
        node_id = ramfs_touch(0, target_name);
    }

    if (node_id < 0) {
        return -1;
    }

    int fd_slot = get_free_fd();
    if (fd_slot < 0) {
        return -1;
    }

    fd_table[fd_slot].open = true;
    fd_table[fd_slot].node_id = node_id;
    
    const ramfs_node_t *node = ramfs_get(node_id);
    if (node && (flags & O_APPEND)) {
        fd_table[fd_slot].offset = node->size;
    } else {
        fd_table[fd_slot].offset = 0;
    }

    return fd_slot + 3;
}

int _read(int file, char *ptr, int len) {
    if (file < 3) {
        return 0;
    }

    int idx = file - 3;
    if (idx >= MAX_OPEN_FILES || !fd_table[idx].open) {
        return -1;
    }

    int node_id = fd_table[idx].node_id;
    const ramfs_node_t *node = ramfs_get(node_id);
    if (!node || node->type != RAMFS_NODE_FILE) {
        return -1;
    }

    if (len <= 0) return 0;

    size_t available = node->size - fd_table[idx].offset;
    size_t to_read = ((size_t)len < available) ? (size_t)len : available;

    if (to_read <= 0) {
        return 0;
    }

    for (size_t i = 0; i < to_read; i++) {
        ptr[i] = node->data[fd_table[idx].offset + i];
    }

    fd_table[idx].offset += to_read;

    return to_read;
}

static char *empty_environ[] = { NULL };
char **environ = empty_environ;

/*
 * 3. _write: printf çıktısı basar veya dosyalara veri yazar.
 */
int _write(int file, char *ptr, int len) {
    if (file == 1 || file == 2) {
        for (int i = 0; i < len; i++) {
            if (ptr[i] == '\n') {
                cursor_x = 10;
                cursor_y += FONT_HEIGHT;
                continue;
            }
            if (ptr[i] == '\r') {
                cursor_x = 10;
                continue;
            }

            char temp_str[2] = { ptr[i], '\0' };
            graphics_draw_text(temp_str, cursor_x, cursor_y, TEXT_COLOR);
            cursor_x += FONT_WIDTH;

            if (cursor_x >= SCREEN_WIDTH - 10) {
                cursor_x = 10;
                cursor_y += FONT_HEIGHT;
            }
            if (cursor_y >= SCREEN_HEIGHT - FONT_HEIGHT) {
                cursor_x = 10;
                cursor_y = 10;
            }
        }
        return len;
    }

    if (file >= 3) {
        int idx = file - 3;
        if (idx >= MAX_OPEN_FILES || !fd_table[idx].open) {
            return -1;
        }

        int node_id = fd_table[idx].node_id;
        ramfs_node_t *node = (ramfs_node_t *)ramfs_get(node_id);
        if (!node || node->type != RAMFS_NODE_FILE) {
            return -1;
        }

        if (fd_table[idx].offset + len > RAMFS_FILE_CAPACITY) {
            return -1; 
        }

        for (int i = 0; i < len; i++) {
            node->data[fd_table[idx].offset + i] = ptr[i];
        }

        fd_table[idx].offset += len;

        if (fd_table[idx].offset > node->size) {
            node->size = fd_table[idx].offset;
        }
        node->data[node->size] = '\0';

        return len;
    }

    return -1;
}

/*
 * 4. _lseek: Dosya içi imleç hareketi.
 */
int _lseek(int file, int ptr, int dir) {
    if (file < 3) {
        return 0;
    }

    int idx = file - 3;
    if (idx >= MAX_OPEN_FILES || !fd_table[idx].open) {
        return -1;
    }

    int node_id = fd_table[idx].node_id;
    const ramfs_node_t *node = ramfs_get(node_id);
    if (!node) {
        return -1;
    }

    int new_offset = fd_table[idx].offset;

    switch (dir) {
        case SEEK_SET:
            new_offset = ptr;
            break;
        case SEEK_CUR:
            new_offset = fd_table[idx].offset + ptr;
            break;
        case SEEK_END:
            new_offset = node->size + ptr;
            break;
        default:
            return -1;
    }

    if (new_offset < 0 || new_offset > (int)node->size) {
        return -1;
    }

    fd_table[idx].offset = new_offset;
    return new_offset;
}

/*
 * 5. _close: Dosya kapatma.
 */
int _close(int file) {
    if (file < 3) {
        return -1;
    }

    int idx = file - 3;
    if (idx >= MAX_OPEN_FILES || !fd_table[idx].open) {
        return -1;
    }

    fd_table[idx].open = false;
    fd_table[idx].node_id = -1;
    fd_table[idx].offset = 0;
    return 0;
}

/*
 * 6. _sbrk: Dinamik Bellek Tahsisi (Page Fault Korumalı)
 */
caddr_t _sbrk(int incr) {
    extern char _end;
    static char *heap_end = 0;

    if (heap_end == 0) {
        heap_end = &_end;
    }

    uintptr_t current_addr = (uintptr_t)heap_end;
    if (current_addr & 3) {
        heap_end = (char *)((current_addr + 3) & ~3);
    }

    uint32_t mapped_limit = paging_identity_map_limit();
    uint32_t safe_limit = (mapped_limit > (1u * 1024u * 1024u))
        ? (mapped_limit - (1u * 1024u * 1024u))
        : mapped_limit;

    if ((uint32_t)((uintptr_t)heap_end + (uintptr_t)incr) > safe_limit) {
        opt_log("[SYS_SBRK] HATA: Heap, identity-map tavanina ulasti!\n");
        return (caddr_t) -1;
    }

    char *prev_heap = heap_end;
    heap_end += incr;
    return (caddr_t) prev_heap;
}

/*
 * 7. _fstat: Dosya Durumu Sorgulama
 */
int _fstat(int file, struct stat *st) {
    if (!st) return -1;

    if (file < 3) {
        st->st_mode = S_IFCHR;
        st->st_size = 0;
        return 0;
    }

    int idx = file - 3;
    if (idx >= MAX_OPEN_FILES || !fd_table[idx].open) {
        return -1;
    }

    int node_id = fd_table[idx].node_id;
    const ramfs_node_t *node = ramfs_get(node_id);
    if (!node) {
        return -1;
    }

    st->st_mode = S_IFREG;
    st->st_size = node->size;
    return 0;
}

int _isatty(int file) {
    if (file == 0 || file == 1 || file == 2) return 1;
    return 0;
}

void _exit(int status) {
    (void)status;
    while (1) {
        __asm__ __volatile__("cli; hlt");
    }
}

void kprintf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
}

int _getpid(void) { return 1; }
int _kill(int pid, int sig) { (void)pid; (void)sig; _exit(0); return -1; }

// POSIX Standart Tip Uyumluluğu
ssize_t write(int file, const void *ptr, size_t len) {
    return (ssize_t)_write(file, (char *)ptr, (int)len);
}

ssize_t read(int file, void *ptr, size_t len) {
    return (ssize_t)_read(file, (char *)ptr, (int)len);
}

int open(const char *name, int flags, ...) {
    return _open(name, flags);
}

int close(int file) {
    return _close(file);
}

off_t lseek(int file, off_t ptr, int dir) {
    return (off_t)_lseek(file, (int)ptr, dir);
}

void *sbrk(ptrdiff_t incr) {
    return (void *)_sbrk((int)incr);
}

int fstat(int file, struct stat *st) {
    return _fstat(file, st);
}

int isatty(int file) {
    return _isatty(file);
}

int getpid(void) {
    return _getpid();
}

int kill(int pid, int sig) {
    return _kill(pid, sig);
}

int __swrite64(void *cookie, const char *buf, int n) {
    return _write((int)(uintptr_t)cookie, (char *)buf, n);
}

int __sseek64(void *cookie, long long offset, int whence) {
    (void)cookie;
    (void)offset;
    (void)whence;
    return -1;
}

/* ==============================================================================
 * TCC POSIX MMAP KÖPRÜSÜ
 * ============================================================================== */
static void *mmap_common(size_t length) {
    if (length == 0) return (void *)-1;

    void *ptr = malloc(length);
    if (!ptr) {
        return (void *)-1;
    }

    uint8_t *b = (uint8_t *)ptr;
    for (size_t i = 0; i < length; i++) {
        b[i] = 0;
    }

    return ptr;
}

void *mmap(void *addr, size_t length, int prot, int flags, int fd, long offset) {
    (void)addr; (void)prot; (void)flags; (void)fd; (void)offset;
    return mmap_common(length);
}

void *mmap64(void *addr, size_t length, int prot, int flags, int fd, long long offset) {
    (void)addr; (void)prot; (void)flags; (void)fd; (void)offset;
    return mmap_common(length);
}

void *__mmap(void *addr, size_t length, int prot, int flags, int fd, long offset) {
    return mmap(addr, length, prot, flags, fd, offset);
}

void *__mmap64(void *addr, size_t length, int prot, int flags, int fd, long long offset) {
    return mmap64(addr, length, prot, flags, fd, offset);
}

int munmap(void *addr, size_t length) {
    (void)length;
    if (addr && addr != (void *)-1) free(addr);
    return 0;
}

int mprotect(void *addr, size_t len, int prot) {
    (void)addr; (void)len; (void)prot;
    return 0;
}

static const uint16_t days_before_month[12] = {
    0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334
};

static bool is_leap_year(uint16_t year) {
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

int gettimeofday(struct timeval *tv, void *tz) {
    (void)tz;
    if (!tv) return -1;

    rtc_time_t t;
    rtc_get_time(&t);

    uint16_t full_year = (t.year < 70) ? (uint16_t)(2000 + t.year) : (uint16_t)(1900 + t.year);

    uint32_t days = 0;
    for (uint16_t y = 1970; y < full_year; y++) {
        days += is_leap_year(y) ? 366 : 365;
    }
    days += days_before_month[(t.month >= 1 && t.month <= 12) ? t.month - 1 : 0];
    if (t.month > 2 && is_leap_year(full_year)) {
        days += 1;
    }
    days += (t.day > 0 ? t.day - 1 : 0);

    uint32_t seconds = days * 86400u + (uint32_t)t.hour * 3600u +
                        (uint32_t)t.minute * 60u + (uint32_t)t.second;

    tv->tv_sec = (long)seconds;
    tv->tv_usec = 0;
    return 0;
}
