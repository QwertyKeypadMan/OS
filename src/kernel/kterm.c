#include "kernel/gui.h"
#include "kernel/graphics.h"
#include "kernel/ramfs.h"
#include "kernel/kstring.h"
#include "kernel/keyboard.h"
#include "kernel/version.h"
#include "kernel/apic.h"
#include "kernel/io.h"
#include <stdint.h>
#include <stdbool.h>

#define TERM_ROWS 12
#define TERM_COLS 50
#define INPUT_MAX 128

/* Renkli Satır Yapısı */
typedef struct {
    char text[TERM_COLS];
    uint32_t color;
} kterm_line_t;

typedef struct {
    int win_id;
    kterm_line_t lines[TERM_ROWS];
    char input_buf[INPUT_MAX];
    int input_len;
    int current_dir_node;
    char cwd_path[128];
} kterm_state_t;

static kterm_state_t g_term;
static bool g_term_active = false;
static int g_run_depth = 0; /* .op betik çalıştırma derinlik sınırı için */

/* Satır Ekleme (Renk Destekli) */
static void kterm_append_line(const char *text, uint32_t color) {
    for (int i = 0; i < TERM_ROWS - 1; i++) {
        g_term.lines[i] = g_term.lines[i + 1];
    }
    k_strlcpy(g_term.lines[TERM_ROWS - 1].text, text, TERM_COLS);
    g_term.lines[TERM_ROWS - 1].color = color;
}

/* Sayı Formatlama Yardımcısı */
static void u32_to_str(uint32_t val, char *buf) {
    if (val == 0) {
        buf[0] = '0';
        buf[1] = '\0';
        return;
    }
    char temp[12];
    int i = 0;
    while (val > 0) {
        temp[i++] = '0' + (val % 10);
        val /= 10;
    }
    int j = 0;
    while (i > 0) {
        buf[j++] = temp[--i];
    }
    buf[j] = '\0';
}

/* Uzantı Kontrolü (.op kontrolü için) */
static bool has_op_extension(const char *path) {
    size_t length = k_strlen(path);
    return length >= 3 && k_streq(path + length - 3, ".op");
}

/* Özyinelemeli Ağaç Yapısı Yazdırma (tree komutu için) */
static void kterm_tree_recursive(int node_id, int depth) {
    const ramfs_node_t *node = ramfs_get(node_id);
    if (!node) return;

    char line[TERM_COLS];
    int indent = depth * 2;
    if (indent >= TERM_COLS - 4) indent = TERM_COLS - 4;

    for (int i = 0; i < indent; i++) {
        line[i] = ' ';
    }
    line[indent] = '\0';

    if (node_id == ramfs_root()) {
        k_strlcat(line, "/", sizeof(line));
        kterm_append_line(line, graphics_rgb(120, 200, 255));
    } else {
        k_strlcat(line, node->name, sizeof(line));
        if (node->type == RAMFS_NODE_DIR) {
            k_strlcat(line, "/", sizeof(line));
            kterm_append_line(line, graphics_rgb(120, 200, 255));
        } else {
            kterm_append_line(line, graphics_rgb(220, 220, 220));
        }
    }

    if (node->type != RAMFS_NODE_DIR) return;

    int child = node->first_child;
    while (child >= 0) {
        kterm_tree_recursive(child, depth + 1);
        const ramfs_node_t *child_node = ramfs_get(child);
        child = child_node ? child_node->next_sibling : -1;
    }
}

/* Komut Ayrıştırıcı ve Yürütücü */
static void kterm_dispatch_cmd(const char *input_str) {
    /* Renk Tanımları */
    uint32_t col_text   = graphics_rgb(220, 225, 235);  /* Genel Metin */
    uint32_t col_info   = graphics_rgb(100, 180, 255);  /* Mavi Bilgi */
    uint32_t col_warn   = graphics_rgb(255, 200, 80);   /* Turuncu Uyarı */
    uint32_t col_error  = graphics_rgb(255, 85, 85);    /* Kırmızı Hata */

    char cmd[32];
    char arg[96];
    cmd[0] = '\0';
    arg[0] = '\0';

    int i = 0;
    while (input_str[i] == ' ') i++;
    int c = 0;
    while (input_str[i] != '\0' && input_str[i] != ' ' && c < 31) {
        cmd[c++] = input_str[i++];
    }
    cmd[c] = '\0';

    while (input_str[i] == ' ') i++;
    int a = 0;
    while (input_str[i] != '\0' && a < 95) {
        arg[a++] = input_str[i++];
    }
    arg[a] = '\0';

    if (cmd[0] == '\0') return;

    /* KOMUT MANTIĞI */
    if (k_streq(cmd, "help")) {
        kterm_append_line("Komutlar:", col_info);
        kterm_append_line("help, clear, version, pwd, cd, ls, cat, run", col_info);
        kterm_append_line("touch, mkdir, write, append, echo, rm", col_info);
        kterm_append_line("tree, mem, reboot, halt, exit", col_info);
    } else if (k_streq(cmd, "clear")) {
        for (int r = 0; r < TERM_ROWS; r++) {
            g_term.lines[r].text[0] = '\0';
        }
    } else if (k_streq(cmd, "version")) {
        kterm_append_line(KAYAOS_NAME " " KAYAOS_VERSION " (" KAYAOS_ARCH ")", col_info);
    } else if (k_streq(cmd, "pwd")) {
        kterm_append_line(g_term.cwd_path, col_info);
    } else if (k_streq(cmd, "erdalhoca")) {
        kterm_append_line("KSPAR QWERTY TRI0X DAVU ", col_info);
    } else if (k_streq(cmd, "cd")) {
        if (arg[0] == '\0' || k_streq(arg, "/") || k_streq(arg, "~")) {
            g_term.current_dir_node = ramfs_root();
            k_strlcpy(g_term.cwd_path, "/", sizeof(g_term.cwd_path));
        } else if (k_streq(arg, "..")) {
            if (!k_streq(g_term.cwd_path, "/")) {
                int len = (int)k_strlen(g_term.cwd_path);
                int last_slash = -1;
                for (int s = len - 1; s >= 0; s--) {
                    if (g_term.cwd_path[s] == '/') {
                        last_slash = s;
                        break;
                    }
                }
                if (last_slash > 0) {
                    g_term.cwd_path[last_slash] = '\0';
                } else {
                    k_strlcpy(g_term.cwd_path, "/", sizeof(g_term.cwd_path));
                }

                int parent_node = ramfs_resolve(ramfs_root(), g_term.cwd_path);
                g_term.current_dir_node = (parent_node >= 0) ? parent_node : ramfs_root();
            }
        } else {
            int target_node = (arg[0] == '/') 
                ? ramfs_resolve(ramfs_root(), arg) 
                : ramfs_resolve(g_term.current_dir_node, arg);

            if (target_node >= 0) {
                const ramfs_node_t *tnode = ramfs_get(target_node);
                if (tnode && tnode->type == RAMFS_NODE_DIR) {
                    g_term.current_dir_node = target_node;
                    if (arg[0] == '/') {
                        k_strlcpy(g_term.cwd_path, arg, sizeof(g_term.cwd_path));
                    } else {
                        if (!k_streq(g_term.cwd_path, "/")) {
                            k_strlcat(g_term.cwd_path, "/", sizeof(g_term.cwd_path));
                        }
                        k_strlcat(g_term.cwd_path, arg, sizeof(g_term.cwd_path));
                    }
                } else {
                    kterm_append_line("Hata: Hedef bir dizin degil.", col_error);
                }
            } else {
                kterm_append_line("Dizin bulunamadi.", col_error);
            }
        }
    } else if (k_streq(cmd, "ls")) {
        int dir = (arg[0] != '\0') ? ramfs_resolve(g_term.current_dir_node, arg) : g_term.current_dir_node;
        if (dir < 0) {
            kterm_append_line("Dizin bulunamadi.", col_error);
        } else {
            const ramfs_node_t *node = ramfs_get(dir);
            if (node && node->type == RAMFS_NODE_DIR) {
                int child = node->first_child;
                if (child < 0) {
                    kterm_append_line("(Dizin bos)", graphics_rgb(140, 140, 150));
                }
                while (child >= 0) {
                    const ramfs_node_t *cnode = ramfs_get(child);
                    if (cnode) {
                        uint32_t item_color = (cnode->type == RAMFS_NODE_DIR) 
                                               ? graphics_rgb(120, 200, 255)  
                                               : graphics_rgb(220, 220, 220); 
                        kterm_append_line(cnode->name, item_color);
                    }
                    child = cnode->next_sibling;
                }
            }
        }
    } else if (k_streq(cmd, "cat")) {
        if (arg[0] == '\0') {
            kterm_append_line("Kullanim: cat <dosya>", col_warn);
        } else {
            int target = ramfs_resolve(g_term.current_dir_node, arg);
            if (target < 0) {
                kterm_append_line("Dosya bulunamadi.", col_error);
            } else {
                const ramfs_node_t *node = ramfs_get(target);
                if (!node || node->type != RAMFS_NODE_FILE) {
                    kterm_append_line("Hata: Gecerli bir dosya degil.", col_error);
                } else if (node->size == 0) {
                    kterm_append_line("(Dosya bos)", graphics_rgb(140, 140, 150));
                } else {
                    size_t offset = 0;
                    while (offset < node->size) {
                        char line_buf[TERM_COLS];
                        size_t l_idx = 0;
                        while (offset < node->size && node->data[offset] != '\n' && l_idx < TERM_COLS - 1) {
                            line_buf[l_idx++] = node->data[offset++];
                        }
                        while (offset < node->size && node->data[offset] != '\n') {
                            offset++;
                        }
                        if (offset < node->size && node->data[offset] == '\n') {
                            offset++;
                        }
                        line_buf[l_idx] = '\0';
                        kterm_append_line(line_buf, col_text);
                    }
                }
            }
        }
    } else if (k_streq(cmd, "run")) {
        if (arg[0] == '\0') {
            kterm_append_line("Kullanim: run <program.op>", col_warn);
        } else if (!has_op_extension(arg)) {
            kterm_append_line("Hata: Program .op uzantili olmalidir.", col_error);
        } else if (g_run_depth >= 4) {
            kterm_append_line("Hata: Ic ice program siniri asildi.", col_error);
        } else {
            int target = ramfs_resolve(g_term.current_dir_node, arg);
            if (target < 0) {
                kterm_append_line("Program bulunamadi.", col_error);
            } else {
                const ramfs_node_t *node = ramfs_get(target);
                if (!node || node->type != RAMFS_NODE_FILE) {
                    kterm_append_line("Hata: Gecerli bir dosya degil.", col_error);
                } else {
                    char run_msg[TERM_COLS];
                    k_strlcpy(run_msg, "[run] ", sizeof(run_msg));
                    k_strlcat(run_msg, arg, sizeof(run_msg));
                    kterm_append_line(run_msg, col_warn);

                    g_run_depth++;
                    size_t offset = 0;
                    while (offset < node->size) {
                        char line[128];
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

                        /* Başındaki boşlukları geç */
                        char *cursor = line;
                        while (*cursor != '\0' && k_isspace(*cursor)) {
                            cursor++;
                        }

                        /* Boş satır veya yorum satırını atla */
                        if (*cursor == '\0' || *cursor == '#') {
                            continue;
                        }

                        /* Ekrana 'op> komut' yazısını ekle */
                        char op_line[TERM_COLS];
                        k_strlcpy(op_line, "op> ", sizeof(op_line));
                        k_strlcat(op_line, cursor, sizeof(op_line));
                        kterm_append_line(op_line, graphics_rgb(140, 140, 150));

                        /* Komutu çalıştır */
                        kterm_dispatch_cmd(cursor);
                    }
                    g_run_depth--;
                }
            }
        }
    } else if (k_streq(cmd, "mkdir")) {
        if (arg[0] != '\0') {
            ramfs_mkdir(g_term.current_dir_node, arg);
            kterm_append_line("Dizin olusturuldu.", col_info);
        } else {
            kterm_append_line("Kullanim: mkdir <dizin_adi>", col_warn);
        }
    } else if (k_streq(cmd, "touch")) {
        if (arg[0] != '\0') {
            ramfs_touch(g_term.current_dir_node, arg);
            kterm_append_line("Dosya olusturuldu.", col_info);
        } else {
            kterm_append_line("Kullanim: touch <dosya_adi>", col_warn);
        }
    } else if (k_streq(cmd, "write") || k_streq(cmd, "append")) {
        bool is_append = k_streq(cmd, "append");
        char file_name[32];
        char write_text[96];
        file_name[0] = '\0';
        write_text[0] = '\0';

        int idx = 0;
        while (arg[idx] == ' ') idx++;
        int f_idx = 0;
        while (arg[idx] != '\0' && arg[idx] != ' ' && f_idx < 31) {
            file_name[f_idx++] = arg[idx++];
        }
        file_name[f_idx] = '\0';

        while (arg[idx] == ' ') idx++;
        int t_idx = 0;
        while (arg[idx] != '\0' && t_idx < 95) {
            write_text[t_idx++] = arg[idx++];
        }
        write_text[t_idx] = '\0';

        if (file_name[0] == '\0') {
            kterm_append_line(is_append ? "Kullanim: append <dosya> <metin>" : "Kullanim: write <dosya> <metin>", col_warn);
        } else {
            int res = ramfs_write_file(g_term.current_dir_node, file_name, write_text, is_append);
            if (res < 0) {
                kterm_append_line("Yazma hatasi.", col_error);
            } else {
                kterm_append_line("Dosyaya yazildi.", col_info);
            }
        }
    } else if (k_streq(cmd, "echo")) {
        kterm_append_line(arg, col_text);
    } else if (k_streq(cmd, "rm")) {
        if (arg[0] == '\0') {
            kterm_append_line("Kullanim: rm <dosya_veya_dizin>", col_warn);
        } else {
            int res = ramfs_remove(g_term.current_dir_node, arg);
            if (res < 0) {
                kterm_append_line("Silme hatasi.", col_error);
            } else {
                kterm_append_line("Silindi.", col_info);
            }
        }
    } else if (k_streq(cmd, "tree")) {
        int target = (arg[0] != '\0') ? ramfs_resolve(g_term.current_dir_node, arg) : g_term.current_dir_node;
        if (target < 0) {
            kterm_append_line("Dizin bulunamadi.", col_error);
        } else {
            kterm_tree_recursive(target, 0);
        }
    } else if (k_streq(cmd, "mem")) {
        char buf[TERM_COLS];
        char num[16];

        k_strlcpy(buf, "Nodes: ", sizeof(buf));
        u32_to_str((uint32_t)ramfs_used_nodes(), num);
        k_strlcat(buf, num, sizeof(buf));
        k_strlcat(buf, "/", sizeof(buf));
        u32_to_str((uint32_t)ramfs_total_nodes(), num);
        k_strlcat(buf, num, sizeof(buf));
        kterm_append_line(buf, col_info);

        k_strlcpy(buf, "Bytes: ", sizeof(buf));
        u32_to_str((uint32_t)ramfs_used_bytes(), num);
        k_strlcat(buf, num, sizeof(buf));
        k_strlcat(buf, "/", sizeof(buf));
        u32_to_str((uint32_t)ramfs_total_bytes(), num);
        k_strlcat(buf, num, sizeof(buf));
        kterm_append_line(buf, col_info);
    } else if (k_streq(cmd, "reboot")) {
        kterm_append_line("Yeniden baslatiliyor...", col_warn);
        outb(0x64, 0xFE);
        for (;;) { __asm__ __volatile__("hlt"); }
    } else if (k_streq(cmd, "halt")) {
        kernel_panic("Kullanici istegi ile sistem durduruldu.");
        for (;;) { __asm__ __volatile__("cli; hlt"); }
    } else if (k_streq(cmd, "exit")) {
        close_window(g_term.win_id);
    } else {
        kterm_append_line("Bilinmeyen komut.", col_error);
    }
}

static void kterm_execute_command(void) {
    if (g_term.input_len == 0) return;

    /* Ekrana kullanıcı girdisini ekle */
    char prompt_line[TERM_COLS];
    prompt_line[0] = '>';
    prompt_line[1] = ' ';
    prompt_line[2] = '\0';
    k_strlcat(prompt_line, g_term.input_buf, TERM_COLS);
    kterm_append_line(prompt_line, graphics_rgb(220, 225, 235));

    /* Komut İşleme */
    kterm_dispatch_cmd(g_term.input_buf);

    g_term.input_len = 0;
    g_term.input_buf[0] = '\0';
}

static void kterm_on_key(int win_id, char ch, uint8_t scancode) {
    (void)win_id;
    (void)scancode;

    if (ch == '\n' || ch == '\r') {
        kterm_execute_command();
    } else if (ch == '\b') {
        if (g_term.input_len > 0) {
            g_term.input_buf[--g_term.input_len] = '\0';
        }
    } else if (ch >= 32 && ch <= 126 && g_term.input_len < INPUT_MAX - 1) {
        g_term.input_buf[g_term.input_len++] = ch;
        g_term.input_buf[g_term.input_len] = '\0';
    }
}

/* ÇİZİM CALLBACK FONKSİYONU */
void kterm_draw_cb(gui_window_t *win, int cx, int cy, int cw, int ch) {
    (void)win;

    /* Arka plan ve Renk Paleti */
    uint32_t bg_color     = graphics_rgb(15, 16, 22);   /* Koyu Siyah/Lacivert */
    uint32_t prompt_color = graphics_rgb(80, 220, 120);  /* Canlı Yeşil Prompt */
    uint32_t input_color  = graphics_rgb(255, 255, 255); /* Beyaz Yazı */
    uint32_t cursor_color = graphics_rgb(0, 255, 150);  /* Neon Yeşil İmleç */

    /* 1. Arka planı temizle */
    graphics_fill_rect(cx, cy, cw, ch, bg_color);

    int margin_x = 10;
    int margin_y = 10;
    int line_height = 14;

    /* 2. Geçmiş satırları kendi renkleriyle çiz */
    int current_y = cy + margin_y;
    for (int i = 0; i < TERM_ROWS; i++) {
        if (g_term.lines[i].text[0] != '\0') {
            ui_draw_text(g_term.lines[i].text, cx + margin_x, current_y, g_term.lines[i].color, bg_color, 14.0f);
            current_y += line_height;
        }
    }

    /* 3. Prompt ve Kullanıcı Girdisi */
    const char *prompt_str = "kayaos> ";
    int prompt_len = (int)k_strlen(prompt_str);

    ui_draw_text(prompt_str, cx + margin_x, current_y, prompt_color, bg_color, 14.0f);

    int input_x = cx + margin_x + (prompt_len * 8);
    if (g_term.input_len > 0) {
        ui_draw_text(g_term.input_buf, input_x, current_y, input_color, bg_color, 14.0f);
    }

    /* 4. Dinamik İmleç */
    int cursor_x = input_x + (g_term.input_len * 8);
    graphics_fill_rect(cursor_x, current_y, 8, 8, cursor_color);
}

static void kterm_on_close(int win_id) {
    (void)win_id;
    g_term_active = false;
}

void kterm_open(void) {
    if (g_term_active) return;

    int win = opa_window_create("Terminal", 440, 280, (void*)kterm_draw_cb);
    if (win < 0) return;

    g_term.win_id = win;
    g_term.input_len = 0;
    g_term.input_buf[0] = '\0';
    g_term.current_dir_node = ramfs_root();
    g_run_depth = 0;
    k_strlcpy(g_term.cwd_path, "/", sizeof(g_term.cwd_path));

    for (int i = 0; i < TERM_ROWS; i++) {
        g_term.lines[i].text[0] = '\0';
        g_term.lines[i].color = graphics_rgb(220, 225, 235);
    }
    
    /* Karşılama Mesajları */
    kterm_append_line("KayaOS Terminal v1.0", graphics_rgb(100, 180, 255));
    kterm_append_line("Yardim icin 'help' yazin.", graphics_rgb(150, 150, 160));

    gui_set_key_callback(win, kterm_on_key);
    gui_set_close_callback(win, kterm_on_close);

    g_term_active = true;
}

#include "kernel/graphics.h"
#include "kernel/io.h"

void kernel_panic(const char *reason) {
    /* 1. Renk Paleti */
    uint32_t bg_color   = graphics_rgb(160, 0, 0);     /* Koyu Kırmızı Arka Plan */
    uint32_t text_color = graphics_rgb(255, 255, 255); /* Beyaz Metin */

    /* 2. Dinamik Ekran Boyutunu Al */
    uint32_t w = graphics_width();
    uint32_t h = graphics_height();

    /* Çözünürlük henüz ilklendirilmemişse varsayılan değerlere düş */
    if (w == 0) w = 1024;
    if (h == 0) h = 768;

    /* Ekranı tamamen Kırmızı ile kapla */
    graphics_fill_rect(0, 0, (int)w, (int)h, bg_color); 

    /* 3. Başlık ve Hata Mesajını Çiz */
    ui_draw_text("========================================", 40, 40, text_color, bg_color, 14.0f);
    ui_draw_text("         *** KERNEL PANIC ***          ", 40, 60, text_color, bg_color, 14.0f);
    ui_draw_text("========================================", 40, 80, text_color, bg_color, 14.0f);

    ui_draw_text("KayaOS kritik bir hata nedeniyle durduruldu.", 40, 120, text_color, bg_color, 14.0f);
    ui_draw_text("Sebep:", 40, 150, text_color, bg_color, 14.0f);
    
    if (reason && reason[0] != '\0') {
        ui_draw_text(reason, 110, 150, text_color, bg_color, 14.0f);
    } else {
        ui_draw_text("Bilinmeyen Hata", 110, 150, text_color, bg_color, 14.0f);
    }

    ui_draw_text("Sistemi yeniden baslatmak icin bilgisayari kapatin.", 40, 200, text_color, bg_color, 14.0f);

    /* 4. KISA YOL: Arka bellekteki (backbuffer) çizimi VRAM'e fırlat (ÇOK ÖNEMLİ) */
    graphics_present();

    apic_delay_ms(1000);

    /* 5. Donanım kesmelerini (interrupts) kapat ve işlemciyi kilitle */
    __asm__ __volatile__("cli");
    for (;;) {
        __asm__ __volatile__("hlt");
    }
}