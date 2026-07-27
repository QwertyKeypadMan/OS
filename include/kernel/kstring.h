#ifndef KERNEL_KSTRING_H
#define KERNEL_KSTRING_H

#include <stddef.h>
#include <stdbool.h>

void *k_memset(void *dest, int value, size_t count);
void *k_memcpy(void *dest, const void *src, size_t count);
void *k_memmove(void *dest, const void *src, size_t count);
size_t k_strlen(const char *str);
int k_strcmp(const char *left, const char *right);
int k_strncmp(const char *left, const char *right, size_t count);
char *k_strcpy(char *dest, const char *src);
size_t k_strlcpy(char *dest, const char *src, size_t size);
bool k_streq(const char *left, const char *right);
void sse_memcpy_fast(void *dest, const void *src, size_t n);
bool k_isspace(char ch);
bool k_isalpha(char ch);
char k_toupper(char ch);
void *k_malloc(size_t size);
void k_free(void *ptr);
double floor(double x);
double ceil(double x);

#endif
