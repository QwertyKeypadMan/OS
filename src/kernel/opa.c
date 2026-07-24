#include "kernel/opa.h"
#include "kernel/ramfs.h"
#include "kernel/gui.h"
#include "kernel/kstring.h"

#include <stddef.h>
#include <stdlib.h> /* Newlib malloc/free */

/* opa_lang.c icindeki GUI Script calistiricisi */
extern bool opa_run_gui_script(const char *script_data);

bool opa_run(int cwd, const char *path) {
    int target = ramfs_resolve(cwd, path);
    if (target < 0) {
        gui_notify("opa: dosya bulunamadi");
        return false;
    }

    const ramfs_node_t *node = ramfs_get(target);
    if (node == 0 || node->type != RAMFS_NODE_FILE) {
        gui_notify("opa: gecersiz dosya");
        return false;
    }

    /* .opa dosyasinin icerigi oku ve GUI Script motoruna gonder */
    char *source = (char *)malloc(node->size + 1);
    if (source == 0) {
        gui_notify("opa: bellek yetersiz");
        return false;
    }
    k_memcpy(source, node->data, node->size);
    source[node->size] = '\0';

    bool ok = opa_run_gui_script(source);

    free(source);
    return ok;
}