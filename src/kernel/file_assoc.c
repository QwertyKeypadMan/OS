#include "kernel/file_assoc.h"
#include "kernel/notepad.h"
#include "kernel/kstring.h"

static file_association_t g_associations[16];
static int g_assoc_count = 0;

void file_assoc_register(const char *ext, file_open_handler_t handler) {
    if (g_assoc_count < 16) {
        g_associations[g_assoc_count].extension = ext;
        g_associations[g_assoc_count].handler = handler;
        g_assoc_count++;
    }
}

void file_assoc_init(void) {
    g_assoc_count = 0;

    /* İlk Sürüm: Metin Tabanlı Uzantıların Notepad'e Yönlendirilmesi */
    file_assoc_register(".txt", notepad_open);
    file_assoc_register(".cfg", notepad_open);
    file_assoc_register(".log", notepad_open);
    file_assoc_register(".ini", notepad_open);
	file_assoc_register("", notepad_open);
}

bool file_association_open(const char *path) {
    if (!path) return false;

    int len = (int)k_strlen(path);
    int dot_pos = -1;

    for (int i = len - 1; i >= 0; i--) {
        if (path[i] == '.') { dot_pos = i; break; }
        if (path[i] == '/') break;
    }

    if (dot_pos < 0) return false;

    const char *ext = &path[dot_pos];

    for (int i = 0; i < g_assoc_count; i++) {
        if (k_streq(ext, g_associations[i].extension)) {
            g_associations[i].handler(path);
            return true;
        }
    }

    return false;
}