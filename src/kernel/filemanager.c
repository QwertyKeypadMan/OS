#include "filemanager.h"
#include "kernel/gui.h"
#include "kernel/graphics.h"
#include "kernel/bmp.h"
#include "kernel/assets.h"
#include "kernel/ramfs.h"
#include "kernel/kstring.h"
#include "kernel/keyboard.h"
#include "kernel/file_assoc.h"

#define MAX_FM_ITEMS 128
#define ITEM_NAME_LEN 64
#define SIDEBAR_COUNT 4
#define ROW_HEIGHT 40  /* 32x32 ikonlar icin ideal satir yuksekligi */

/* Dizin / Dosya Oge Yapisi */
typedef struct {
    char name[ITEM_NAME_LEN];
    int node_id;
    bool is_dir;
    uint32_t size;
} fm_item_t;

/* Sol Panel Hizli Erisim Yapisi */
typedef struct {
    char label[32];
    char path[64];
} fm_sidebar_item_t;

/* File Manager Uygulama Durumu */
typedef struct {
    int win_id;
    bool active;

    /* Dizin Durumu */
    int current_node;
    char cwd_path[256];

    /* Ogeler ve Secim */
    fm_item_t items[MAX_FM_ITEMS];
    int item_count;
    int selected_index;
    int scroll_offset;

    /* Sol Panel */
    fm_sidebar_item_t sidebar[SIDEBAR_COUNT];
    int selected_sidebar;
} fm_state_t;

static fm_state_t g_fm;

/* -------------------------------------------------------------------------- */
/* IKON CIZIMI (Asset Sistemi Uzerinden)                                      */
/* -------------------------------------------------------------------------- */
static void fm_draw_icon(int x, int y, bool is_dir) {
    if (is_dir) {
        bmp_draw_asset("folder.bmp", x, y, true);
    } else {
        bmp_draw_asset("file.bmp", x, y, true);
    }
}

/* -------------------------------------------------------------------------- */
/* DIZIN LISTELEME VE YONETIM                                                */
/* -------------------------------------------------------------------------- */
static void fm_refresh_list(void) {
    g_fm.item_count = 0;
    g_fm.selected_index = -1;

    const ramfs_node_t *node = ramfs_get(g_fm.current_node);
    if (!node || node->type != RAMFS_NODE_DIR) return;

    int child = node->first_child;
    while (child >= 0 && g_fm.item_count < MAX_FM_ITEMS) {
        const ramfs_node_t *cnode = ramfs_get(child);
        if (cnode) {
            k_strlcpy(g_fm.items[g_fm.item_count].name, cnode->name, ITEM_NAME_LEN);
            g_fm.items[g_fm.item_count].node_id = child;
            g_fm.items[g_fm.item_count].is_dir  = (cnode->type == RAMFS_NODE_DIR);
            g_fm.items[g_fm.item_count].size    = cnode->size;
            g_fm.item_count++;
        }
        child = cnode->next_sibling;
    }
}

void fm_change_directory(int target_node, const char *path) {
    if (target_node < 0) return;

    g_fm.current_node = target_node;
    k_strlcpy(g_fm.cwd_path, path, sizeof(g_fm.cwd_path));
    g_fm.scroll_offset = 0;
    
    fm_refresh_list();
}

void fm_select_item(int index) {
    if (index >= 0 && index < g_fm.item_count) {
        g_fm.selected_index = index;
    } else {
        g_fm.selected_index = -1;
    }
}

void fm_open_selected(void) {
    if (g_fm.selected_index < 0 || g_fm.selected_index >= g_fm.item_count) return;

    fm_item_t *item = &g_fm.items[g_fm.selected_index];
    
    char full_path[256];
    k_strlcpy(full_path, g_fm.cwd_path, sizeof(full_path));
    if (!k_streq(full_path, "/")) {
        k_strlcat(full_path, "/", sizeof(full_path));
    }
    k_strlcat(full_path, item->name, sizeof(full_path));

    if (item->is_dir) {
        /* Dizin ise Klasöre Gir */
        fm_change_directory(item->node_id, full_path);
    } else {
        /* Dosya ise İlişkilendirme Yöneticisi Üzerinden İlgili Uygulamayı Başlat */
        file_association_open(full_path);
    }
}

/* -------------------------------------------------------------------------- */
/* ARAYUZ CIZIM FONKSIYONLARI                                                */
/* -------------------------------------------------------------------------- */
void fm_draw_toolbar(int cx, int cy, int cw) {
    uint32_t bg_col     = graphics_rgb(45, 48, 55);
    uint32_t btn_col    = graphics_rgb(60, 65, 75);
    uint32_t text_col   = graphics_rgb(240, 240, 240);
    uint32_t path_bg    = graphics_rgb(25, 28, 32);

    graphics_fill_rect(cx, cy, cw, 32, bg_col);

    /* Geri ve Ust Dizin Butonlari */
    graphics_fill_rect(cx + 6, cy + 4, 24, 24, btn_col);
    ui_draw_text("<", cx + 13, cy + 5, text_col, btn_col, 14.0f);

    graphics_fill_rect(cx + 34, cy + 4, 24, 24, btn_col);
    ui_draw_text("^", cx + 41, cy + 5, text_col, btn_col, 14.0f);

    /* Adres Cubugu */
    graphics_fill_rect(cx + 64, cy + 4, cw - 70, 24, path_bg);
    ui_draw_text(g_fm.cwd_path, cx + 72, cy + 6, text_col, path_bg, 13.0f);
}

void fm_draw_sidebar(int cx, int cy, int ch) {
    uint32_t bg_col     = graphics_rgb(32, 35, 42);
    uint32_t text_col   = graphics_rgb(210, 215, 225);
    uint32_t sel_bg     = graphics_rgb(50, 75, 115);
    uint32_t border_col = graphics_rgb(20, 22, 26);

    graphics_fill_rect(cx, cy, 130, ch, bg_col);

    int item_y = cy + 6;
    for (int i = 0; i < SIDEBAR_COUNT; i++) {
        uint32_t current_bg = (g_fm.selected_sidebar == i) ? sel_bg : bg_col;
        
        if (g_fm.selected_sidebar == i) {
            graphics_fill_rect(cx + 4, item_y, 122, 36, current_bg);
        }

        /* 32x32 Ikon Cizimi */
        fm_draw_icon(cx + 8, item_y + 2, true);

        /* Metin: Ikon bitisinden (8 + 32 = 40) sonra 8px bosluk birakip cx + 48'e yaziyoruz */
        ui_draw_text(g_fm.sidebar[i].label, cx + 48, item_y + 11, text_col, current_bg, 13.0f);
        
        item_y += ROW_HEIGHT;
    }

    /* Sol - Sag panel ayirici cizgi */
    graphics_fill_rect(cx + 129, cy, 1, ch, border_col);
}

void fm_draw_file_list(int cx, int cy, int cw, int ch) {
    uint32_t bg_col      = graphics_rgb(20, 22, 26);
    uint32_t text_col    = graphics_rgb(225, 225, 230);
    uint32_t sel_bg      = graphics_rgb(45, 90, 160);
    uint32_t sel_text    = graphics_rgb(255, 255, 255);

    graphics_fill_rect(cx, cy, cw, ch, bg_col);

    int visible_rows = ch / ROW_HEIGHT;
    int start_index = g_fm.scroll_offset;
    int end_index = start_index + visible_rows;
    if (end_index > g_fm.item_count) end_index = g_fm.item_count;

    int draw_y = cy + 6;
    for (int i = start_index; i < end_index; i++) {
        bool is_selected = (i == g_fm.selected_index);
        uint32_t row_bg = is_selected ? sel_bg : bg_col;
        uint32_t row_fg = is_selected ? sel_text : text_col;

        if (is_selected) {
            graphics_fill_rect(cx + 2, draw_y, cw - 4, 36, row_bg);
        }

        /* 32x32 Ikon Cizimi */
        fm_draw_icon(cx + 8, draw_y + 2, g_fm.items[i].is_dir);

        /* Metin: Ikonun sagina (cx + 48) ve dikeyde ortalanmis (draw_y + 11) */
        ui_draw_text(g_fm.items[i].name, cx + 48, draw_y + 11, row_fg, row_bg, 13.0f);

        draw_y += ROW_HEIGHT;
    }
}

void fm_draw_statusbar(int cx, int cy, int cw, int ch) {
    (void)ch;
    uint32_t bg_col   = graphics_rgb(32, 35, 42);
    uint32_t text_col = graphics_rgb(170, 175, 185);

    graphics_fill_rect(cx, cy, cw, 24, bg_col);

    char status_str[128];
    if (g_fm.selected_index >= 0 && g_fm.selected_index < g_fm.item_count) {
        k_strlcpy(status_str, "Secili: ", sizeof(status_str));
        k_strlcat(status_str, g_fm.items[g_fm.selected_index].name, sizeof(status_str));
    } else {
        k_strlcpy(status_str, "Toplam Oge: ", sizeof(status_str));
        
        char count_buf[16];
        int val = g_fm.item_count;
        int pos = 0;
        if (val == 0) { count_buf[pos++] = '0'; }
        else {
            char temp[16]; int tpos = 0;
            while(val > 0) { temp[tpos++] = '0' + (val % 10); val /= 10; }
            while(tpos > 0) count_buf[pos++] = temp[--tpos];
        }
        count_buf[pos] = '\0';
        k_strlcat(status_str, count_buf, sizeof(status_str));
    }

    ui_draw_text(status_str, cx + 10, cy + 4, text_col, bg_col, 13.0f);
}

/* -------------------------------------------------------------------------- */
/* ANA CALLBACK & ETKILESIM YONETIMI                                          */
/* -------------------------------------------------------------------------- */
void fm_draw(gui_window_t *win, int cx, int cy, int cw, int ch) {
    (void)win;

    /* Arka Plan */
    graphics_fill_rect(cx, cy, cw, ch, graphics_rgb(20, 22, 26));

    /* Arayuz Bilesenleri */
    fm_draw_toolbar(cx, cy, cw);
    fm_draw_sidebar(cx, cy + 32, ch - 56);
    fm_draw_file_list(cx + 130, cy + 32, cw - 130, ch - 56);
    fm_draw_statusbar(cx, cy + ch - 24, cw, 24);
}

void fm_handle_mouse(int win_id, int mouse_x, int mouse_y, uint8_t buttons) {
    if (win_id != g_fm.win_id || !(buttons & 0x01)) return;

    /* 1. Toolbar Etkilesimleri */
    if (mouse_y >= 0 && mouse_y < 32) {
        if (mouse_x >= 34 && mouse_x <= 58) {
            /* Ust Dizin (..) */
            if (!k_streq(g_fm.cwd_path, "/")) {
                int len = (int)k_strlen(g_fm.cwd_path);
                int last_slash = -1;
                for (int s = len - 1; s >= 0; s--) {
                    if (g_fm.cwd_path[s] == '/') { last_slash = s; break; }
                }
                if (last_slash > 0) g_fm.cwd_path[last_slash] = '\0';
                else k_strlcpy(g_fm.cwd_path, "/", sizeof(g_fm.cwd_path));

                int parent = ramfs_resolve(ramfs_root(), g_fm.cwd_path);
                fm_change_directory(parent >= 0 ? parent : ramfs_root(), g_fm.cwd_path);
            }
        }
        return;
    }

    /* 2. Sol Panel Etkilesimi (ROW_HEIGHT = 40'a gore ayarlandi) */
    if (mouse_x >= 0 && mouse_x < 130 && mouse_y >= 32 && mouse_y < 336) {
        int idx = (mouse_y - 38) / ROW_HEIGHT;
        if (idx >= 0 && idx < SIDEBAR_COUNT) {
            g_fm.selected_sidebar = idx;
            int target = ramfs_resolve(ramfs_root(), g_fm.sidebar[idx].path);
            fm_change_directory(target >= 0 ? target : ramfs_root(), g_fm.sidebar[idx].path);
        }
        return;
    }

    /* 3. Sag Panel / Dosya Listesi Etkilesimi (ROW_HEIGHT = 40'a gore ayarlandi) */
    if (mouse_x >= 130 && mouse_y >= 32 && mouse_y < 336) {
        int clicked_row = (mouse_y - 38) / ROW_HEIGHT + g_fm.scroll_offset;
        
        if (clicked_row >= 0 && clicked_row < g_fm.item_count) {
            if (g_fm.selected_index == clicked_row) {
                fm_open_selected();
            } else {
                fm_select_item(clicked_row);
            }
        }
        return;
    }
}

void fm_update(void) {
    /* Ihtiyac halinde guncelleme mantigi */
}

/* -------------------------------------------------------------------------- */
/* BASLATMA VE KAPATMA                                                       */
/* -------------------------------------------------------------------------- */
static void fm_on_close(int win_id) {
    (void)win_id;
    g_fm.active = false;
}

void fm_open(void) {
    if (g_fm.active) return;

    int win = opa_window_create("Dosya Yoneticisi", 560, 360, (void*)fm_draw);
    if (win < 0) return;

    g_fm.win_id = win;
    g_fm.active = true;
    g_fm.selected_sidebar = 0;

    /* Sol Panel Hizli Erisim Tanimlari */
    k_strlcpy(g_fm.sidebar[0].label, "Kok Dizin", 32);
    k_strlcpy(g_fm.sidebar[0].path, "/", 64);

    k_strlcpy(g_fm.sidebar[1].label, "Desktop", 32);
    k_strlcpy(g_fm.sidebar[1].path, "/desktop", 64);


    /* Baslangic Dizini (Root) */
    fm_change_directory(ramfs_root(), "/");

    gui_set_close_callback(win, fm_on_close);
}

void fm_close(void) {
    if (g_fm.active) {
        close_window(g_fm.win_id);
        g_fm.active = false;
    }
}