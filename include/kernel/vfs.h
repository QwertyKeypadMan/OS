#ifndef KERNEL_VFS_H
#define KERNEL_VFS_H

#include "kernel/vfs_driver.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define VFS_MAX_OPEN_FILES 32
#define VFS_MAX_MOUNTS     8

/* VFS Genel API */
void vfs_init(void);

int  vfs_mount(const char *mount_point, vfs_driver_t *driver);
int  vfs_unmount(const char *mount_point);

int  vfs_open(const char *path, int mode);
int  vfs_close(int fd);
int  vfs_read(int fd, void *buf, size_t count);
int  vfs_write(int fd, const void *buf, size_t count);

int  vfs_create(const char *path);
int  vfs_delete(const char *path);
bool vfs_exists(const char *path);

int  vfs_mkdir(const char *path);
int  vfs_rmdir(const char *path);
int  vfs_list(const char *path, vfs_dir_entry_t *entries, int max_entries);

int  vfs_rename(const char *oldpath, const char *newpath);
int  vfs_copy(const char *src, const char *dst);
int  vfs_move(const char *src, const char *dst);

int  vfs_stat(const char *path, vfs_stat_t *st);

#endif /* KERNEL_VFS_H */