
#include "kernel/ramfs.h"
 
#include "kernel/kstring.h"
 
static ramfs_node_t nodes[RAMFS_MAX_NODES];
 
static bool valid_node(int node_id)
{
    return node_id >= 0 && node_id < RAMFS_MAX_NODES && nodes[node_id].used;
}
 
static int read_component(const char **cursor, char *out)
{
    const char *p = *cursor;
    while (*p == '/') {
        p++;
    }
 
    if (*p == '\0') {
        *cursor = p;
        out[0] = '\0';
        return 0;
    }
 
    size_t length = 0;
    while (p[length] != '\0' && p[length] != '/') {
        if (length >= RAMFS_NAME_MAX) {
            return RAMFS_ERR_BAD_NAME;
        }
        out[length] = p[length];
        length++;
    }
 
    out[length] = '\0';
    *cursor = p + length;
    return 1;
}
 
static bool bad_name(const char *name)
{
    return name[0] == '\0' || k_streq(name, ".") || k_streq(name, "..");
}
 
int ramfs_find_child(int parent, const char *name)
{
    if (!valid_node(parent) || nodes[parent].type != RAMFS_NODE_DIR) {
        return RAMFS_ERR_NOT_DIR;
    }
 
    int child = nodes[parent].first_child;
    while (child >= 0) {
        if (k_streq(nodes[child].name, name)) {
            return child;
        }
        child = nodes[child].next_sibling;
    }
 
    return RAMFS_ERR_NOT_FOUND;
}
 
static void link_child(int parent, int child)
{
    nodes[child].next_sibling = -1;
 
    if (nodes[parent].first_child < 0) {
        nodes[parent].first_child = child;
        return;
    }
 
    int cursor = nodes[parent].first_child;
    while (nodes[cursor].next_sibling >= 0) {
        cursor = nodes[cursor].next_sibling;
    }
    nodes[cursor].next_sibling = child;
}
 
static int create_node(int parent, ramfs_node_type_t type, const char *name)
{
    if (!valid_node(parent) || nodes[parent].type != RAMFS_NODE_DIR) {
        return RAMFS_ERR_NOT_DIR;
    }
 
    if (bad_name(name) || k_strlen(name) > RAMFS_NAME_MAX) {
        return RAMFS_ERR_BAD_NAME;
    }
 
    if (ramfs_find_child(parent, name) >= 0) {
        return RAMFS_ERR_EXISTS;
    }
 
    for (int i = 0; i < RAMFS_MAX_NODES; i++) {
        if (!nodes[i].used) {
            nodes[i].used = true;
            nodes[i].type = type;
            k_strlcpy(nodes[i].name, name, sizeof(nodes[i].name));
            nodes[i].parent = parent;
            nodes[i].first_child = -1;
            nodes[i].next_sibling = -1;
            nodes[i].size = 0;
            nodes[i].data[0] = '\0';
            link_child(parent, i);
            return i;
        }
    }
 
    return RAMFS_ERR_FULL;
}
 
static int parent_for_path(int cwd, const char *path, char *basename)
{
    if (path == 0 || path[0] == '\0') {
        return RAMFS_ERR_BAD_NAME;
    }
 
    int current = path[0] == '/' ? 0 : cwd;
    const char *cursor = path;
    char component[RAMFS_NAME_MAX + 1];
    bool saw_component = false;
 
    for (;;) {
        int result = read_component(&cursor, component);
        if (result < 0) {
            return result;
        }
 
        if (result == 0) {
            return saw_component ? RAMFS_ERR_BAD_NAME : RAMFS_ERR_BAD_NAME;
        }
 
        saw_component = true;
 
        const char *lookahead = cursor;
        while (*lookahead == '/') {
            lookahead++;
        }
        bool is_last = *lookahead == '\0';
 
        if (is_last) {
            if (bad_name(component)) {
                return RAMFS_ERR_BAD_NAME;
            }
            k_strlcpy(basename, component, RAMFS_NAME_MAX + 1);
            return current;
        }
 
        if (k_streq(component, ".")) {
            continue;
        }
 
        if (k_streq(component, "..")) {
            current = nodes[current].parent;
            continue;
        }
 
        int child = ramfs_find_child(current, component);
        if (child < 0) {
            return child;
        }
 
        if (nodes[child].type != RAMFS_NODE_DIR) {
            return RAMFS_ERR_NOT_DIR;
        }
 
        current = child;
    }
}
 
void ramfs_init(void)
{
    for (int i = 0; i < RAMFS_MAX_NODES; i++) {
        nodes[i].used = false;
        nodes[i].first_child = -1;
        nodes[i].next_sibling = -1;
    }
 
    nodes[0].used = true;
    nodes[0].type = RAMFS_NODE_DIR;
    k_strlcpy(nodes[0].name, "/", sizeof(nodes[0].name));
    nodes[0].parent = 0;
    nodes[0].first_child = -1;
    nodes[0].next_sibling = -1;
    nodes[0].size = 0;
    nodes[0].data[0] = '\0';
 
    ramfs_mkdir(0, "/bin");
    ramfs_mkdir(0, "/etc");
    ramfs_mkdir(0, "/home");
    ramfs_mkdir(0, "/tmp");
    ramfs_mkdir(0, "/var");
    ramfs_write_file(0, "/README.txt",
        "Welcome to KayaOS.\n"
        "Try: help, ls, cat /etc/motd, mkdir /tmp/demo, write /tmp/demo/note hello\n",
        false);
    ramfs_write_file(0, "/etc/motd",
        "KayaOS boots into a tiny shell backed by a writable RAM filesystem.\n",
        false);
    ramfs_write_file(0, "/test.op",
        "# KayaOS operation program\n"
        "echo Running test.op\n"
        "echo This message came from a .op program file.\n"
        "write /tmp/test-output.txt test.op created this file\n"
        "cat /tmp/test-output.txt\n",
        false);
 
    ramfs_mkdir(0, "/desktop");
 
    ramfs_write_file(0, "/desktop/test.opa",
		"WINDOW Ornek Uygulama\n"
		"TEXT Merhaba KayaOS!\n"
		"BUTTON Tikla Bana\n"
		"TEXTBOX Metin giriniz...\n"
		"NOTIFY \"Çizim dosyası yüklendi\"\n",
        false);
}
int ramfs_root(void)
{
    return 0;
}
 
const ramfs_node_t *ramfs_get(int node_id)
{
    if (!valid_node(node_id)) {
        return 0;
    }
    return &nodes[node_id];
}
 
int ramfs_resolve(int cwd, const char *path)
{
    if (!valid_node(cwd) || nodes[cwd].type != RAMFS_NODE_DIR) {
        return RAMFS_ERR_NOT_DIR;
    }
 
    if (path == 0 || path[0] == '\0') {
        return cwd;
    }
 
    int current = path[0] == '/' ? 0 : cwd;
    const char *cursor = path;
    char component[RAMFS_NAME_MAX + 1];
 
    for (;;) {
        int result = read_component(&cursor, component);
        if (result < 0) {
            return result;
        }
 
        if (result == 0) {
            return current;
        }
 
        if (k_streq(component, ".")) {
            continue;
        }
 
        if (k_streq(component, "..")) {
            current = nodes[current].parent;
            continue;
        }
 
        if (nodes[current].type != RAMFS_NODE_DIR) {
            return RAMFS_ERR_NOT_DIR;
        }
 
        int child = ramfs_find_child(current, component);
        if (child < 0) {
            return child;
        }
 
        current = child;
    }
}
 
int ramfs_mkdir(int cwd, const char *path)
{
    char name[RAMFS_NAME_MAX + 1];
    int parent = parent_for_path(cwd, path, name);
    if (parent < 0) {
        return parent;
    }
    return create_node(parent, RAMFS_NODE_DIR, name);
}
 
int ramfs_touch(int cwd, const char *path)
{
    char name[RAMFS_NAME_MAX + 1];
    int parent = parent_for_path(cwd, path, name);
    if (parent < 0) {
        return parent;
    }
 
    int existing = ramfs_find_child(parent, name);
    if (existing >= 0) {
        return nodes[existing].type == RAMFS_NODE_FILE ? existing : RAMFS_ERR_IS_DIR;
    }
 
    return create_node(parent, RAMFS_NODE_FILE, name);
}
 
int ramfs_write_file(int cwd, const char *path, const char *data, bool append)
{
    char name[RAMFS_NAME_MAX + 1];
    int parent = parent_for_path(cwd, path, name);
    if (parent < 0) {
        return parent;
    }
 
    int file = ramfs_find_child(parent, name);
    if (file == RAMFS_ERR_NOT_FOUND) {
        file = create_node(parent, RAMFS_NODE_FILE, name);
    }
 
    if (file < 0) {
        return file;
    }
 
    if (nodes[file].type != RAMFS_NODE_FILE) {
        return RAMFS_ERR_IS_DIR;
    }
 
    size_t input_size = k_strlen(data);
    if (append) {
        if (nodes[file].size + input_size > RAMFS_FILE_CAPACITY) {
            return RAMFS_ERR_TOO_BIG;
        }
        k_memcpy(nodes[file].data + nodes[file].size, data, input_size);
        nodes[file].size += input_size;
        nodes[file].data[nodes[file].size] = '\0';
        return file;
    }
 
    if (input_size > RAMFS_FILE_CAPACITY) {
        return RAMFS_ERR_TOO_BIG;
    }
 
    k_memcpy(nodes[file].data, data, input_size);
    nodes[file].size = input_size;
    nodes[file].data[nodes[file].size] = '\0';
    return file;
}
 
int ramfs_remove(int cwd, const char *path)
{
    int target = ramfs_resolve(cwd, path);
    if (target < 0) {
        return target;
    }
 
    if (target == 0) {
        return RAMFS_ERR_ROOT;
    }
 
    if (nodes[target].type == RAMFS_NODE_DIR && nodes[target].first_child >= 0) {
        return RAMFS_ERR_NOT_EMPTY;
    }
 
    int parent = nodes[target].parent;
    int child = nodes[parent].first_child;
    int previous = -1;
 
    while (child >= 0) {
        if (child == target) {
            if (previous < 0) {
                nodes[parent].first_child = nodes[child].next_sibling;
            } else {
                nodes[previous].next_sibling = nodes[child].next_sibling;
            }
 
            k_memset(&nodes[child], 0, sizeof(nodes[child]));
            return 0;
        }
 
        previous = child;
        child = nodes[child].next_sibling;
    }
 
    return RAMFS_ERR_NOT_FOUND;
}
 
static void append_text(char *out, size_t out_size, const char *text)
{
    size_t used = k_strlen(out);
    if (used >= out_size) {
        return;
    }
    k_strlcpy(out + used, text, out_size - used);
}
 
void ramfs_path(int node_id, char *out, size_t out_size)
{
    if (out_size == 0) {
        return;
    }
 
    out[0] = '\0';
 
    if (!valid_node(node_id)) {
        k_strlcpy(out, "?", out_size);
        return;
    }
 
    if (node_id == 0) {
        k_strlcpy(out, "/", out_size);
        return;
    }
 
    int chain[RAMFS_MAX_NODES];
    int count = 0;
    int cursor = node_id;
 
    while (cursor != 0 && count < RAMFS_MAX_NODES) {
        chain[count++] = cursor;
        cursor = nodes[cursor].parent;
    }
 
    for (int i = count - 1; i >= 0; i--) {
        append_text(out, out_size, "/");
        append_text(out, out_size, nodes[chain[i]].name);
    }
}
 
const char *ramfs_error(int error)
{
    switch (error) {
    case RAMFS_ERR_NOT_FOUND:
        return "not found";
    case RAMFS_ERR_EXISTS:
        return "already exists";
    case RAMFS_ERR_FULL:
        return "filesystem is full";
    case RAMFS_ERR_BAD_NAME:
        return "bad name";
    case RAMFS_ERR_NOT_DIR:
        return "not a directory";
    case RAMFS_ERR_IS_DIR:
        return "is a directory";
    case RAMFS_ERR_NOT_EMPTY:
        return "directory is not empty";
    case RAMFS_ERR_TOO_BIG:
        return "file is too large";
    case RAMFS_ERR_ROOT:
        return "cannot remove root";
    default:
        return "unknown error";
    }
}
 
size_t ramfs_used_nodes(void)
{
    size_t used = 0;
    for (int i = 0; i < RAMFS_MAX_NODES; i++) {
        if (nodes[i].used) {
            used++;
        }
    }
    return used;
}
 
size_t ramfs_total_nodes(void)
{
    return RAMFS_MAX_NODES;
}
 
size_t ramfs_used_bytes(void)
{
    size_t used = 0;
    for (int i = 0; i < RAMFS_MAX_NODES; i++) {
        if (nodes[i].used && nodes[i].type == RAMFS_NODE_FILE) {
            used += nodes[i].size;
        }
    }
    return used;
}
 
size_t ramfs_total_bytes(void)
{
    return RAMFS_MAX_NODES * RAMFS_FILE_CAPACITY;
}
 
