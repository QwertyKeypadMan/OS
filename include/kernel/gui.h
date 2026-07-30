#ifndef LTHU_GUI_H
#define LTHU_GUI_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================================
 * SABİTLER (CONSTANTS)
 * ========================================================================= */
#define GUI_MENU_MAX_ITEMS   12
#define GUI_MENU_MAX_TITLES  8
#define GUI_MENUBAR_HEIGHT   24

/* =========================================================================
 * VERİ YAPILARI VE TİP TANIMLARI
 * ========================================================================= */
struct gui_window;

typedef void (*gui_draw_cb_t)(struct gui_window *win, int cx, int cy, int cw, int ch);
typedef void (*menu_item_callback_t)(void);
typedef void (*save_dialog_cb_t)(const char *filename, bool saved);
typedef void (*gui_key_cb_t)(int win_id, char ch, uint8_t scancode);
typedef void (*gui_close_cb_t)(int win_id);
typedef void (*gui_mouse_cb_t)(int win_id, int mouse_x, int mouse_y, uint8_t buttons); /* YENİ */

/* 2. SONRA FONKSİYON BİLDİRİMLERİ */
void gui_set_key_callback(int win_id, gui_key_cb_t cb);
void gui_set_close_callback(int win_id, gui_close_cb_t cb);
void gui_set_mouse_callback(int win_id, gui_mouse_cb_t cb); /* YENİ */

void gui_set_cursor(const char *bmp_path);
void gui_set_wallpaper(const char *bmp_path);
bool gui_needs_redraw(void);

/* Pencere Yapısı */
typedef struct gui_window {
    int x, y, w, h;
    char title[32];
    bool open;
    bool dragging;
    int drag_off_x, drag_off_y;
    bool is_app;
    gui_draw_cb_t draw_cb;
} gui_window_t;

/* Menü Elemanı Yapısı */
typedef struct {
    char title[32];
    menu_item_callback_t callback;
    bool is_separator;
    bool enabled;
} gui_menu_item_t;


typedef struct {
    char title[32];
    int x, w;
    gui_menu_item_t items[16];
    int item_count;
} gui_menu_t;

/* Menü Başlığı Yapısı */
typedef struct {
    char title[32];
    gui_menu_item_t items[GUI_MENU_MAX_ITEMS];
    int item_count;
    int x_offset;
    int width;
} gui_menu_header_t;

/* Menü Çubuğu (MenuBar) Yapısı */
typedef struct {
    gui_menu_header_t headers[GUI_MENU_MAX_TITLES];
    int header_count;
    int active_menu;   /* -1: Açık menü yok, >=0: Açık olan menünün indeksi */
    int hovered_item;  /* -1: Vurgulanan öge yok, >=0: Vurgulanan alt öge indeksi */
    int height;
} gui_menubar_t;

/* =========================================================================
 * TEMEL GUI VE ÇİZİM API FONKSİYONLARI
 * ========================================================================= */

/**
 * @brief Lthu OS Grafik Arayüz (GUI) alt sistemini başlatır.
 *        Pencere yöneticisini ve LVGL motorunu ayağa kaldırır.
 */
void gui_init(void);

/**
 * @brief Fare girdilerini ve GUI durumunu günceller.
 */
void gui_update(int mouse_x, int mouse_y, bool mouse_clicked);

/**
 * @brief Tüm masaüstünü, görev çubuğunu, pencereleri ve LVGL içeriklerini ekrana çizer.
 */
void gui_draw(void);

/**
 * @brief Ekrana belirtilen renk ve boyutta metin çizer.
 */
void ui_draw_text(const char *text, int x, int y,
                  uint32_t foreground, uint32_t background,
                  float font_size);

/* =========================================================================
 * PENCERE YÖNETİMİ VE OPA UYGULAMA API
 * ========================================================================= */

/**
 * @brief OPA uygulamaları için boş, başlığı verilmiş yeni bir pencere açar.
 * @return Başarılı olursa pencere indeksini (>=0), yer yoksa -1 döner.
 */
int opa_window_create(const char *title, int custom_w, int custom_h, gui_draw_cb_t draw_cb);

/**
 * @brief Belirtilen indeksteki pencereyi kapatır.
 */
void close_window(int idx);

/**
 * @brief Ekranın altında kısa süreli bir bildirim (toast) gösterir.
 */
void gui_notify(const char *text);

/* =========================================================================
 * MENUBAR API
 * ========================================================================= */
void gui_menubar_init(gui_menubar_t *mb);
int  gui_menubar_add_menu(gui_menubar_t *mb, const char *title);
bool gui_menubar_add_item(gui_menubar_t *mb, int menu_idx, const char *label, menu_item_callback_t cb);
bool gui_menubar_add_separator(gui_menubar_t *mb, int menu_idx);
void gui_draw_menubar(gui_menubar_t *mb, int x, int y, int w);
bool gui_menubar_handle_mouse(gui_menubar_t *mb, int mouse_x, int mouse_y, uint8_t buttons, bool is_click);

/* =========================================================================
 * SAVE DIALOG API
 * ========================================================================= */
void gui_show_save_dialog(const char *default_filename, int parent_win_id, save_dialog_cb_t cb);
bool gui_save_dialog_is_open(void);
int  gui_save_dialog_get_win_id(void);

#ifdef __cplusplus
}
#endif

#endif // LTHU_GUI_H