#include "kernel/shell.h"
#include <stdbool.h>
#include <stdint.h>

#include "kernel/io.h"
#include "kernel/gui.h"
#include "kernel/keyboard.h"
#include "kernel/kstring.h"
#include "kernel/ramfs.h"
#include "kernel/terminal.h"
#include "kernel/version.h"

#define SHELL_LINE_MAX 256
#define SHELL_RUN_DEPTH_MAX 4

static int cwd;
static int run_depth;

// graphics_present prototipi ( graphics.h içinden de gelebilir )
void graphics_present(void);

static char *skip_spaces(char *cursor)
{
    while (*cursor != '\0' && k_isspace(*cursor)) {
        cursor++;
    }
    return cursor;
}



static char *next_token(char **cursor)
{
    char *start = skip_spaces(*cursor);
    if (*start == '\0') {
        *cursor = start;
        return 0;
    }

    char *end = start;
    while (*end != '\0' && !k_isspace(*end)) {
        end++;
    }

    if (*end != '\0') {
        *end = '\0';
        end++;
    }

    *cursor = end;
    return start;
}

static void print_error(const char *subject, int error)
{
    terminal_setcolor(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
    terminal_writestring(subject);
    terminal_writestring(": ");
    terminal_writestring(ramfs_error(error));
    terminal_writestring("\n");
    terminal_setcolor(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    
    // Hata mesajı basıldıktan sonra ekranı yenile
    if (graphics_available()) graphics_present();
}

static void print_prompt(void)
{
    char path[256];
    ramfs_path(cwd, path, sizeof(path));

    terminal_setcolor(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    terminal_writestring(KAYAOS_NAME);
    terminal_setcolor(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    terminal_writestring(" ");
    terminal_setcolor(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    terminal_writestring(path);
    terminal_setcolor(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    terminal_writestring(" $ ");

    // Prompt bittiğinde TEK SEFERDE ekrana basıyoruz. Yazı geçişleri pürüzsüz olur!
    if (graphics_available()) {
        graphics_present();
    }
}

static void read_line(char *buffer, size_t capacity)
{
    size_t length = 0;

    for (;;) {
        char ch = keyboard_read_char();

        if (ch == '\n') {
            terminal_putchar('\n');
            buffer[length] = '\0';
            // Satır bittiğinde present çağrısı zaten terminal_putchar içindeki \n kontrolüyle tetikleniyor
            return;
        }

        if (ch == '\b') {
            if (length > 0) {
                length--;
                terminal_putchar('\b');
                // Karakter silindiği an ekranda hemen kaybolması için tetikliyoruz
                if (graphics_available()) {
                    graphics_present();
                }
            }
            continue;
        }

        if (ch >= 32 && ch <= 126 && length + 1 < capacity) {
            buffer[length++] = ch;
            terminal_putchar(ch);
            // Kullanıcı klavyede harfe bastığı an ekrana anında (ama sadece o hücre için) yansısın!
            if (graphics_available()) {
                graphics_present();
            }
        }
    }
}

static void cmd_help(void)
{
    terminal_writestring("Commands:\n");
    terminal_writestring("  help                  show this help\n");
    terminal_writestring("  clear                 clear the screen\n");
    terminal_writestring("  version               show kernel version\n");
    terminal_writestring("  pwd                   print current directory\n");
    terminal_writestring("  ls [path]             list directory or file\n");
    terminal_writestring("  cd [path]             change directory\n");
    terminal_writestring("  cat <file>            print file contents\n");
    terminal_writestring("  touch <file>          create empty file\n");
    terminal_writestring("  mkdir <dir>           create directory\n");
    terminal_writestring("  write <file> <text>   replace file contents\n");
    terminal_writestring("  append <file> <txt>   append to file\n");
    terminal_writestring("  echo <text>           print text\n");
    terminal_writestring("  run <file.op>         execute an .op program\n");
    terminal_writestring("  gui                   open the text-mode GUI\n");
    terminal_writestring("  rm <path>             remove empty dir or file\n");
    terminal_writestring("  tree [path]           print filesystem tree\n");
    terminal_writestring("  mem                   show RAMFS usage\n");
    terminal_writestring("  reboot                reboot via keyboard controller\n");
    terminal_writestring("  halt                  stop CPU\n");
}

static void cmd_version(void)
{
    terminal_writestring(KAYAOS_NAME " " KAYAOS_VERSION " (" KAYAOS_ARCH ")\n");
}

static void cmd_pwd(void)
{
    char path[256];
    ramfs_path(cwd, path, sizeof(path));
    terminal_writestring(path);
    terminal_putchar('\n');
}

static void print_node_line(int node_id)
{
    const ramfs_node_t *node = ramfs_get(node_id);
    if (node == 0) {
        return;
    }

    if (node->type == RAMFS_NODE_DIR) {
        terminal_setcolor(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
        terminal_writestring(node->name);
        terminal_writestring("/");
        terminal_setcolor(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
        terminal_putchar('\n');
        return;
    }

    terminal_writestring(node->name);
    terminal_writestring("  ");
    terminal_write_dec((uint32_t)node->size);
    terminal_writestring(" bytes\n");
}

static void cmd_ls(char *path)
{
    int target = path == 0 ? cwd : ramfs_resolve(cwd, path);
    if (target < 0) {
        print_error("ls", target);
        return;
    }

    const ramfs_node_t *node = ramfs_get(target);
    if (node == 0) {
        print_error("ls", RAMFS_ERR_NOT_FOUND);
        return;
    }

    if (node->type == RAMFS_NODE_FILE) {
        print_node_line(target);
        return;
    }

    int child = node->first_child;
    while (child >= 0) {
        print_node_line(child);
        const ramfs_node_t *child_node = ramfs_get(child);
        child = child_node == 0 ? -1 : child_node->next_sibling;
    }
}

static void cmd_cd(char *path)
{
    int target = path == 0 ? ramfs_root() : ramfs_resolve(cwd, path);
    if (target < 0) {
        print_error("cd", target);
        return;
    }

    const ramfs_node_t *node = ramfs_get(target);
    if (node == 0 || node->type != RAMFS_NODE_DIR) {
        print_error("cd", RAMFS_ERR_NOT_DIR);
        return;
    }

    cwd = target;
}

static void cmd_cat(char *path)
{
    if (path == 0) {
        terminal_writestring("cat: missing file\n");
        return;
    }

    int target = ramfs_resolve(cwd, path);
    if (target < 0) {
        print_error("cat", target);
        return;
    }

    const ramfs_node_t *node = ramfs_get(target);
    if (node == 0) {
        print_error("cat", RAMFS_ERR_NOT_FOUND);
        return;
    }

    if (node->type != RAMFS_NODE_FILE) {
        print_error("cat", RAMFS_ERR_IS_DIR);
        return;
    }

    terminal_write(node->data, node->size);
    if (node->size == 0 || node->data[node->size - 1] != '\n') {
        terminal_putchar('\n');
    }
}

static void cmd_touch(char *path)
{
    if (path == 0) {
        terminal_writestring("touch: missing file\n");
        return;
    }

    int result = ramfs_touch(cwd, path);
    if (result < 0) {
        print_error("touch", result);
    }
}

static void cmd_mkdir(char *path)
{
    if (path == 0) {
        terminal_writestring("mkdir: missing directory\n");
        return;
    }

    int result = ramfs_mkdir(cwd, path);
    if (result < 0) {
        print_error("mkdir", result);
    }
}

static void cmd_write(char *path, char *text, bool append)
{
    if (path == 0) {
        terminal_writestring(append ? "append: missing file\n" : "write: missing file\n");
        return;
    }

    text = skip_spaces(text);
    int result = ramfs_write_file(cwd, path, text, append);
    if (result < 0) {
        print_error(append ? "append" : "write", result);
    }
}

extern char* ramfs_read_file(const char* filename);

static void cmd_echo(char *text)
{
    text = skip_spaces(text);
    terminal_writestring(text);
    terminal_putchar('\n');
}

static bool has_op_extension(const char *path)
{
    size_t length = k_strlen(path);
    return length >= 3 && k_streq(path + length - 3, ".op");
}

static void cmd_rm(char *path)
{
    if (path == 0) {
        terminal_writestring("rm: missing path\n");
        return;
    }

    int result = ramfs_remove(cwd, path);
    if (result < 0) {
        print_error("rm", result);
    }
}

static void tree_recursive(int node_id, int depth)
{
    const ramfs_node_t *node = ramfs_get(node_id);
    if (node == 0) {
        return;
    }

    for (int i = 0; i < depth; i++) {
        terminal_writestring("  ");
    }

    if (node_id == ramfs_root()) {
        terminal_writestring("/\n");
    } else {
        terminal_writestring(node->name);
        if (node->type == RAMFS_NODE_DIR) {
            terminal_writestring("/");
        }
        terminal_putchar('\n');
    }

    if (node->type != RAMFS_NODE_DIR) {
        return;
    }

    int child = node->first_child;
    while (child >= 0) {
        tree_recursive(child, depth + 1);
        const ramfs_node_t *child_node = ramfs_get(child);
        child = child_node == 0 ? -1 : child_node->next_sibling;
    }
}

static void cmd_tree(char *path)
{
    int target = path == 0 ? cwd : ramfs_resolve(cwd, path);
    if (target < 0) {
        print_error("tree", target);
        return;
    }
    tree_recursive(target, 0);
    if (graphics_available()) graphics_present();
}

static void cmd_mem(void)
{
    terminal_writestring("RAMFS nodes: ");
    terminal_write_dec((uint32_t)ramfs_used_nodes());
    terminal_writestring("/");
    terminal_write_dec((uint32_t)ramfs_total_nodes());
    terminal_writestring("\nRAMFS bytes: ");
    terminal_write_dec((uint32_t)ramfs_used_bytes());
    terminal_writestring("/");
    terminal_write_dec((uint32_t)ramfs_total_bytes());
    terminal_putchar('\n');
}

static void execute_line(char *line);

static void cmd_run(char *path)
{
    if (path == 0) {
        terminal_writestring("run: missing program\n");
        return;
    }

    if (!has_op_extension(path)) {
        terminal_writestring("run: program files must end with .op\n");
        return;
    }

    if (run_depth >= SHELL_RUN_DEPTH_MAX) {
        terminal_writestring("run: nested program limit reached\n");
        return;
    }

    int target = ramfs_resolve(cwd, path);
    if (target < 0) {
        print_error("run", target);
        return;
    }

    const ramfs_node_t *node = ramfs_get(target);
    if (node == 0) {
        print_error("run", RAMFS_ERR_NOT_FOUND);
        return;
    }

    if (node->type != RAMFS_NODE_FILE) {
        print_error("run", RAMFS_ERR_IS_DIR);
        return;
    }

    terminal_setcolor(VGA_COLOR_LIGHT_BROWN, VGA_COLOR_BLACK);
    terminal_writestring("[run] ");
    terminal_writestring(path);
    terminal_putchar('\n');
    terminal_setcolor(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    if (graphics_available()) graphics_present();

    run_depth++;

    size_t offset = 0;
    while (offset < node->size) {
        char line[SHELL_LINE_MAX];
        size_t length = 0;

        while (offset < node->size && node->data[offset] != '\n' && length + 1 < sizeof(line)) {
            line[length++] = node->data[offset++];
        }

        while (offset < node->size && node->data[offset] != '\n') {
            offset++;
        }

        if (offset < node->size && node->data[offset] == '\n') {
            offset++;
        }

        line[length] = '\0';

        char *command = skip_spaces(line);
        if (command[0] == '\0' || command[0] == '#') {
            continue;
        }

        terminal_setcolor(VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
        terminal_writestring("op> ");
        terminal_writestring(command);
        terminal_putchar('\n');
        terminal_setcolor(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
        if (graphics_available()) graphics_present();

        execute_line(command);
    }

    run_depth--;
}

static void cmd_reboot(void)
{
    terminal_writestring("Rebooting...\n");
    if (graphics_available()) graphics_present();
    outb(0x64, 0xFE);
    for (;;) {
        __asm__ __volatile__("hlt");
    }
}

static void cmd_halt(void)
{
    terminal_writestring("System halted.\n");
    if (graphics_available()) graphics_present();
    for (;;) {
        __asm__ __volatile__("cli; hlt");
    }
}

static void execute_line(char *line)
{
    char *cursor = line;
    char *command = next_token(&cursor);

    if (command == 0) {
        return;
    }

    if (k_streq(command, "help")) {
        cmd_help();
    } else if (k_streq(command, "clear")) {
        terminal_clear();
    } else if (k_streq(command, "version")) {
        cmd_version();
    } else if (k_streq(command, "pwd")) {
        cmd_pwd();
    } else if (k_streq(command, "ls")) {
        cmd_ls(next_token(&cursor));
    } else if (k_streq(command, "cd")) {
        cmd_cd(next_token(&cursor));
    } else if (k_streq(command, "cat")) {
        cmd_cat(next_token(&cursor));
    } else if (k_streq(command, "touch")) {
        cmd_touch(next_token(&cursor));
    } else if (k_streq(command, "mkdir")) {
        cmd_mkdir(next_token(&cursor));
    } else if (k_streq(command, "write")) {
        char *path = next_token(&cursor);
        cmd_write(path, cursor, false);
    } else if (k_streq(command, "append")) {
        char *path = next_token(&cursor);
        cmd_write(path, cursor, true);
    } else if (k_streq(command, "echo")) {
        cmd_echo(cursor);
    } else if (k_streq(command, "run")) {
        cmd_run(next_token(&cursor));
    } else if (k_streq(command, "rm")) {
        cmd_rm(next_token(&cursor));
    } else if (k_streq(command, "tree")) {
        cmd_tree(next_token(&cursor));
    } else if (k_streq(command, "mem")) {
        cmd_mem();
    } else if (k_streq(command, "reboot")) {
        cmd_reboot();
    } else if (k_streq(command, "halt")) {
        cmd_halt();
    } else {
        terminal_writestring(command);
        terminal_writestring(": command not found\n");
    }
}

void shell_run(void)
{
    char line[SHELL_LINE_MAX];
    cwd = ramfs_root();
    run_depth = 0;

    terminal_writestring("Type 'help' to list commands.\n\n");
    if (graphics_available()) graphics_present();

    for (;;) {
        print_prompt();
        read_line(line, sizeof(line));
        execute_line(line);
    }
}