#ifndef KERNEL_VFS_DRIVER_H
#define KERNEL_VFS_DRIVER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define VFS_MAX_NAME_LEN 64
#define VFS_MAX_PATH_LEN 128

typedef enum {
    VFS_NODE_FILE = 0,
    VFS_NODE_DIR  = 1
} vfs_node_type_t;

typedef struct {
    char name[VFS_MAX_NAME_LEN];
    vfs_node_type_t type;
    uint32_t size;
} vfs_stat_t;

typedef struct {
    char name[VFS_MAX_NAME_LEN];
    vfs_node_type_t type;
    uint32_t size;
} vfs_dir_entry_t;

/* Sürücü Operasyon Fonksiyon İşaretçileri Tablosu */
typedef struct vfs_driver {
    const char *fs_name;

    int   (*mount)(const char *target);
    int   (*unmount)(void);
    int   (*open)(const char *path, int mode);
    int   (*close)(int driver_fd);
    int   (*read)(int driver_fd, void *buf, size_t count, uint32_t offset);
    int   (*write)(int driver_fd, const void *buf, size_t count, uint32_t offset);
    int   (*mkdir)(const char *path);
    int   (*rmdir)(const char *path);
    int   (*delete)(const char *path);
    bool  (*exists)(const char *path);
    int   (*list)(const char *path, vfs_dir_entry_t *entries, int max_entries);
    int   (*rename)(const char *oldpath, const char *newpath);
    int   (*stat)(const char *path, vfs_stat_t *st);
} vfs_driver_t;

#endif /* KERNEL_VFS_DRIVER_H */