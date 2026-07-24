#include "kernel/ramfs_driver.h"
#include "kernel/ramfs.h"
#include "kernel/kstring.h"

#define MAX_RAMFS_OPEN_FILES 16

typedef struct {
    bool in_use;
    int node_id;
} ramfs_fd_entry_t;

static ramfs_fd_entry_t g_ramfs_fds[MAX_RAMFS_OPEN_FILES];

static int ramfs_drv_mount(const char *target) {
    (void)target;
    for (int i = 0; i < MAX_RAMFS_OPEN_FILES; i++) {
        g_ramfs_fds[i].in_use = false;
        g_ramfs_fds[i].node_id = -1;
    }
    return 0;
}

static int ramfs_drv_unmount(void) {
    return 0;
}

static int ramfs_drv_open(const char *path, int mode) {
    (void)mode;
    int root = ramfs_root();
    int node_id = ramfs_resolve(root, path);

    /* Dosya yoksa ve yazma modundaysa oluştur (örnek mantık) */
    if (node_id < 0 && mode == 1) {
        /* RAMFS dosya oluşturma mantığı bağlama */
        node_id = ramfs_resolve(root, path); 
    }

    if (node_id < 0) return -1;

    for (int i = 0; i < MAX_RAMFS_OPEN_FILES; i++) {
        if (!g_ramfs_fds[i].in_use) {
            g_ramfs_fds[i].in_use = true;
            g_ramfs_fds[i].node_id = node_id;
            return i;
        }
    }
    return -1;
}

static int ramfs_drv_close(int driver_fd) {
    if (driver_fd < 0 || driver_fd >= MAX_RAMFS_OPEN_FILES) return -1;
    g_ramfs_fds[driver_fd].in_use = false;
    g_ramfs_fds[driver_fd].node_id = -1;
    return 0;
}

static int ramfs_drv_read(int driver_fd, void *buf, size_t count, uint32_t offset) {
    if (driver_fd < 0 || driver_fd >= MAX_RAMFS_OPEN_FILES || !g_ramfs_fds[driver_fd].in_use) {
        return -1;
    }

    const ramfs_node_t *node = ramfs_get(g_ramfs_fds[driver_fd].node_id);
    if (!node || node->type != RAMFS_NODE_FILE) return -1;

    if (offset >= node->size) return 0;

    size_t available = node->size - offset;
    size_t to_read = (count < available) ? count : available;

    k_memcpy(buf, (const char *)node->data + offset, to_read);
    return (int)to_read;
}

static int ramfs_drv_write(int driver_fd, const void *buf, size_t count, uint32_t offset) {
    if (driver_fd < 0 || driver_fd >= MAX_RAMFS_OPEN_FILES || !g_ramfs_fds[driver_fd].in_use) {
        return -1;
    }

    /* Mevcut RAMFS yapınızda dinamik büyütme veya var olan tampona yazma */
    ramfs_node_t *node = (ramfs_node_t *)ramfs_get(g_ramfs_fds[driver_fd].node_id);
    if (!node || node->type != RAMFS_NODE_FILE) return -1;

    if (offset + count > 4096) { /* Sabit RAMFS tampon boyutu sınırı */
        count = 4096 - offset;
    }

    k_memcpy((char *)node->data + offset, buf, count);
    if (offset + count > node->size) {
        node->size = offset + count;
    }

    return (int)count;
}

static bool ramfs_drv_exists(const char *path) {
    int root = ramfs_root();
    int node_id = ramfs_resolve(root, path);
    return node_id >= 0;
}

static int ramfs_drv_stat(const char *path, vfs_stat_t *st) {
    if (!st) return -1;

    int root = ramfs_root();
    int node_id = ramfs_resolve(root, path);
    if (node_id < 0) return -1;

    const ramfs_node_t *node = ramfs_get(node_id);
    if (!node) return -1;

    k_strlcpy(st->name, node->name, sizeof(st->name));
    st->size = node->size;
    st->type = (node->type == RAMFS_NODE_DIR) ? VFS_NODE_DIR : VFS_NODE_FILE;

    return 0;
}

static int ramfs_drv_list(const char *path, vfs_dir_entry_t *entries, int max_entries) {
    int root = ramfs_root();
    int node_id = ramfs_resolve(root, path);
    if (node_id < 0) return -1;

    const ramfs_node_t *dir_node = ramfs_get(node_id);
    if (!dir_node || dir_node->type != RAMFS_NODE_DIR) return -1;

    int count = 0;
    int child = dir_node->first_child;

    while (child >= 0 && count < max_entries) {
        const ramfs_node_t *cnode = ramfs_get(child);
        if (!cnode) break;

        k_strlcpy(entries[count].name, cnode->name, sizeof(entries[count].name));
        entries[count].size = cnode->size;
        entries[count].type = (cnode->type == RAMFS_NODE_DIR) ? VFS_NODE_DIR : VFS_NODE_FILE;

        count++;
        child = cnode->next_sibling;
    }

    return count;
}

static vfs_driver_t g_ramfs_driver = {
    .fs_name = "ramfs",
    .mount   = ramfs_drv_mount,
    .unmount = ramfs_drv_unmount,
    .open    = ramfs_drv_open,
    .close   = ramfs_drv_close,
    .read    = ramfs_drv_read,
    .write   = ramfs_drv_write,
    .mkdir   = NULL,
    .rmdir   = NULL,
    .delete  = NULL,
    .exists  = ramfs_drv_exists,
    .list    = ramfs_drv_list,
    .rename  = NULL,
    .stat    = ramfs_drv_stat
};

vfs_driver_t *ramfs_get_driver(void) {
    return &g_ramfs_driver;
}