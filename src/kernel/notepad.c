#include "kernel/notepad.h"
#include "kernel/gui.h"
#include "kernel/graphics.h"
#include "kernel/ramfs.h"
#include "kernel/kstring.h"
#include "kernel/keyboard.h"

static notepad_state_t g_np;

/* gui.c icinde tanimli yardimci cizim ve bildirim fonksiyonlari */
extern void fill_rounded_rect_alpha(int x, int y, int w, int h, int r, uint32_t color, uint8_t alpha);
extern void fill_circle(int cx, int cy, int r, uint32_t color);
extern void gui_notify(const char *text);

/* ---- SADECE GORSEL/ETKILESIM AMACLI, HEADER'A DOKUNMAYAN YEREL DURUM ---- */
static int  s_frame = 0;
static int  s_last_mouse_x = -1;
static int  s_last_mouse_y = -1;

/* "Dosya" menusu acik mi? */
static bool s_file_menu_open = false;

/* Kaydetme ismi sorma promptu */
static bool s_save_prompt_active = false;
static bool s_save_prompt_is_saveas = false;
static char s_save_prompt_buf[64];
static int  s_save_prompt_len = 0;

/* ---- MENU GEOMETRISI VE ARAYUZ SABITLERI ---- */
#define NP_MENU_H          26
#define NP_DROPDOWN_W      172
#define NP_DROPDOWN_ITEM_H   24
#define UI_CHAR_WIDTH      7  /* ui_draw_text font genisligi ile birebir uyumlu */

static const char *s_menu_labels[4] = { "Dosya", "Duzen", "Gorunum", "Yardim" };
static const int   s_menu_x[4]      = { 10, 62, 118, 182 };
static const int   s_menu_w[4]      = { 44, 48, 56, 50 };

static const char *s_file_dropdown_items[2] = { "Kaydet", "Farkli Kaydet" };

/* Helper: İmleç indisinden Satır ve Kolon Numarası Hesaplama */
static void notepad_update_line_col(void) {
    int line = 1;
    int col = 1;

    for (int i = 0; i < g_np.cursor_pos && i < g_np.text_len; i++) {
        if (g_np.buffer[i] == '\n') {
            line++;
            col = 1;
        } else {
            col++;
        }
    }

    g_np.current_line = line;
    g_np.current_col = col;
}

/* Kucuk yardimci: pozitif bir int'i decimal string'e cevirir, uzunluk doner */
static int notepad_itoa(int value, char *out) {
    if (value <= 0) {
        out[0] = '0';
        out[1] = '\0';
        return 1;
    }
    char tmp[12];
    int tp = 0;
    while (value > 0) {
        tmp[tp++] = (char)('0' + (value % 10));
        value /= 10;
    }
    int len = 0;
    while (tp > 0) {
        out[len++] = tmp[--tp];
    }
    out[len] = '\0';
    return len;
}

/* -------------------------------------------------------------------------- */
/* DOSYA İŞLEMLERİ                                                           */
/* -------------------------------------------------------------------------- */

bool notepad_load_file(const char *path) {
    if (!path || k_strlen(path) == 0) return false;

    int node_id = ramfs_resolve(ramfs_root(), path);
    if (node_id < 0) return false;

    const ramfs_node_t *node = ramfs_get(node_id);
    if (!node || node->type == RAMFS_NODE_DIR) return false;

    uint32_t read_size = node->size;
    if (read_size >= NOTEPAD_MAX_BUFFER) {
        read_size = NOTEPAD_MAX_BUFFER - 1;
    }

    if (node->data && read_size > 0) {
        k_memcpy(g_np.buffer, node->data, read_size);
    }

    g_np.buffer[read_size] = '\0';
    g_np.text_len = (int)read_size;
    g_np.cursor_pos = 0;
    g_np.modified = false;

    k_strlcpy(g_np.filepath, path, sizeof(g_np.filepath));

    int len = (int)k_strlen(path);
    int last_slash = -1;
    for (int i = len - 1; i >= 0; i--) {
        if (path[i] == '/') { last_slash = i; break; }
    }
    if (last_slash >= 0) {
        k_strlcpy(g_np.filename, &path[last_slash + 1], sizeof(g_np.filename));
    } else {
        k_strlcpy(g_np.filename, path, sizeof(g_np.filename));
    }

    notepad_update_line_col();
    return true;
}

bool notepad_save_file(const char *path) {
    const char *target_path = (path && k_strlen(path) > 0) ? path : g_np.filepath;
    if (!target_path || k_strlen(target_path) == 0) return false;

    int result = ramfs_write_file(ramfs_root(), target_path, g_np.buffer, false);

    if (result >= 0) {
        g_np.modified = false;
        k_strlcpy(g_np.filepath, target_path, sizeof(g_np.filepath));

        int len = (int)k_strlen(target_path);
        int last_slash = -1;
        for (int i = len - 1; i >= 0; i--) {
            if (target_path[i] == '/') { last_slash = i; break; }
        }
        if (last_slash >= 0) {
            k_strlcpy(g_np.filename, &target_path[last_slash + 1], sizeof(g_np.filename));
        } else {
            k_strlcpy(g_np.filename, target_path, sizeof(g_np.filename));
        }

        return true;
    }

    return false;
}

void notepad_new(void) {
    g_np.buffer[0] = '\0';
    g_np.text_len = 0;
    g_np.cursor_pos = 0;
    g_np.scroll_x = 0;
    g_np.scroll_y = 0;
    g_np.modified = false;
    k_strlcpy(g_np.filepath, "", sizeof(g_np.filepath));
    k_strlcpy(g_np.filename, "Yeni Belge.txt", sizeof(g_np.filename));
    notepad_update_line_col();
}

/* -------------------------------------------------------------------------- */
/* KAYDETME AKIŞI                                                             */
/* -------------------------------------------------------------------------- */
static void notepad_resolve_save_path(const char *typed_name, char *out, size_t out_size) {
    if (typed_name[0] == '/') {
        k_strlcpy(out, typed_name, out_size);
        return;
    }

    if (g_np.filepath[0] != '\0') {
        int len = (int)k_strlen(g_np.filepath);
        int last_slash = -1;
        for (int i = len - 1; i >= 0; i--) {
            if (g_np.filepath[i] == '/') { last_slash = i; break; }
        }
        if (last_slash >= 0) {
            int dir_len = last_slash + 1;
            if (dir_len >= (int)out_size) dir_len = (int)out_size - 1;
            k_memcpy(out, g_np.filepath, dir_len);
            out[dir_len] = '\0';
            k_strlcat(out, typed_name, out_size);
            return;
        }
    }

    k_strlcpy(out, "/", out_size);
    k_strlcat(out, typed_name, out_size);
}

static void notepad_do_save_with_name(const char *typed_name) {
    if (typed_name == 0 || typed_name[0] == '\0') return;

    char path[160];
    notepad_resolve_save_path(typed_name, path, sizeof(path));

    bool ok = notepad_save_file(path);
    gui_notify(ok ? "Dosya kaydedildi." : "Dosya kaydetme hatasi!");
}

static void notepad_begin_save(bool is_saveas) {
    if (!is_saveas && g_np.filepath[0] != '\0') {
        bool ok = notepad_save_file(NULL);
        gui_notify(ok ? "Dosya kaydedildi." : "Dosya kaydetme hatasi!");
        return;
    }

    s_save_prompt_active = true;
    s_save_prompt_is_saveas = is_saveas;
    k_strlcpy(s_save_prompt_buf, g_np.filename, sizeof(s_save_prompt_buf));
    s_save_prompt_len = (int)k_strlen(s_save_prompt_buf);
}

/* -------------------------------------------------------------------------- */
/* METİN DÜZENLEME İŞLEMLERİ                                                 */
/* -------------------------------------------------------------------------- */

void notepad_insert_char(char c) {
    if (g_np.text_len >= NOTEPAD_MAX_BUFFER - 1) return;

    for (int i = g_np.text_len; i > g_np.cursor_pos; i--) {
        g_np.buffer[i] = g_np.buffer[i - 1];
    }

    g_np.buffer[g_np.cursor_pos] = c;
    g_np.cursor_pos++;
    g_np.text_len++;
    g_np.buffer[g_np.text_len] = '\0';
    g_np.modified = true;

    notepad_update_line_col();
}

void notepad_delete_backspace(void) {
    if (g_np.cursor_pos <= 0) return;

    for (int i = g_np.cursor_pos - 1; i < g_np.text_len - 1; i++) {
        g_np.buffer[i] = g_np.buffer[i + 1];
    }

    g_np.cursor_pos--;
    g_np.text_len--;
    g_np.buffer[g_np.text_len] = '\0';
    g_np.modified = true;

    notepad_update_line_col();
}

void notepad_delete_suppr(void) {
    if (g_np.cursor_pos >= g_np.text_len) return;

    for (int i = g_np.cursor_pos; i < g_np.text_len - 1; i++) {
        g_np.buffer[i] = g_np.buffer[i + 1];
    }

    g_np.text_len--;
    g_np.buffer[g_np.text_len] = '\0';
    g_np.modified = true;

    notepad_update_line_col();
}

void notepad_move_cursor(int delta) {
    int new_pos = g_np.cursor_pos + delta;
    if (new_pos >= 0 && new_pos <= g_np.text_len) {
        g_np.cursor_pos = new_pos;
        notepad_update_line_col();
    }
}

void notepad_move_cursor_line(int line_delta) {
    if (line_delta == -1) { /* Yukarı Ok */
        int cur = g_np.cursor_pos;
        while (cur > 0 && g_np.buffer[cur - 1] != '\n') cur--;
        if (cur > 0) {
            cur--;
            int target_col = g_np.current_col;
            int prev_line_start = cur;
            while (prev_line_start > 0 && g_np.buffer[prev_line_start - 1] != '\n') prev_line_start--;

            int new_pos = prev_line_start + (target_col - 1);
            if (new_pos > cur) new_pos = cur;
            g_np.cursor_pos = new_pos;
        }
    } else if (line_delta == 1) { /* Aşağı Ok */
        int cur = g_np.cursor_pos;
        while (cur < g_np.text_len && g_np.buffer[cur] != '\n') cur++;
        if (cur < g_np.text_len) {
            cur++;
            int target_col = g_np.current_col;
            int next_line_end = cur;
            while (next_line_end < g_np.text_len && g_np.buffer[next_line_end] != '\n') next_line_end++;

            int new_pos = cur + (target_col - 1);
            if (new_pos > next_line_end) new_pos = next_line_end;
            g_np.cursor_pos = new_pos;
        }
    }
    notepad_update_line_col();
}

/* -------------------------------------------------------------------------- */
/* ARAYÜZ ÇİZİMİ                                                              */
/* -------------------------------------------------------------------------- */
void notepad_draw(gui_window_t *win, int cx, int cy, int cw, int ch) {
    (void)win;
    s_frame++;

    /* ---- RENK PALETİ ---- */
    uint32_t bg_main     = graphics_rgb(23, 24, 29);
    uint32_t bg_gutter    = graphics_rgb(18, 19, 23);
    uint32_t menu_bg      = graphics_rgb(33, 35, 43);
    uint32_t menu_bg_hi   = graphics_rgb(45, 92, 168);
    uint32_t menu_border  = graphics_rgb(50, 53, 64);
    uint32_t text_col     = graphics_rgb(224, 227, 232);
    uint32_t dim_text     = graphics_rgb(112, 118, 130);
    uint32_t accent       = graphics_rgb(80, 150, 250);
    uint32_t status_bg    = graphics_rgb(17, 18, 22);
    uint32_t status_txt   = graphics_rgb(148, 156, 168);
    uint32_t cursor_col   = graphics_rgb(255, 255, 255);
    uint32_t linehl_col   = graphics_rgb(35, 38, 47);
    uint32_t modified_col = graphics_rgb(240, 170, 60);
    uint32_t saved_col    = graphics_rgb(110, 200, 120);
    uint32_t dropdown_bg  = graphics_rgb(30, 32, 39);

    /* 1. Ana koyu gri zemin */
    graphics_fill_rect(cx, cy, cw, ch, bg_main);

    /* 2. Üst menü çubuğu */
    graphics_fill_rect(cx, cy, cw, NP_MENU_H, menu_bg);
    graphics_fill_rect(cx, cy + NP_MENU_H - 1, cw, 1, menu_border);

    for (int i = 0; i < 4; i++) {
        bool is_file_tab = (i == 0);
        bool hover = (s_last_mouse_x >= s_menu_x[i] && s_last_mouse_x < s_menu_x[i] + s_menu_w[i] &&
                      s_last_mouse_y >= 0 && s_last_mouse_y < NP_MENU_H);
        bool active_look = hover || (is_file_tab && s_file_menu_open);

        if (active_look) {
            fill_rounded_rect_alpha(cx + s_menu_x[i] - 6, cy + 3, s_menu_w[i], NP_MENU_H - 6, 5,
                menu_bg_hi, 210);
        }

        uint32_t label_bg = active_look ? menu_bg_hi : menu_bg;
        uint32_t label_fg = active_look ? graphics_rgb(255, 255, 255) : text_col;
        ui_draw_text(s_menu_labels[i], cx + s_menu_x[i], cy + 5, label_fg, label_bg, 13.0f);
    }

    /* Başlıkta dosya adı gösterimi */
    char header_name[80];
    k_strlcpy(header_name, g_np.filename, sizeof(header_name));
    if (g_np.modified) {
        k_strlcat(header_name, " *", sizeof(header_name));
    }
    int header_len = (int)k_strlen(header_name);
    int header_x = cx + cw - 26 - (header_len * UI_CHAR_WIDTH);
    if (header_x < cx + 220) header_x = cx + 220;
    ui_draw_text(header_name, header_x, cy + 5,
        g_np.modified ? modified_col : dim_text, menu_bg, 13.0f);

    fill_circle(cx + cw - 16, cy + NP_MENU_H / 2, 4, g_np.modified ? modified_col : saved_col);

    /* 3. "Dosya" dropdown menüsü */
    if (s_file_menu_open) {
        int drop_x = cx + s_menu_x[0] - 6;
        int drop_y = cy + NP_MENU_H;
        int drop_h = 2 * NP_DROPDOWN_ITEM_H + 8;

        fill_rounded_rect_alpha(drop_x, drop_y, NP_DROPDOWN_W, drop_h, 8, dropdown_bg, 245);
        graphics_draw_rounded_rect(drop_x, drop_y, NP_DROPDOWN_W, drop_h, 8, menu_border);

        for (int i = 0; i < 2; i++) {
            int item_y = drop_y + 4 + i * NP_DROPDOWN_ITEM_H;
            bool hover = (s_last_mouse_x >= drop_x && s_last_mouse_x < drop_x + NP_DROPDOWN_W &&
                          s_last_mouse_y >= item_y && s_last_mouse_y < item_y + NP_DROPDOWN_ITEM_H);

            if (hover) {
                graphics_fill_rect(drop_x + 4, item_y, NP_DROPDOWN_W - 8, NP_DROPDOWN_ITEM_H, menu_bg_hi);
            }

            uint32_t item_bg = hover ? menu_bg_hi : dropdown_bg;
            ui_draw_text(s_file_dropdown_items[i], drop_x + 14, item_y + 5,
                graphics_rgb(230, 232, 236), item_bg, 13.0f);
        }
    }

    /* 4. Metin Alanı ve Satır Numaraları */
    int edit_y = cy + NP_MENU_H;
    int status_h = 24;
    int edit_h = ch - NP_MENU_H - status_h;
    if (edit_h < 0) edit_h = 0;
    int gutter_w = 46;

    graphics_fill_rect(cx, edit_y, gutter_w, edit_h, bg_gutter);
    graphics_fill_rect(cx + gutter_w, edit_y, 1, edit_h, menu_border);

    int visible_lines = edit_h / NOTEPAD_LINE_HEIGHT;
    if (visible_lines < 1) visible_lines = 1;

    /* Otomatik Kaydırma (Auto-Scroll): İmleci görünür alanda tut */
    int cur_line_idx = g_np.current_line - 1;
    if (cur_line_idx < g_np.scroll_y) {
        g_np.scroll_y = cur_line_idx;
    } else if (cur_line_idx >= g_np.scroll_y + visible_lines) {
        g_np.scroll_y = cur_line_idx - visible_lines + 1;
    }
    if (g_np.scroll_y < 0) g_np.scroll_y = 0;

    int line_idx = 0;
    int char_idx = 0;
    int line_start = 0;
    int draw_line = 0;

    while (char_idx <= g_np.text_len && draw_line < visible_lines) {
        if (char_idx == g_np.text_len || g_np.buffer[char_idx] == '\n') {
            if (line_idx >= g_np.scroll_y) {
                int print_y = edit_y + 4 + (draw_line * NOTEPAD_LINE_HEIGHT);
                bool cursor_on_line = (g_np.cursor_pos >= line_start && g_np.cursor_pos <= char_idx);

                if (cursor_on_line) {
                    graphics_fill_rect(cx + gutter_w + 1, print_y - 2, cw - gutter_w - 1,
                        NOTEPAD_LINE_HEIGHT, linehl_col);
                }

                char lnbuf[12];
                int lp = notepad_itoa(line_idx + 1, lnbuf);
                int ln_x = cx + gutter_w - 10 - (lp * UI_CHAR_WIDTH);
                uint32_t ln_bg = cursor_on_line ? linehl_col : bg_gutter;
                uint32_t ln_fg = cursor_on_line ? accent : dim_text;
                ui_draw_text(lnbuf, ln_x, print_y, ln_fg, ln_bg, 12.0f);

                char line_buf[256];
                int len = char_idx - line_start;
                if (len > 255) len = 255;
                for (int k = 0; k < len; k++) {
                    line_buf[k] = g_np.buffer[line_start + k];
                }
                line_buf[len] = '\0';

                uint32_t line_bg = cursor_on_line ? linehl_col : bg_main;
                ui_draw_text(line_buf, cx + gutter_w + 10, print_y, text_col, line_bg, 13.0f);

                /* DÜZELTME: UI_CHAR_WIDTH (7px) kullanılarak imlecin hizalaması sağlandı */
                if (cursor_on_line) {
                    int cursor_col_offset = g_np.cursor_pos - line_start;
                    int cursor_x = cx + gutter_w + 10 + (cursor_col_offset * UI_CHAR_WIDTH);
                    bool blink_on = ((s_frame / 30) % 2) == 0;
                    if (blink_on) {
                        graphics_fill_rect(cursor_x, print_y - 1, 2, NOTEPAD_LINE_HEIGHT - 2, cursor_col);
                    }
                }

                draw_line++;
            }
            line_idx++;
            line_start = char_idx + 1;
        }
        char_idx++;
    }

    /* 5. Kaydırma Çubuğu */
    int total_lines = (line_idx > 0) ? line_idx : 1;
    if (total_lines > visible_lines) {
        int track_x = cx + cw - 8;
        int track_y = edit_y + 2;
        int track_h = edit_h - 4;
        if (track_h < 4) track_h = 4;

        graphics_fill_rect(track_x, track_y, 4, track_h, graphics_rgb(30, 32, 38));

        int thumb_h = (track_h * visible_lines) / total_lines;
        if (thumb_h < 20) thumb_h = 20;
        if (thumb_h > track_h) thumb_h = track_h;

        int max_scroll = total_lines - visible_lines;
        int thumb_y = track_y;
        if (max_scroll > 0) {
            thumb_y += ((track_h - thumb_h) * g_np.scroll_y) / max_scroll;
        }

        graphics_fill_rect(track_x, thumb_y, 4, thumb_h, accent);
    }

    /* 6. Durum Çubuğu */
    int status_y = cy + ch - status_h;
    graphics_fill_rect(cx, status_y, cw, status_h, status_bg);
    graphics_fill_rect(cx, status_y, cw, 1, menu_border);

    char status_str[160];
    char num_buf[16];

    k_strlcpy(status_str, "Satir ", sizeof(status_str));
    notepad_itoa(g_np.current_line, num_buf);
    k_strlcat(status_str, num_buf, sizeof(status_str));

    k_strlcat(status_str, "   Kolon ", sizeof(status_str));
    notepad_itoa(g_np.current_col, num_buf);
    k_strlcat(status_str, num_buf, sizeof(status_str));

    ui_draw_text(status_str, cx + 10, status_y + 5, status_txt, status_bg, 12.0f);

    const char *state_text = g_np.modified ? "Kaydedilmedi" : "Kaydedildi";
    uint32_t state_col = g_np.modified ? modified_col : saved_col;
    int state_len = (int)k_strlen(state_text);
    int state_x = cx + cw - 14 - (state_len * UI_CHAR_WIDTH);

    fill_circle(state_x - 10, status_y + status_h / 2, 3, state_col);
    ui_draw_text(state_text, state_x, status_y + 5, state_col, status_bg, 12.0f);

    /* 7. KAYDETME PROMPTU */
    if (s_save_prompt_active) {
        fill_rounded_rect_alpha(cx, cy, cw, ch, 0, graphics_rgb(0, 0, 0), 150);

        int box_w = 320;
        int box_h = 96;
        int box_x = cx + (cw - box_w) / 2;
        int box_y = cy + (ch - box_h) / 2;

        fill_rounded_rect_alpha(box_x, box_y, box_w, box_h, 10, graphics_rgb(38, 40, 48), 250);
        graphics_draw_rounded_rect(box_x, box_y, box_w, box_h, 10, accent);

        const char *title = s_save_prompt_is_saveas ? "Farkli Kaydet" : "Dosyayi Kaydet";
        ui_draw_text(title, box_x + 16, box_y + 12,
            graphics_rgb(235, 237, 240), graphics_rgb(38, 40, 48), 14.0f);

        ui_draw_text("Dosya adi:", box_x + 16, box_y + 36,
            dim_text, graphics_rgb(38, 40, 48), 12.0f);

        int input_x = box_x + 16;
        int input_y = box_y + 52;
        int input_w = box_w - 32;
        int input_h = 22;
        graphics_fill_rect(input_x, input_y, input_w, input_h, graphics_rgb(20, 21, 26));
        graphics_draw_rounded_rect(input_x, input_y, input_w, input_h, 4, accent);
        ui_draw_text(s_save_prompt_buf, input_x + 6, input_y + 4,
            graphics_rgb(240, 240, 244), graphics_rgb(20, 21, 26), 13.0f);

        /* DÜZELTME: Kaydetme promptunda da genişlik hesaplaması düzeltildi */
        if ((s_frame / 30) % 2 == 0) {
            int cur_x = input_x + 6 + (s_save_prompt_len * UI_CHAR_WIDTH);
            graphics_fill_rect(cur_x, input_y + 3, 2, input_h - 6, graphics_rgb(255, 255, 255));
        }

        ui_draw_text("Enter: Kaydet   Esc: Iptal", box_x + 16, box_y + box_h - 16,
            dim_text, graphics_rgb(38, 40, 48), 11.0f);
    }
}

/* -------------------------------------------------------------------------- */
/* ETKİLEŞİM VE GİRDİ YÖNETİMİ                                               */
/* -------------------------------------------------------------------------- */

void notepad_on_key(int win_id, char ascii, uint8_t scancode) {
    (void)win_id;
    if (!g_np.active) return;

    if (s_save_prompt_active) {
        if (ascii == '\n' || ascii == '\r') {
            if (s_save_prompt_len > 0) {
                notepad_do_save_with_name(s_save_prompt_buf);
            }
            s_save_prompt_active = false;
            return;
        }

        if (ascii == 27) {
            s_save_prompt_active = false;
            return;
        }

        if (ascii == '\b') {
            if (s_save_prompt_len > 0) {
                s_save_prompt_len--;
                s_save_prompt_buf[s_save_prompt_len] = '\0';
            }
            return;
        }

        if (ascii >= 32 && ascii <= 126 && s_save_prompt_len < (int)sizeof(s_save_prompt_buf) - 1) {
            s_save_prompt_buf[s_save_prompt_len++] = ascii;
            s_save_prompt_buf[s_save_prompt_len] = '\0';
        }
        return;
    }

    switch (scancode) {
        case KEY_LEFT:
            notepad_move_cursor(-1);
            return;
        case KEY_RIGHT:
            notepad_move_cursor(1);
            return;
        case KEY_UP:
            notepad_move_cursor_line(-1);
            return;
        case KEY_DOWN:
            notepad_move_cursor_line(1);
            return;
        case KEY_DELETE:
            notepad_delete_suppr();
            return;
        case KEY_HOME:
            while (g_np.cursor_pos > 0 && g_np.buffer[g_np.cursor_pos - 1] != '\n') {
                g_np.cursor_pos--;
            }
            notepad_update_line_col();
            return;
        case KEY_END:
            while (g_np.cursor_pos < g_np.text_len && g_np.buffer[g_np.cursor_pos] != '\n') {
                g_np.cursor_pos++;
            }
            notepad_update_line_col();
            return;
    }

    if (ascii == '\n' || ascii == '\r') {
        notepad_insert_char('\n');
    } else if (ascii == '\b') {
        notepad_delete_backspace();
    } else if (ascii >= 32 && ascii <= 126) {
        notepad_insert_char(ascii);
    }
}

void notepad_handle_mouse(int win_id, int mouse_x, int mouse_y, uint8_t buttons) {
    s_last_mouse_x = mouse_x;
    s_last_mouse_y = mouse_y;

    if (win_id != g_np.win_id) return;
    if (!(buttons & 0x01)) return;

    if (s_save_prompt_active) return;

    /* Dropdown menü kontrolleri */
    if (s_file_menu_open) {
        int drop_x = s_menu_x[0] - 6;
        int drop_y = NP_MENU_H;
        int drop_h = 2 * NP_DROPDOWN_ITEM_H + 8;

        if (mouse_x >= drop_x && mouse_x < drop_x + NP_DROPDOWN_W &&
            mouse_y >= drop_y && mouse_y < drop_y + drop_h) {
            int rel_y = mouse_y - (drop_y + 4);
            if (rel_y >= 0) {
                int idx = rel_y / NP_DROPDOWN_ITEM_H;
                if (idx == 0) {
                    notepad_begin_save(false);
                } else if (idx == 1) {
                    notepad_begin_save(true);
                }
            }
        }

        s_file_menu_open = false;
        return;
    }

    /* Üst menü sekmeleri */
    if (mouse_y >= 0 && mouse_y < NP_MENU_H) {
        if (mouse_x >= s_menu_x[0] && mouse_x < s_menu_x[0] + s_menu_w[0]) {
            s_file_menu_open = !s_file_menu_open;
        }
        return;
    }

    /* YENİ: Metin alanına fare ile tıklayarak imleç konumlandırma */
    if (mouse_y >= NP_MENU_H) {
        int gutter_w = 46;
        int text_x = gutter_w + 10;
        int clicked_rel_line = (mouse_y - NP_MENU_H - 4) / NOTEPAD_LINE_HEIGHT;
        if (clicked_rel_line < 0) clicked_rel_line = 0;

        int target_line = g_np.scroll_y + clicked_rel_line;
        int cur_line = 0;
        int char_idx = 0;
        int line_start = 0;

        while (char_idx <= g_np.text_len) {
            if (char_idx == g_np.text_len || g_np.buffer[char_idx] == '\n') {
                if (cur_line == target_line) {
                    int line_len = char_idx - line_start;
                    int click_col = (mouse_x - text_x + (UI_CHAR_WIDTH / 2)) / UI_CHAR_WIDTH;
                    if (click_col < 0) click_col = 0;
                    if (click_col > line_len) click_col = line_len;

                    g_np.cursor_pos = line_start + click_col;
                    notepad_update_line_col();
                    break;
                }
                cur_line++;
                line_start = char_idx + 1;
            }
            char_idx++;
        }
    }
}

/* -------------------------------------------------------------------------- */
/* LIFECYCLE MANAGEMENT                                                       */
/* -------------------------------------------------------------------------- */

static void notepad_on_close(int win_id) {
    (void)win_id;
    g_np.active = false;
    s_file_menu_open = false;
    s_save_prompt_active = false;
    s_last_mouse_x = -1;
    s_last_mouse_y = -1;
}

void notepad_open(const char *path) {
    if (g_np.active) {
        if (path && k_strlen(path) > 0) {
            notepad_load_file(path);
        }
        return;
    }

    notepad_new();
    s_file_menu_open = false;
    s_save_prompt_active = false;

    if (path && k_strlen(path) > 0) {
        notepad_load_file(path);
    }

    char title[128];
    k_strlcpy(title, "Notepad - ", sizeof(title));
    k_strlcat(title, g_np.filename, sizeof(title));

    int win = opa_window_create(title, 600, 400, (void*)notepad_draw);
    if (win < 0) return;

    g_np.win_id = win;
    g_np.active = true;

    gui_set_key_callback(win, notepad_on_key);
    gui_set_close_callback(win, notepad_on_close);
}

void notepad_close(void) {
    if (g_np.active) {
        close_window(g_np.win_id);
        g_np.active = false;
    }
}