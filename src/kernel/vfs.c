#include "kernel/vfs.h"
#include "kernel/kstring.h"
#include "kernel/block/block.h"

typedef struct {
    char mount_point[VFS_MAX_PATH_LEN];
    vfs_driver_t *driver;
    bool active;
} vfs_mount_entry_t;

typedef struct {
    bool in_use;
    vfs_driver_t *driver;
    int driver_fd;
    uint32_t offset;
    int mode;
    char path[VFS_MAX_PATH_LEN];
} vfs_file_handle_t;

static vfs_mount_entry_t g_mounts[VFS_MAX_MOUNTS];
static vfs_file_handle_t g_fd_table[VFS_MAX_OPEN_FILES];

/* Yol çözümleme: En uzun uyan Mount Point'i bulur */
static vfs_mount_entry_t *vfs_resolve_mount(const char *path, const char **rel_path) {
    if (!path) return NULL;

    int best_match_len = -1;
    vfs_mount_entry_t *best_mount = NULL;

    for (int i = 0; i < VFS_MAX_MOUNTS; i++) {
        if (!g_mounts[i].active) continue;

        size_t mlen = k_strlen(g_mounts[i].mount_point);

        /* Kök dizin kontrolü ("/") */
        if (mlen == 1 && g_mounts[i].mount_point[0] == '/') {
            if (best_match_len < 1) {
                best_match_len = 1;
                best_mount = &g_mounts[i];
                if (rel_path) *rel_path = path;
            }
            continue;
        }

        /* Alt dizin mount noktaları kontrolü (Örn: "/disk") */
        if (k_strncmp(path, g_mounts[i].mount_point, mlen) == 0) {
            if (path[mlen] == '\0' || path[mlen] == '/') {
                if ((int)mlen > best_match_len) {
                    best_match_len = (int)mlen;
                    best_mount = &g_mounts[i];
                    if (rel_path) {
                        *rel_path = (path[mlen] == '\0') ? "/" : &path[mlen];
                    }
                }
            }
        }
    }

    return best_mount;
}

void vfs_init(void) {
    for (int i = 0; i < VFS_MAX_MOUNTS; i++) {
        g_mounts[i].active = false;
        g_mounts[i].driver = NULL;
        g_mounts[i].mount_point[0] = '\0';
    }

    for (int i = 0; i < VFS_MAX_OPEN_FILES; i++) {
        g_fd_table[i].in_use = false;
        g_fd_table[i].driver = NULL;
        g_fd_table[i].driver_fd = -1;
        g_fd_table[i].offset = 0;
    }
}

int vfs_mount(const char *mount_point, vfs_driver_t *driver) {
    if (!mount_point || !driver) return -1;

    for (int i = 0; i < VFS_MAX_MOUNTS; i++) {
        if (!g_mounts[i].active) {
            k_strlcpy(g_mounts[i].mount_point, mount_point, sizeof(g_mounts[i].mount_point));
            g_mounts[i].driver = driver;
            g_mounts[i].active = true;

            if (driver->mount) {
                driver->mount(mount_point);
            }
            return 0;
        }
    }
    return -1;
}

int vfs_unmount(const char *mount_point) {
    if (!mount_point) return -1;

    for (int i = 0; i < VFS_MAX_MOUNTS; i++) {
        if (g_mounts[i].active && k_streq(g_mounts[i].mount_point, mount_point)) {
            if (g_mounts[i].driver && g_mounts[i].driver->unmount) {
                g_mounts[i].driver->unmount();
            }
            g_mounts[i].active = false;
            return 0;
        }
    }
    return -1;
}

int vfs_open(const char *path, int mode) {
    const char *rel_path = NULL;
    vfs_mount_entry_t *mnt = vfs_resolve_mount(path, &rel_path);
    if (!mnt || !mnt->driver || !mnt->driver->open) return -1;

    int drv_fd = mnt->driver->open(rel_path, mode);
    if (drv_fd < 0) return -1;

    for (int i = 0; i < VFS_MAX_OPEN_FILES; i++) {
        if (!g_fd_table[i].in_use) {
            g_fd_table[i].in_use = true;
            g_fd_table[i].driver = mnt->driver;
            g_fd_table[i].driver_fd = drv_fd;
            g_fd_table[i].offset = 0;
            g_fd_table[i].mode = mode;
            k_strlcpy(g_fd_table[i].path, path, sizeof(g_fd_table[i].path));
            return i;
        }
    }

    mnt->driver->close(drv_fd);
    return -1;
}

int vfs_close(int fd) {
    if (fd < 0 || fd >= VFS_MAX_OPEN_FILES || !g_fd_table[fd].in_use) return -1;

    if (g_fd_table[fd].driver && g_fd_table[fd].driver->close) {
        g_fd_table[fd].driver->close(g_fd_table[fd].driver_fd);
    }

    g_fd_table[fd].in_use = false;
    g_fd_table[fd].driver = NULL;
    g_fd_table[fd].driver_fd = -1;
    g_fd_table[fd].offset = 0;
    return 0;
}

int vfs_read(int fd, void *buf, size_t count) {
    if (fd < 0 || fd >= VFS_MAX_OPEN_FILES || !g_fd_table[fd].in_use) return -1;

    vfs_file_handle_t *h = &g_fd_table[fd];
    if (!h->driver || !h->driver->read) return -1;

    int bytes_read = h->driver->read(h->driver_fd, buf, count, h->offset);
    if (bytes_read > 0) {
        h->offset += (uint32_t)bytes_read;
    }
    return bytes_read;
}

int vfs_write(int fd, const void *buf, size_t count) {
    if (fd < 0 || fd >= VFS_MAX_OPEN_FILES || !g_fd_table[fd].in_use) return -1;

    vfs_file_handle_t *h = &g_fd_table[fd];
    if (!h->driver || !h->driver->write) return -1;

    int bytes_written = h->driver->write(h->driver_fd, buf, count, h->offset);
    if (bytes_written > 0) {
        h->offset += (uint32_t)bytes_written;
    }
    return bytes_written;
}

int vfs_read_block(uint32_t dev_id, uint64_t lba, uint32_t count, uint8_t* buffer) {
    block_device_t* dev = block_get_by_id(dev_id);
    if (!dev) {
        /* Aygıt ID verilmediyse varsayılan primary aygıtı kullan */
        dev = block_get_primary();
    }

    if (!dev) return -1;

    return block_read(dev, lba, count, buffer) ? 0 : -1;
}

int vfs_write_block(uint32_t dev_id, uint64_t lba, uint32_t count, const uint8_t* buffer) {
    block_device_t* dev = block_get_by_id(dev_id);
    if (!dev) {
        dev = block_get_primary();
    }

    if (!dev) return -1;

    return block_write(dev, lba, count, buffer) ? 0 : -1;
}

int vfs_create(const char *path) {
    int fd = vfs_open(path, 1);
    if (fd >= 0) {
        vfs_close(fd);
        return 0;
    }
    return -1;
}

int vfs_delete(const char *path) {
    const char *rel_path = NULL;
    vfs_mount_entry_t *mnt = vfs_resolve_mount(path, &rel_path);
    if (!mnt || !mnt->driver || !mnt->driver->delete) return -1;

    return mnt->driver->delete(rel_path);
}

bool vfs_exists(const char *path) {
    const char *rel_path = NULL;
    vfs_mount_entry_t *mnt = vfs_resolve_mount(path, &rel_path);
    if (!mnt || !mnt->driver || !mnt->driver->exists) return false;

    return mnt->driver->exists(rel_path);
}

int vfs_mkdir(const char *path) {
    const char *rel_path = NULL;
    vfs_mount_entry_t *mnt = vfs_resolve_mount(path, &rel_path);
    if (!mnt || !mnt->driver || !mnt->driver->mkdir) return -1;

    return mnt->driver->mkdir(rel_path);
}

int vfs_rmdir(const char *path) {
    const char *rel_path = NULL;
    vfs_mount_entry_t *mnt = vfs_resolve_mount(path, &rel_path);
    if (!mnt || !mnt->driver || !mnt->driver->rmdir) return -1;

    return mnt->driver->rmdir(rel_path);
}

int vfs_list(const char *path, vfs_dir_entry_t *entries, int max_entries) {
    const char *rel_path = NULL;
    vfs_mount_entry_t *mnt = vfs_resolve_mount(path, &rel_path);
    if (!mnt || !mnt->driver || !mnt->driver->list) return -1;

    return mnt->driver->list(rel_path, entries, max_entries);
}

int vfs_rename(const char *oldpath, const char *newpath) {
    const char *rel_old = NULL;
    const char *rel_new = NULL;

    vfs_mount_entry_t *mnt1 = vfs_resolve_mount(oldpath, &rel_old);
    vfs_mount_entry_t *mnt2 = vfs_resolve_mount(newpath, &rel_new);

    if (!mnt1 || mnt1 != mnt2 || !mnt1->driver || !mnt1->driver->rename) return -1;

    return mnt1->driver->rename(rel_old, rel_new);
}

int vfs_stat(const char *path, vfs_stat_t *st) {
    const char *rel_path = NULL;
    vfs_mount_entry_t *mnt = vfs_resolve_mount(path, &rel_path);
    if (!mnt || !mnt->driver || !mnt->driver->stat) return -1;

    return mnt->driver->stat(rel_path, st);
}

int vfs_copy(const char *src, const char *dst) {
    int fsrc = vfs_open(src, 0);
    if (fsrc < 0) return -1;

    int fdst = vfs_open(dst, 1);
    if (fdst < 0) {
        vfs_close(fsrc);
        return -1;
    }

    char buffer[256];
    int bytes;

    while ((bytes = vfs_read(fsrc, buffer, sizeof(buffer))) > 0) {
        vfs_write(fdst, buffer, (size_t)bytes);
    }

    vfs_close(fsrc);
    vfs_close(fdst);
    return 0;
}

int vfs_move(const char *src, const char *dst) {
    if (vfs_rename(src, dst) == 0) {
        return 0;
    }

    if (vfs_copy(src, dst) == 0) {
        return vfs_delete(src);
    }

    return -1;
}