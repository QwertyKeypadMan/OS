#include "kernel/settings.h"
#include "kernel/gui.h"
#include "kernel/graphics.h"
#include "kernel/kstring.h"

static settings_state_t g_settings;

/* gui.c veya sistem tarafindan saglanan harici cizim / sistem fonksiyonlari */
extern void fill_rounded_rect_alpha(int x, int y, int w, int h, int r, uint32_t color, uint8_t alpha);
extern void fill_circle(int cx, int cy, int r, uint32_t color);
extern void gui_notify(const char *text);

/* Sisteminizde imlec ve duvar kagidini dinamik degistiren fonksiyonlar.
 * (Eger isimleri farkliysa kendi sisteminizdekilerle degistirebilirsiniz) */
extern void gui_set_cursor(const char *bmp_path);
extern void gui_set_wallpaper(const char *bmp_path);

/* Arayuz Durum Bilgileri */
static int s_last_mouse_x = -1;
static int s_last_mouse_y = -1;

/* Dosya Yollari */
static const char *s_cursor_paths[2] = {
    "cursor.bmp",
    "cursorwhite.bmp"
};

static const char *s_wallpaper_paths[3] = {
    "wallpaper.bmp",
    "wl2.bmp",
    "wl3.bmp"
};

/* -------------------------------------------------------------------------- */
/* ARAYÜZ ÇİZİMİ                                                             */
/* -------------------------------------------------------------------------- */
void settings_draw(gui_window_t *win, int cx, int cy, int cw, int ch) {
    (void)win;

    /* ---- RENK PALETİ (Notepad ile Birebir Uyumlu) ---- */
    uint32_t bg_main       = graphics_rgb(23, 24, 29);
    uint32_t sidebar_bg    = graphics_rgb(18, 19, 23);
    uint32_t card_bg       = graphics_rgb(30, 32, 40);
    uint32_t card_hover    = graphics_rgb(38, 41, 52);
    uint32_t border_col    = graphics_rgb(50, 53, 64);
    uint32_t text_col      = graphics_rgb(224, 227, 232);
    uint32_t dim_text      = graphics_rgb(120, 126, 138);
    uint32_t accent        = graphics_rgb(80, 150, 250);
    uint32_t active_tab_bg = graphics_rgb(45, 92, 168);

    /* 1. Arka Plan */
    graphics_fill_rect(cx, cy, cw, ch, bg_main);

    /* 2. Sol Bölme (Sidebar) */
    int sidebar_w = 140;
    graphics_fill_rect(cx, cy, sidebar_w, ch, sidebar_bg);
    graphics_fill_rect(cx + sidebar_w - 1, cy, 1, ch, border_col);

    /* Sidebar Basligi */
    ui_draw_text("Ayarlar", cx + 16, cy + 16, text_col, sidebar_bg, 15.0f);

    /* Sidebar Sekmesi: Gorunum */
    int tab_y = cy + 50;
    bool tab_hover = (s_last_mouse_x >= 8 && s_last_mouse_x < sidebar_w - 8 &&
                      s_last_mouse_y >= 50 && s_last_mouse_y < 82);

    uint32_t tab_bg = (g_settings.current_tab == 0) ? active_tab_bg : (tab_hover ? card_hover : sidebar_bg);
    fill_rounded_rect_alpha(cx + 8, tab_y, sidebar_w - 16, 32, 6, tab_bg, 255);
    ui_draw_text("Gorunum", cx + 20, tab_y + 8, graphics_rgb(255, 255, 255), tab_bg, 13.0f);

    /* 3. Sag Icerik Alani (Gorunum Sekmesi) */
    int content_x = cx + sidebar_w + 20;
    int content_y = cy + 20;

    ui_draw_text("Gorunum Ayarlari", content_x, content_y, text_col, bg_main, 16.0f);
    
    /* --- BÖLÜM 1: Fare İmleci Rengi --- */
    int section1_y = content_y + 35;
    ui_draw_text("Fare Imleci", content_x, section1_y, dim_text, bg_main, 12.0f);

    int card_w = 160;
    int card_h = 42;
    int cursor_cards_y = section1_y + 20;

    const char *cursor_names[2] = { "Siyah (cursor.bmp)", "Beyaz (cursorwhite.bmp)" };

    for (int i = 0; i < 2; i++) {
        int card_x = content_x + (i * (card_w + 12));
        bool is_selected = (g_settings.selected_cursor == i);
        
        /* Local mouse coords */
        int rel_mx = s_last_mouse_x - sidebar_w - 20;
        int rel_my = s_last_mouse_y - (cursor_cards_y - cy);
        bool hover = (rel_mx >= (i * (card_w + 12)) && rel_mx < (i * (card_w + 12)) + card_w &&
                      rel_my >= 0 && rel_my < card_h);

        uint32_t c_bg = is_selected ? graphics_rgb(35, 55, 90) : (hover ? card_hover : card_bg);
        fill_rounded_rect_alpha(card_x, cursor_cards_y, card_w, card_h, 8, c_bg, 255);
        graphics_draw_rounded_rect(card_x, cursor_cards_y, card_w, card_h, 8, is_selected ? accent : border_col);

        /* Radio Button / Secim Noktasi */
        fill_circle(card_x + 16, cursor_cards_y + (card_h / 2), 6, is_selected ? accent : border_col);
        if (is_selected) {
            fill_circle(card_x + 16, cursor_cards_y + (card_h / 2), 2, graphics_rgb(255, 255, 255));
        }

        ui_draw_text(cursor_names[i], card_x + 30, cursor_cards_y + 13, text_col, c_bg, 11.0f);
    }

    /* --- BÖLÜM 2: Duvar Kağıdı Seçimi --- */
    int section2_y = cursor_cards_y + card_h + 25;
    ui_draw_text("Masaustu Duvar Kagidi", content_x, section2_y, dim_text, bg_main, 12.0f);

    int wp_cards_y = section2_y + 20;
    int wp_card_w = 110;
    int wp_card_h = 60;

    const char *wp_names[3] = { "wallpaper.bmp", "wl2.bmp", "wl3.bmp" };

    for (int i = 0; i < 3; i++) {
        int card_x = content_x + (i * (wp_card_w + 10));
        bool is_selected = (g_settings.selected_wallpaper == i);

        int rel_mx = s_last_mouse_x - sidebar_w - 20;
        int rel_my = s_last_mouse_y - (wp_cards_y - cy);
        bool hover = (rel_mx >= (i * (wp_card_w + 10)) && rel_mx < (i * (wp_card_w + 10)) + wp_card_w &&
                      rel_my >= 0 && rel_my < wp_card_h);

        uint32_t c_bg = is_selected ? graphics_rgb(35, 55, 90) : (hover ? card_hover : card_bg);
        fill_rounded_rect_alpha(card_x, wp_cards_y, wp_card_w, wp_card_h, 8, c_bg, 255);
        graphics_draw_rounded_rect(card_x, wp_cards_y, wp_card_w, wp_card_h, 8, is_selected ? accent : border_col);

        /* Duvar Kagidi Onizleme Kutucugu (Simbolik Mockup) */
        graphics_fill_rect(card_x + 8, wp_cards_y + 8, wp_card_w - 16, 26, graphics_rgb(15, 16, 20));
        fill_circle(card_x + (wp_card_w / 2), wp_cards_y + 21, 5, is_selected ? accent : dim_text);

        /* Isim */
        ui_draw_text(wp_names[i], card_x + 10, wp_cards_y + 40, text_col, c_bg, 11.0f);
    }
}

/* -------------------------------------------------------------------------- */
/* ETKİLEŞİM VE GİRDİ YÖNETİMİ                                               */
/* -------------------------------------------------------------------------- */
void settings_handle_mouse(int win_id, int mouse_x, int mouse_y, uint8_t buttons) {
    s_last_mouse_x = mouse_x;
    s_last_mouse_y = mouse_y;

    if (win_id != g_settings.win_id) return;
    if (!(buttons & 0x01)) return; /* Sadece sol tiklama */

    int sidebar_w = 140;
    int content_x = sidebar_w + 20;

    /* 1. Imlec Secim Tiklamalari */
    int cursor_cards_y = 75;
    int card_w = 160;
    int card_h = 42;

    if (mouse_y >= cursor_cards_y && mouse_y < cursor_cards_y + card_h) {
        for (int i = 0; i < 2; i++) {
            int card_x = content_x + (i * (card_w + 12));
            if (mouse_x >= card_x && mouse_x < card_x + card_w) {
                if (g_settings.selected_cursor != i) {
                    g_settings.selected_cursor = i;
                    
                    /* Sistemde Imleci Guncelle */
                    if (gui_set_cursor != 0) {
                        gui_set_cursor(s_cursor_paths[i]);
                    }
                    gui_notify("Imlec degistirildi.");
                }
                return;
            }
        }
    }

    /* 2. Duvar Kagidi Secim Tiklamalari */
    int wp_cards_y = 182;
    int wp_card_w = 110;
    int wp_card_h = 60;

    if (mouse_y >= wp_cards_y && mouse_y < wp_cards_y + wp_card_h) {
        for (int i = 0; i < 3; i++) {
            int card_x = content_x + (i * (wp_card_w + 10));
            if (mouse_x >= card_x && mouse_x < card_x + wp_card_w) {
                if (g_settings.selected_wallpaper != i) {
                    g_settings.selected_wallpaper = i;
                    
                    /* Sistemde Duvar Kagidini Guncelle */
                    if (gui_set_wallpaper != 0) {
                        gui_set_wallpaper(s_wallpaper_paths[i]);
                    }
                    gui_notify("Duvar kagidi degistirildi.");
                }
                return;
            }
        }
    }
}

/* -------------------------------------------------------------------------- */
/* LIFECYCLE MANAGEMENT                                                       */
/* -------------------------------------------------------------------------- */
static void settings_on_close(int win_id) {
    (void)win_id;
    g_settings.active = false;
    s_last_mouse_x = -1;
    s_last_mouse_y = -1;
}

void settings_open(void) {
    if (g_settings.active) return;

    g_settings.current_tab = 0;
    
    /* Pencere Boyutu: 540x320 */
    int win = opa_window_create("Ayarlar", 540, 320, (void*)settings_draw);
    if (win < 0) return;

    g_settings.win_id = win;
    g_settings.active = true;

    /* Callback baglantilari */
    gui_set_mouse_callback(win, settings_handle_mouse);
    gui_set_close_callback(win, settings_on_close);
}

void settings_close(void) {
    if (g_settings.active) {
        close_window(g_settings.win_id);
        g_settings.active = false;
    }
}