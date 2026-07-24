#include "kernel/kstring.h"
#include <stdint.h>
#include <stdbool.h>

/* ---- HIZLI BELLEK OPERASYONLARI ---- */

void *k_memset(void *dest, int value, size_t count)
{
    uint8_t *bytes = dest;
    uint8_t v8 = (uint8_t)value;

    size_t words = count / sizeof(uint32_t);
    if (words > 0) {
        uint32_t v32 = (uint32_t)v8 | ((uint32_t)v8 << 8) | ((uint32_t)v8 << 16) | ((uint32_t)v8 << 24);
        uint32_t *dw = (uint32_t *)bytes;
        for (size_t i = 0; i < words; i++) {
            dw[i] = v32;
        }
    }

    size_t consumed = words * sizeof(uint32_t);
    for (size_t i = consumed; i < count; i++) {
        bytes[i] = v8;
    }

    return dest;
}

void *k_memcpy(void *dest, const void *src, size_t count)
{
    uint8_t *to = dest;
    const uint8_t *from = src;

    size_t words = count / sizeof(uint32_t);
    if (words > 0) {
        uint32_t *dw = (uint32_t *)to;
        const uint32_t *sw = (const uint32_t *)from;
        for (size_t i = 0; i < words; i++) {
            dw[i] = sw[i];
        }
    }

    size_t consumed = words * sizeof(uint32_t);
    for (size_t i = consumed; i < count; i++) {
        to[i] = from[i];
    }

    return dest;
}

void *k_memmove(void *dest, const void *src, size_t count)
{
    uint8_t *to = dest;
    const uint8_t *from = src;

    if (to == from || count == 0) {
        return dest;
    }

    if (to < from) {
        size_t words = count / sizeof(uint32_t);
        if (words > 0) {
            uint32_t *dw = (uint32_t *)to;
            const uint32_t *sw = (const uint32_t *)from;
            for (size_t i = 0; i < words; i++) {
                dw[i] = sw[i];
            }
        }
        size_t consumed = words * sizeof(uint32_t);
        for (size_t i = consumed; i < count; i++) {
            to[i] = from[i];
        }
    } else {
        size_t words = count / sizeof(uint32_t);
        size_t consumed = words * sizeof(uint32_t);

        for (size_t i = count; i > consumed; i--) {
            to[i - 1] = from[i - 1];
        }

        if (words > 0) {
            uint32_t *dw = (uint32_t *)to;
            const uint32_t *sw = (const uint32_t *)from;
            for (size_t i = words; i > 0; i--) {
                dw[i - 1] = sw[i - 1];
            }
        }
    }

    return dest;
}

int k_memcmp(const void *s1, const void *s2, size_t n)
{
    const unsigned char *p1 = s1;
    const unsigned char *p2 = s2;
    for (size_t i = 0; i < n; i++) {
        if (p1[i] != p2[i]) {
            return p1[i] - p2[i];
        }
    }
    return 0;
}

/* ---- METİN (STRING) FONKSİYONLARI ---- */

size_t k_strlen(const char *str)
{
    size_t length = 0;
    while (str[length] != '\0') {
        length++;
    }
    return length;
}

int k_strcmp(const char *left, const char *right)
{
    while (*left != '\0' && *left == *right) {
        left++;
        right++;
    }
    return (unsigned char)*left - (unsigned char)*right;
}

int k_strncmp(const char *left, const char *right, size_t count)
{
    for (size_t i = 0; i < count; i++) {
        unsigned char lc = (unsigned char)left[i];
        unsigned char rc = (unsigned char)right[i];
        if (lc != rc || lc == '\0' || rc == '\0') {
            return lc - rc;
        }
    }
    return 0;
}

char *k_strcpy(char *dest, const char *src)
{
    char *start = dest;
    while ((*dest++ = *src++) != '\0') {
    }
    return start;
}

size_t k_strlcpy(char *dest, const char *src, size_t size)
{
    size_t src_length = k_strlen(src);

    if (size != 0) {
        size_t copy_length = src_length;
        if (copy_length >= size) {
            copy_length = size - 1;
        }
        k_memcpy(dest, src, copy_length);
        dest[copy_length] = '\0';
    }

    return src_length;
}

bool k_streq(const char *left, const char *right)
{
    return k_strcmp(left, right) == 0;
}

bool k_isspace(char ch)
{
    return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r';
}

bool k_isalpha(char ch)
{
    return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z');
}

char k_toupper(char ch)
{
    if (ch >= 'a' && ch <= 'z') {
        return (char)(ch - ('a' - 'A'));
    }
    return ch;
}

char *k_strchr(const char *s, int c)
{
    while (*s != (char)c) {
        if (!*s) {
            return NULL;
        }
        s++;
    }
    return (char *)s;
}

char *k_strrchr(const char *s, int c)
{
    char *last = NULL;
    do {
        if (*s == (char)c) {
            last = (char *)s;
        }
    } while (*s++);
    return last;
}

char *k_strstr(const char *haystack, const char *needle)
{
    if (!*needle) {
        return (char *)haystack;
    }
    for (; *haystack; haystack++) {
        if (*haystack == *needle) {
            const char *h = haystack;
            const char *n = needle;
            while (*h && *n && *h == *n) {
                h++;
                n++;
            }
            if (!*n) {
                return (char *)haystack;
            }
        }
    }
    return NULL;
}

/* ---- STATİK KERNEL HEAP ALTYAPISI ---- */

#define KERNEL_HEAP_SIZE (16 * 1024 * 1024) // 16 MB Arena Heap (Rahat çalışması için büyütüldü)
static unsigned char kernel_heap[KERNEL_HEAP_SIZE] __attribute__((aligned(16)));
static size_t heap_offset = 0;

void *k_malloc(size_t size)
{
    if (size == 0) return NULL;

    // Strict 16-byte alignment (Lua nesneleri ve SSE talimatları için şarttır)
    size = (size + 15) & ~15;

    if (heap_offset + size > KERNEL_HEAP_SIZE) {
        return NULL; // Bellek Bitti
    }

    void *ptr = &kernel_heap[heap_offset];
    heap_offset += size;
    return ptr;
}

void k_free(void *ptr)
{
    (void)ptr;
}

// Eski boyutu açıkça alan GÜVENLİ realloc
void *k_realloc_sized(void *ptr, size_t old_size, size_t new_size)
{
    if (!ptr) return k_malloc(new_size);
    if (new_size == 0) return NULL;

    void *new_ptr = k_malloc(new_size);
    if (new_ptr) {
        size_t copy_size = (old_size < new_size) ? old_size : new_size;
        k_memcpy(new_ptr, ptr, copy_size);
    }
    return new_ptr;
}

size_t k_strlcat(char *dest, const char *src, size_t size)
{
    size_t dest_length = k_strlen(dest);
    size_t src_length = k_strlen(src);

    if (dest_length >= size) {
        return size + src_length;
    }

    size_t copy_length = size - dest_length - 1;
    if (copy_length > src_length) {
        copy_length = src_length;
    }

    k_memcpy(dest + dest_length, src, copy_length);
    dest[dest_length + copy_length] = '\0';

    return dest_length + src_length;
}

/* ---- KMALLOC / LUA KÖPRÜLERİ ---- */

void *kmalloc(size_t size) {
    return k_malloc(size);
}

void* kmalloc_aligned(size_t size, size_t alignment) {
    if (alignment == 0 || (alignment & (alignment - 1)) != 0) {
        return NULL; /* Alignment 2'nin kuvveti değilse iptal et */
    }

    /* Adres işaretçisi (pointer) saklamak için fazladan yer ayırıyoruz */
    size_t total_size = size + alignment + sizeof(void*);
    void* raw_ptr = kmalloc(total_size);

    if (!raw_ptr) return NULL;

    /* Hizalanmış adresi hesapla */
    uintptr_t raw_addr = (uintptr_t)raw_ptr + sizeof(void*);
    uintptr_t aligned_addr = (raw_addr + (alignment - 1)) & ~(alignment - 1);

    /* Orijinal ham adresi hizalanmış alanın hemen öncesine sakla (kfree için) */
    ((void**)aligned_addr)[-1] = raw_ptr;

    return (void*)aligned_addr;
}

void *krealloc(void *ptr, size_t size) {
    // Eski boyut bilinmıyorsa tahmini olarak 0 verilmez, ancak k_realloc_sized kullanmak esastır.
    return k_realloc_sized(ptr, 0, size);
}

// LUA'YA VERİLECEK ALLOCATOR FONKSİYONU
void *kayaos_lua_alloc(void *ud, void *ptr, size_t osize, size_t nsize) {
    (void)ud;
    if (nsize == 0) {
        k_free(ptr);
        return NULL;
    }
    return k_realloc_sized(ptr, osize, nsize);
}