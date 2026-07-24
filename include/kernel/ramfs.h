#ifndef KERNEL_RAMFS_H
#define KERNEL_RAMFS_H

#include <stddef.h>
#include <stdbool.h>

#define RAMFS_MAX_NODES 128
#define RAMFS_NAME_MAX 31
#define RAMFS_FILE_CAPACITY 1024

#define RAMFS_ERR_NOT_FOUND -1
#define RAMFS_ERR_EXISTS -2
#define RAMFS_ERR_FULL -3
#define RAMFS_ERR_BAD_NAME -4
#define RAMFS_ERR_NOT_DIR -5
#define RAMFS_ERR_IS_DIR -6
#define RAMFS_ERR_NOT_EMPTY -7
#define RAMFS_ERR_TOO_BIG -8
#define RAMFS_ERR_ROOT -9

typedef enum {
    RAMFS_NODE_FILE = 1,
    RAMFS_NODE_DIR = 2
} ramfs_node_type_t;

typedef struct {
    bool used;
    ramfs_node_type_t type;
    char name[RAMFS_NAME_MAX + 1];
    int parent;
    int first_child;
    int next_sibling;
    size_t size;
    char data[RAMFS_FILE_CAPACITY + 1];
} ramfs_node_t;

void ramfs_init(void);
int ramfs_root(void);
const ramfs_node_t *ramfs_get(int node_id);
int ramfs_resolve(int cwd, const char *path);
int ramfs_mkdir(int cwd, const char *path);
int ramfs_touch(int cwd, const char *path);
int ramfs_write_file(int cwd, const char *path, const char *data, bool append);
int ramfs_remove(int cwd, const char *path);
int ramfs_find_child(int parent, const char *name);
void ramfs_path(int node_id, char *out, size_t out_size);
const char *ramfs_error(int error);
size_t ramfs_used_nodes(void);
size_t ramfs_total_nodes(void);
size_t ramfs_used_bytes(void);
size_t ramfs_total_bytes(void);

#endif

