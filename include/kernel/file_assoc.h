#ifndef FILE_ASSOC_H
#define FILE_ASSOC_H
#include <stdbool.h>

typedef void (*file_open_handler_t)(const char *path);

typedef struct {
    const char *extension;
    file_open_handler_t handler;
} file_association_t;

void file_assoc_init(void);
bool file_association_open(const char *path);

#endif