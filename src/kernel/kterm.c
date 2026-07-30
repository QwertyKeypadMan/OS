#include "kernel/gui.h"
#include "kernel/graphics.h"
#include "kernel/ramfs.h"
#include "kernel/kstring.h"
#include "kernel/keyboard.h"
#include "kernel/version.h"
#include "kernel/apic.h"
#include "kernel/io.h"
#include "../src/kernel/net/ipv4.h"
#include "../src/kernel/net/icmp.h"
#include "../src/kernel/net/endian.h"
#include <stdint.h>
#include <stdbool.h>

#define TERM_MAX_LINES 64     /* Scrollback hafızasındaki toplam satır */
#define TERM_ROWS      12     /* Ekranda aynı anda görünen satır */
#define TERM_COLS      55     /* Satır genişliği */
#define INPUT_MAX      128    /* Maksimum girdi uzunluğu */
#define HISTORY_MAX    16     /* Hatırlanacak komut sayısı */

/* Scancode Tanımları (PS/2) */
#define SCANCODE_UP   0x48
#define SCANCODE_DOWN 0x50

/* Renkli Satır Yapısı */
typedef struct {
    char text[TERM_COLS];
    uint32_t color;
} kterm_line_t;

typedef struct {
    int win_id;
    kterm_line_t lines[TERM_MAX_LINES];
    int line_count;         /* Toplam yazdırılan satır sayısı */
    
    char input_buf[INPUT_MAX];
    int input_len;

    /* Komut Geçmişi (History) */
    char history[HISTORY_MAX][INPUT_MAX];
    int history_count;
    int history_idx;        /* Gezinme indeksi */

    int current_dir_node;
    char cwd_path[128];
    uint32_t frame_counter; /* İmleç animasyonu için */
} kterm_state_t;

static kterm_state_t g_term;
static bool g_term_active = false;
static int g_run_depth = 0;

/* --- YARDIMCI FONKSİYONLAR --- */

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

static bool has_op_extension(const char *path) {
    size_t length = k_strlen(path);
    return length >= 3 && k_streq(path + length - 3, ".op");
}

/* Satır Ekleme (Scrollback Desteği ile) */
static void kterm_append_line(const char *text, uint32_t color) {
    if (g_term.line_count < TERM_MAX_LINES) {
        k_strlcpy(g_term.lines[g_term.line_count].text, text, TERM_COLS);
        g_term.lines[g_term.line_count].color = color;
        g_term.line_count++;
    } else {
        /* Dizi dolduysa yukarı kaydır */
        for (int i = 0; i < TERM_MAX_LINES - 1; i++) {
            g_term.lines[i] = g_term.lines[i + 1];
        }
        k_strlcpy(g_term.lines[TERM_MAX_LINES - 1].text, text, TERM_COLS);
        g_term.lines[TERM_MAX_LINES - 1].color = color;
    }
}

/* Komut Geçmişine Ekleme */
static void kterm_add_history(const char *cmd) {
    if (!cmd || cmd[0] == '\0') return;
    
    /* Son eklenen komutla aynıysa tekrar ekleme */
    if (g_term.history_count > 0 && k_streq(g_term.history[g_term.history_count - 1], cmd)) {
        g_term.history_idx = g_term.history_count;
        return;
    }

    if (g_term.history_count < HISTORY_MAX) {
        k_strlcpy(g_term.history[g_term.history_count], cmd, INPUT_MAX);
        g_term.history_count++;
    } else {
        for (int i = 0; i < HISTORY_MAX - 1; i++) {
            k_strlcpy(g_term.history[i], g_term.history[i + 1], INPUT_MAX);
        }
        k_strlcpy(g_term.history[HISTORY_MAX - 1], cmd, INPUT_MAX);
    }
    g_term.history_idx = g_term.history_count;
}

/* Metin halindeki IP adresini (örn: "10.0.2.2") bayt dizisine çevirir */
static bool parse_ip(const char *str, uint8_t *ip) {
    int val = 0;
    int octet = 0;
    int digits = 0;

    while (*str) {
        if (*str >= '0' && *str <= '9') {
            val = val * 10 + (*str - '0');
            digits++;
            if (val > 255) return false;
        } else if (*str == '.') {
            if (digits == 0 || octet >= 3) return false;
            ip[octet++] = (uint8_t)val;
            val = 0;
            digits = 0;
        } else {
            return false;
        }
        str++;
    }
    if (digits == 0 || octet != 3) return false;
    ip[octet] = (uint8_t)val;
    return true;
}

/* Neofetch / Sistem Bilgisi Gösterici */


    /* Kaplumbağa Logolu Sistem Bilgisi Gösterici */
static void kterm_print_osinfo(void) {
    uint32_t c_ascii = graphics_rgb(80, 220, 120);   /* Canlı Yeşil Kaplumbağa */
    uint32_t c_label = graphics_rgb(100, 180, 255);  /* Mavi Etiketler */

    kterm_append_line("    _____     OS: KayaOS x86_64", c_ascii);
    kterm_append_line("  /  ___  \\   Kernel: v1.0-baremetal", c_ascii);
    kterm_append_line(" (  (o_o)  )  Shell: kterm v2.0", c_ascii);
    kterm_append_line("  <='---'=>", c_ascii);
    kterm_append_line("    \"\" \"\"", c_ascii);
    
    char mem_info[TERM_COLS];
    char num1[16], num2[16];
    u32_to_str((uint32_t)ramfs_used_bytes(), num1);
    u32_to_str((uint32_t)ramfs_total_bytes(), num2);
    
    k_strlcpy(mem_info, "              RAMFS: ", sizeof(mem_info));
    k_strlcat(mem_info, num1, sizeof(mem_info));
    k_strlcat(mem_info, " / ", sizeof(mem_info));
    k_strlcat(mem_info, num2, sizeof(mem_info));
    k_strlcat(mem_info, " B", sizeof(mem_info));
    
    kterm_append_line(mem_info, c_label);
}
   


/* Özyinelemeli Ağaç Yapısı */
static void kterm_tree_recursive(int node_id, int depth) {
    const ramfs_node_t *node = ramfs_get(node_id);
    if (!node) return;

    char line[TERM_COLS];
    int indent = depth * 2;
    if (indent >= TERM_COLS - 4) indent = TERM_COLS - 4;

    for (int i = 0; i < indent; i++) line[i] = ' ';
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

/* --- KOMUT İŞLEYİCİ --- */

static void kterm_dispatch_cmd(const char *input_str) {
    uint32_t col_text  = graphics_rgb(220, 225, 235);
    uint32_t col_info  = graphics_rgb(100, 180, 255);
    uint32_t col_warn  = graphics_rgb(255, 200, 80);
    uint32_t col_error = graphics_rgb(255, 85, 85);

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

    if (k_streq(cmd, "help")) {
        kterm_append_line("--- KayaOS Shell Komutlari ---", col_info);
        kterm_append_line("Sistem : help, clear, osinfo, version, mem, reboot, halt, exit", col_text);
        kterm_append_line("Gezinti: pwd, cd, ls, tree", col_text);
        kterm_append_line("Dosya  : cat, touch, mkdir, write, append, rm, run", col_text);
		kterm_append_line("Ag     : ping <ip>", col_text);
    } else if (k_streq(cmd, "clear")) {
        g_term.line_count = 0;
    } else if (k_streq(cmd, "osinfo") || k_streq(cmd, "kinfo")) {
        kterm_print_osinfo();
    } else if (k_streq(cmd, "version")) {
        kterm_append_line(KAYAOS_NAME " " KAYAOS_VERSION " (" KAYAOS_ARCH ")", col_info);
    } else if (k_streq(cmd, "pwd")) {
        kterm_append_line(g_term.cwd_path, col_info);
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
                    child = cnode ? cnode->next_sibling : -1;
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
                        while (offset < node->size && node->data[offset] != '\n') offset++;
                        if (offset < node->size && node->data[offset] == '\n') offset++;
                        line_buf[l_idx] = '\0';
                        kterm_append_line(line_buf, col_text);
                    }
                }
            }
        }
		
    }
	else if (k_streq(cmd, "ping")) {
        if (arg[0] == '\0') {
            kterm_append_line("Kullanim: ping <ip_adresi>", col_warn);
        } else {
            uint8_t target_ip[4];
            if (!parse_ip(arg, target_ip)) {
                kterm_append_line("Hata: Gecersiz IP adresi biçimi!", col_error);
            } else {
                char ping_msg[TERM_COLS];
                k_strlcpy(ping_msg, "PING ", sizeof(ping_msg));
                k_strlcat(ping_msg, arg, sizeof(ping_msg));
                k_strlcat(ping_msg, " (32 bayt veri):", sizeof(ping_msg));
                kterm_append_line(ping_msg, col_info);

                /* 32 Baytlık ICMP Echo Request Paketi Hazırla */
                uint8_t payload_len = sizeof(icmp_hdr_t) + 32;
                uint8_t ping_pkt[payload_len];

                icmp_hdr_t *icmp = (icmp_hdr_t *)ping_pkt;
                icmp->type = ICMP_TYPE_ECHO_REQUEST; // 8
                icmp->code = 0;
                icmp->checksum = 0;
                icmp->id = htons(0x1337);
                icmp->sequence = htons(1);

                /* Test verisi doldur (metin: "KayaOS Ping Payload Test Data...") */
                uint8_t *data_ptr = ping_pkt + sizeof(icmp_hdr_t);
                const char *test_data = "KayaOS Network Stack Ping Test!";
                for (int d = 0; d < 32; d++) {
                    data_ptr[d] = test_data[d % 31];
                }

                /* ICMP Checksum Hesapla */
                icmp->checksum = net_checksum(ping_pkt, payload_len);

                /* Paketi IPv4 Katmanı Üzerinden Fırlat */
                int res = ipv4_send_packet(target_ip, IP_PROTO_ICMP, ping_pkt, payload_len);
                if (res >= 0) {
                    kterm_append_line(" -> 32 bayt paket ag kartindan gonderildi.", graphics_rgb(80, 220, 120));
                } else {
                    kterm_append_line("Hata: Ağ kartina paket iletilemedi.", col_error);
                }
            }
        }
    }

	else if (k_streq(cmd, "run")) {
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
                        while (offset < node->size && node->data[offset] != '\n') offset++;
                        if (offset < node->size && node->data[offset] == '\n') offset++;
                        line[length] = '\0';

                        char *cursor = line;
                        while (*cursor != '\0' && k_isspace(*cursor)) cursor++;

                        if (*cursor == '\0' || *cursor == '#') continue;

                        char op_line[TERM_COLS];
                        k_strlcpy(op_line, "op> ", sizeof(op_line));
                        k_strlcat(op_line, cursor, sizeof(op_line));
                        kterm_append_line(op_line, graphics_rgb(140, 140, 150));

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
    } else if (k_streq(cmd, "exit")) {
        close_window(g_term.win_id);
    } else {
        kterm_append_line("Bilinmeyen komut. Yardım icin 'help' yazin.", col_error);
    }
}

static void kterm_execute_command(void) {
    if (g_term.input_len == 0) return;

    /* Dinamik Prompt ile komutu ekrana yaz */
    char prompt_line[TERM_COLS];
    k_strlcpy(prompt_line, "root@kayaos:", sizeof(prompt_line));
    k_strlcat(prompt_line, g_term.cwd_path, sizeof(prompt_line));
    k_strlcat(prompt_line, "# ", sizeof(prompt_line));
    k_strlcat(prompt_line, g_term.input_buf, sizeof(prompt_line));
    
    kterm_append_line(prompt_line, graphics_rgb(180, 190, 200));

    /* Komut Geçmişine Ekle */
    kterm_add_history(g_term.input_buf);

    /* Komutu Çalıştır */
    kterm_dispatch_cmd(g_term.input_buf);

    g_term.input_len = 0;
    g_term.input_buf[0] = '\0';
}

/* --- KLAVYE OLAYLARI --- */

static void kterm_on_key(int win_id, char ch, uint8_t scancode) {
    (void)win_id;

    /* YUKARI OK: Önceki Komutu Getir */
    if (scancode == SCANCODE_UP) {
        if (g_term.history_count > 0 && g_term.history_idx > 0) {
            g_term.history_idx--;
            k_strlcpy(g_term.input_buf, g_term.history[g_term.history_idx], INPUT_MAX);
            g_term.input_len = (int)k_strlen(g_term.input_buf);
        }
        return;
    }

    /* AŞAĞI OK: Sonraki Komutu Getir */
    if (scancode == SCANCODE_DOWN) {
        if (g_term.history_idx < g_term.history_count - 1) {
            g_term.history_idx++;
            k_strlcpy(g_term.input_buf, g_term.history[g_term.history_idx], INPUT_MAX);
            g_term.input_len = (int)k_strlen(g_term.input_buf);
        } else {
            g_term.history_idx = g_term.history_count;
            g_term.input_buf[0] = '\0';
            g_term.input_len = 0;
        }
        return;
    }

    /* ENTER */
    if (ch == '\n' || ch == '\r') {
        kterm_execute_command();
    } 
    /* BACKSPACE */
    else if (ch == '\b') {
        if (g_term.input_len > 0) {
            g_term.input_buf[--g_term.input_len] = '\0';
        }
    } 
    /* Standart Karakterler */
    else if (ch >= 32 && ch <= 126 && g_term.input_len < INPUT_MAX - 1) {
        g_term.input_buf[g_term.input_len++] = ch;
        g_term.input_buf[g_term.input_len] = '\0';
    }
}

/* --- GUI ÇİZİM CALLBACK FONKSİYONU --- */

void kterm_draw_cb(gui_window_t *win, int cx, int cy, int cw, int ch) {
    (void)win;

    g_term.frame_counter++;

    /* Renk Paleti (Modern Dark Slate) */
    uint32_t bg_color     = graphics_rgb(14, 16, 22);   /* Mat Koyu Siyah/Mavi */
    uint32_t prompt_user  = graphics_rgb(80, 220, 120);  /* Canlı Yeşil */
    uint32_t prompt_path  = graphics_rgb(100, 180, 255); /* Açık Mavi Dizin */
    uint32_t input_color  = graphics_rgb(255, 255, 255); /* Beyaz Metin */
    uint32_t cursor_color = graphics_rgb(0, 255, 150);   /* Neon Yeşil İmleç */

    /* 1. Arka planı temizle */
    graphics_fill_rect(cx, cy, cw, ch, bg_color);

    int margin_x = 10;
    int margin_y = 10;
    int line_height = 14;

    /* 2. Son TERM_ROWS kadar satırı çiz */
    int start_index = 0;
    if (g_term.line_count > TERM_ROWS) {
        start_index = g_term.line_count - TERM_ROWS;
    }

    int current_y = cy + margin_y;
    for (int i = start_index; i < g_term.line_count; i++) {
        if (g_term.lines[i].text[0] != '\0') {
            ui_draw_text(g_term.lines[i].text, cx + margin_x, current_y, g_term.lines[i].color, bg_color, 14.0f);
            current_y += line_height;
        }
    }

    /* 3. Dinamik Prompt: "root@kayaos:<path># " */
    char p_user[] = "root@kayaos:";
    ui_draw_text(p_user, cx + margin_x, current_y, prompt_user, bg_color, 14.0f);
    
    int user_len = (int)k_strlen(p_user);
    int path_x = cx + margin_x + (user_len * 8);
    ui_draw_text(g_term.cwd_path, path_x, current_y, prompt_path, bg_color, 14.0f);

    int path_len = (int)k_strlen(g_term.cwd_path);
    int hash_x = path_x + (path_len * 8);
    ui_draw_text("# ", hash_x, current_y, prompt_user, bg_color, 14.0f);

    /* 4. Kullanıcı Girdisi */
    int input_x = hash_x + 16;
    if (g_term.input_len > 0) {
        ui_draw_text(g_term.input_buf, input_x, current_y, input_color, bg_color, 14.0f);
    }

    /* 5. Animasyonlu (Blinking) İmleç */
    /* Her ~30 karede bir görünürlüğü değiştir */
    if ((g_term.frame_counter / 20) % 2 == 0) {
        int cursor_x = input_x + (g_term.input_len * 8);
        graphics_fill_rect(cursor_x, current_y, 8, 12, cursor_color);
    }
}

static void kterm_on_close(int win_id) {
    (void)win_id;
    g_term_active = false;
}

void kterm_open(void) {
    if (g_term_active) return;

    int win = opa_window_create("KayaOS Terminal", 460, 290, (void*)kterm_draw_cb);
    if (win < 0) return;

    g_term.win_id = win;
    g_term.input_len = 0;
    g_term.input_buf[0] = '\0';
    g_term.current_dir_node = ramfs_root();
    g_run_depth = 0;
    g_term.line_count = 0;
    g_term.history_count = 0;
    g_term.history_idx = 0;
    g_term.frame_counter = 0;

    k_strlcpy(g_term.cwd_path, "/", sizeof(g_term.cwd_path));

    /* Karşılama Ekranı ve Fetch */
    kterm_print_osinfo();
    kterm_append_line("---------------------------------", graphics_rgb(80, 90, 110));
    kterm_append_line("Yardim icin 'help' yazabilirsiniz.", graphics_rgb(160, 170, 180));

    gui_set_key_callback(win, kterm_on_key);
    gui_set_close_callback(win, kterm_on_close);

    g_term_active = true;
}

/* --- KERNEL PANIC EKRANI --- */

void kernel_panic(const char *reason) {
    uint32_t bg_color   = graphics_rgb(140, 0, 0);   /* Koyu Kırmızı */
    uint32_t text_color = graphics_rgb(255, 255, 255);

    uint32_t w = graphics_width();
    uint32_t h = graphics_height();

    if (w == 0) w = 1024;
    if (h == 0) h = 768;

    graphics_fill_rect(0, 0, (int)w, (int)h, bg_color); 

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

    graphics_present();
    apic_delay_ms(1000);

    __asm__ __volatile__("cli");
    for (;;) {
        __asm__ __volatile__("hlt");
    }
}