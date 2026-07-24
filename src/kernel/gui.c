#include "kernel/gui.h"
#include "kernel/graphics.h"
#include "kernel/assets.h"
#include "kernel/bmp.h"
#include "kernel/terminal.h"
#include "kernel/keyboard.h"
#include "kernel/kstring.h"
#include "kernel/io.h"
#include "kernel/vfs.h"
#include "kernel/opa.h"
#include "freetype/kayaos/kayaos_freetype.h" 
#include "kernel/rtc.h"
#include "kernel/notepad.h"

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifndef GUI_MENUBAR_HEIGHT
#define GUI_MENUBAR_HEIGHT 22
#endif

/* Harici Uygulama Başlatıcı Tanımları */
extern void kterm_open(void);
extern void fm_open(void);
extern void fm_handle_mouse(int win_id, int mouse_x, int mouse_y, uint8_t buttons);
extern void notepad_handle_mouse(int win_id, int mouse_x, int mouse_y, uint8_t buttons);

/* graphics.c icinde tanimli ama header'a eklenmemis olabilir */
extern void fill_rounded_rect_alpha(int x, int y, int w, int h, int r, uint32_t color, uint8_t alpha);
extern void opa_window_on_closed(int win_id);

#define GUI_START_MENU_MAX_ITEMS 8
#define GUI_START_ICON_SIZE    32

/* Forward Declarations */
void notepad_open(const char *path);
void notepad_close(void);
void notepad_new(void);
bool notepad_save_file(const char *path);
bool notepad_load_file(const char *path);

/* =========================================================================
 * MENUBAR VE DIALOG (KAYDETME İLETİŞİM KUTUSU) SİSTEMİ
 * ========================================================================= */

typedef void (*gui_save_dialog_cb_t)(const char *filename, bool saved);

static bool g_save_dialog_open = false;
static char g_save_dialog_filename[128] = "yeni_dosya.txt";

bool gui_save_dialog_is_open(void) {
    return g_save_dialog_open;
}

void gui_show_save_dialog(const char *default_filename, int parent_win_id, gui_save_dialog_cb_t cb) {
    (void)parent_win_id;
    g_save_dialog_open = true;

    if (default_filename && k_strlen(default_filename) > 0) {
        k_strlcpy(g_save_dialog_filename, default_filename, sizeof(g_save_dialog_filename));
    } else {
        k_strlcpy(g_save_dialog_filename, "yeni_dosya.txt", sizeof(g_save_dialog_filename));
    }

    if (cb) {
        cb(g_save_dialog_filename, true);
    }

    g_save_dialog_open = false;
}

void gui_menubar_init(gui_menubar_t *mb) {
    if (!mb) return;
    k_memset(mb, 0, sizeof(gui_menubar_t));
    mb->active_menu = -1;
}

/* 1. Menü Ekleme */
int gui_menubar_add_menu(gui_menubar_t *mb, const char *title) {
    if (!mb || !title || mb->header_count >= GUI_MENU_MAX_TITLES) return -1;

    int idx = mb->header_count;
    k_strlcpy(mb->headers[idx].title, title, sizeof(mb->headers[idx].title));
    mb->headers[idx].item_count = 0;

    // X Offset Hesaplama: İlk eleman 6px içeriden başlar, diğerleri öncekinin genişliğine göre kayar
    if (idx == 0) {
        mb->headers[idx].x_offset = 6;
    } else {
        // Her karakteri ortalama 8px kabul edip üzerine 16px padding ekliyoruz
        int prev_len = k_strlen(mb->headers[idx - 1].title);
        int prev_width = (prev_len * 8) + 16;
        mb->headers[idx].x_offset = mb->headers[idx - 1].x_offset + prev_width;
    }

    mb->header_count++;
    return idx;
}

/* 2. Menü Elemanı Ekleme */
bool gui_menubar_add_item(gui_menubar_t *mb, int menu_idx, const char *label, menu_item_callback_t cb) {
    if (!mb || menu_idx < 0 || menu_idx >= mb->header_count) return false;

    gui_menu_header_t *m = &mb->headers[menu_idx];
    if (m->item_count >= GUI_MENU_MAX_ITEMS) return false;

    gui_menu_item_t *item = &m->items[m->item_count];
    if (label) {
        k_strlcpy(item->title, label, sizeof(item->title));
    } else {
        item->title[0] = '\0';
    }
    item->callback = cb;
    item->is_separator = false;
    item->enabled = true;
    m->item_count++;
    return true;
}

/* 3. Ayırıcı (Separator) Ekleme */
bool gui_menubar_add_separator(gui_menubar_t *mb, int menu_idx) {
    if (!mb || menu_idx < 0 || menu_idx >= mb->header_count) return false;

    gui_menu_header_t *m = &mb->headers[menu_idx];
    if (m->item_count >= GUI_MENU_MAX_ITEMS) return false;

    gui_menu_item_t *item = &m->items[m->item_count];
    item->title[0] = '\0';
    item->callback = NULL;
    item->is_separator = true;
    item->enabled = false;
    m->item_count++;
    return true;
}

/* 4. Menü Çubuğunu Çizme */
void gui_draw_menubar(gui_menubar_t *mb, int x, int y, int w) {
    if (!mb) return;

    // 1. Menü başlıklarını ekrana çiz
    for (int i = 0; i < mb->header_count; i++) {
        gui_menu_header_t *m = &mb->headers[i];
        int header_x = x + m->x_offset;

        // Başlık metnini çiz
        ui_draw_text(m->title, header_x + 8, y + 4,
                     graphics_rgb(255, 255, 255), graphics_rgb(40, 40, 45), 12.0f);
    }

    // 2. Aktif (açık) olan alt menüyü (dropdown) çiz
    if (mb->active_menu >= 0 && mb->active_menu < mb->header_count) {
        gui_menu_header_t *m = &mb->headers[mb->active_menu];

        // Dropdown menünün başlangıç koordinatları ve eleman yüksekliği
        int drop_x = x + m->x_offset;
        int drop_y = y + GUI_MENUBAR_HEIGHT;
        int item_height = 20; // Her bir alt elemanın yüksekliği (piksel)

        for (int j = 0; j < m->item_count; j++) {
            gui_menu_item_t *item = &m->items[j];
            
            // Elemanın Y koordinat hesabı
            int item_y = drop_y + (j * item_height);

            if (!item->is_separator) {
                // Alt menü elemanının metnini çiz
                ui_draw_text(item->title, drop_x + 12, item_y + 3,
                             graphics_rgb(20, 20, 25), graphics_rgb(245, 246, 248), 12.0f);
            } else {
                // Çizgi / Ayırıcı (Separator) çizim mantığınız varsa buraya ekleyebilirsiniz
            }
        }
    }
}

/* 5. Fare Girdilerini İşleme */
bool gui_menubar_handle_mouse(gui_menubar_t *mb, int mouse_x, int mouse_y, uint8_t buttons, bool is_click) {
    if (!mb) return false;

    if (mb->active_menu >= 0 && mb->active_menu < mb->header_count) {
        gui_menu_header_t *m = &mb->headers[mb->active_menu];
        // ... Açık menü etkileşim kodlarınız ...
    }

    for (int i = 0; i < mb->header_count; i++) {
        gui_menu_header_t *m = &mb->headers[i];
        // ... Başlık tıklama / hover kontrol kodlarınız ...
    }

    return false;
}


/* =========================================================================
 * BAŞLAT MENÜSÜ VERİ YAPILARI VE CACHE
 * ========================================================================= */

typedef struct {
    char display_name[24];
    char full_path[64];
    const asset_t *icon;
} start_menu_item_t;

static start_menu_item_t g_start_menu_cache[GUI_START_MENU_MAX_ITEMS];
static int g_start_menu_cache_count = 0;

static int scan_desktop_apps(start_menu_item_t *out, int max_items);

/* =========================================================================
 * NATIVE WIDGET (BUTTON & TEXTBOX) TANIMLARI
 * ========================================================================= */
typedef enum {
    GUI_WIDGET_BUTTON,
    GUI_WIDGET_TEXTBOX
} gui_widget_type_t;

typedef struct gui_widget {
    gui_widget_type_t type;
    int x, y, w, h;
    char label[64];
    char text[128];
    bool is_hovered;
    bool is_pressed;
    bool is_focused;
    void (*on_click)(void);
} gui_widget_t;

/* =========================================================================
 * SERI PORT (COM1) DEBUG LOGGER
 * ========================================================================= */
#define KSERIAL_COM1 0x3F8

static void kserial_init(void) {
    outb(KSERIAL_COM1 + 1, 0x00);
    outb(KSERIAL_COM1 + 3, 0x80);
    outb(KSERIAL_COM1 + 0, 0x03);
    outb(KSERIAL_COM1 + 1, 0x00);
    outb(KSERIAL_COM1 + 3, 0x03);
    outb(KSERIAL_COM1 + 2, 0xC7);
    outb(KSERIAL_COM1 + 4, 0x0B);
}

static bool kserial_tx_empty(void) {
    return (inb(KSERIAL_COM1 + 5) & 0x20) != 0;
}

static void kserial_putc(char c) {
    while (!kserial_tx_empty()) { }
    outb(KSERIAL_COM1, (uint8_t)c);
}

static void kserial_write(const char *s) {
    while (*s) {
        if (*s == '\n') kserial_putc('\r');
        kserial_putc(*s++);
    }
}

static void kserial_write_dec(uint32_t value) {
    char buf[11];
    int i = 0;
    if (value == 0) { kserial_putc('0'); return; }
    while (value > 0 && i < 10) { buf[i++] = (char)('0' + value % 10); value /= 10; }
    while (i > 0) kserial_putc(buf[--i]);
}

/* =========================================================================
 * KayaOS Grafik Kabugu (GUI)
 * ========================================================================= */
#define GUI_MAX_WINDOWS        4
#define GUI_TOPBAR_HEIGHT      26
#define GUI_TITLEBAR_HEIGHT    28
#define GUI_DOCK_HEIGHT        64
#define GUI_DOCK_ICON          44
#define GUI_DOCK_GAP           14
#define GUI_DOCK_ICONS         4
#define GUI_STATUSBAR_HEIGHT   22
#define GUI_CURSOR_H           20

#define GUI_START_LABEL_X      10
#define GUI_START_LABEL_Y      3
#define GUI_START_LABEL_W      70
#define GUI_START_LABEL_H      20
#define GUI_START_MENU_W       220
#define GUI_START_ITEM_H       40

#define GUI_NOTIFY_MAX_LEN            96
#define GUI_NOTIFY_DURATION_FRAMES    180
#define GUI_NOTIFY_FADE_FRAMES        30

struct gui_window;
typedef void (*gui_draw_cb_t)(struct gui_window *win, int cx, int cy, int cw, int ch);

static gui_window_t windows[GUI_MAX_WINDOWS];
static int z_order[GUI_MAX_WINDOWS];
static int window_count = 0;

static uint32_t frame_counter = 0;
static bool last_mouse_down = false;
static int hovered_dock_icon = -1;

static int g_mouse_x = 0;
static int g_mouse_y = 0;
static bool g_mouse_down = false;

static int g_ui_font = -1;
static bool g_debug_font_found = false;
static size_t g_debug_font_size = 0;

static bool g_start_menu_open = false;
static char g_notify_text[GUI_NOTIFY_MAX_LEN];
static uint32_t g_notify_timer = 0;

typedef struct {
    const char *label;
    const char *opa_path;
} opa_app_entry_t;

static const opa_app_entry_t g_dock_apps[GUI_DOCK_ICONS] = {
    { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },
};

static int clampi(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static void fill_circle(int cx, int cy, int r, uint32_t color) {
    for (int dy = -r; dy <= r; dy++) {
        int remaining = r * r - dy * dy;
        if (remaining < 0) continue;
        int dx = 0;
        while ((dx + 1) * (dx + 1) <= remaining) dx++;
        graphics_fill_rect(cx - dx, cy + dy, dx * 2 + 1, 1, color);
    }
}

static void ring_circle(int cx, int cy, int r, uint32_t color) {
    for (int dy = -r; dy <= r; dy++) {
        int remaining_out = r * r - dy * dy;
        if (remaining_out < 0) continue;
        int dx_out = 0;
        while ((dx_out + 1) * (dx_out + 1) <= remaining_out) dx_out++;

        int remaining_in = (r - 1) * (r - 1) - dy * dy;
        int dx_in = -1;
        if (remaining_in >= 0) {
            dx_in = 0;
            while ((dx_in + 1) * (dx_in + 1) <= remaining_in) dx_in++;
        }

        for (int dx = dx_in + 1; dx <= dx_out; dx++) {
            graphics_draw_pixel(cx + dx, cy + dy, color);
            graphics_draw_pixel(cx - dx, cy + dy, color);
        }
    }
}

typedef void (*gui_key_cb_t)(int win_id, char ch, uint8_t scancode);
typedef void (*gui_close_cb_t)(int win_id);

static gui_key_cb_t   g_key_callbacks[GUI_MAX_WINDOWS]   = { NULL };
static gui_close_cb_t g_close_callbacks[GUI_MAX_WINDOWS] = { NULL };

void gui_set_key_callback(int win_id, gui_key_cb_t cb) {
    if (win_id >= 0 && win_id < GUI_MAX_WINDOWS) {
        g_key_callbacks[win_id] = cb;
    }
}

void gui_set_close_callback(int win_id, gui_close_cb_t cb) {
    if (win_id >= 0 && win_id < GUI_MAX_WINDOWS) {
        g_close_callbacks[win_id] = cb;
    }
}

static const int8_t wave_table[16] = {
    0, 3, 6, 8, 8, 8, 6, 3, 0, -3, -6, -8, -8, -8, -6, -3
};

static void launch_notepad(void) {
    notepad_open(NULL); 
}

static void bring_to_front(int z_idx) {
    if (z_idx == window_count - 1) return;
    int id = z_order[z_idx];
    for (int i = z_idx; i < window_count - 1; i++) {
        z_order[i] = z_order[i + 1];
    }
    z_order[window_count - 1] = id;
}

void close_window(int idx) {
    if (idx < 0 || idx >= GUI_MAX_WINDOWS || !windows[idx].open) return;
    
    if (g_close_callbacks[idx] != NULL) {
        g_close_callbacks[idx](idx);
        g_close_callbacks[idx] = NULL;
    }
    g_key_callbacks[idx] = NULL;

    opa_window_on_closed(idx);
    windows[idx].open = false;

    int found = -1;
    for (int i = 0; i < window_count; i++) {
        if (z_order[i] == idx) { found = i; break; }
    }
    if (found >= 0) {
        for (int i = found; i < window_count - 1; i++) {
            z_order[i] = z_order[i + 1];
        }
        window_count--;
    }
}

void opt_log(const char *msg) {
    kserial_write("[OPT_LOG] ");
    kserial_write(msg);
    kserial_write("\n");
}

void opt_log_val(const char *msg, uint32_t val) {
    kserial_write("[OPT_LOG] ");
    kserial_write(msg);
    kserial_write(" = ");
    kserial_write_dec(val);
    kserial_write("\n");
}

void gui_dispatch_key(char ch, uint8_t scancode) {
    opt_log("--- KEY EVENT DISPATCH ---");
    opt_log_val("Gelen ASCII (dec)", (uint8_t)ch);
    opt_log_val("Gelen Scancode", scancode);
    opt_log_val("window_count", window_count);

    if (window_count <= 0) {
        opt_log("HATA: Hic acik pencere yok!");
        return;
    }

    int active_win_id = z_order[window_count - 1];
    opt_log_val("Aktif Pencere ID", active_win_id);

    if (active_win_id >= 0 && active_win_id < GUI_MAX_WINDOWS) {
        opt_log_val("Pencere open durumu", windows[active_win_id].open);
        opt_log_val("Key Callback tanimli mi", g_key_callbacks[active_win_id] != NULL);

        if (windows[active_win_id].open && g_key_callbacks[active_win_id] != NULL) {
            opt_log("Callback tetikleniyor...");
            g_key_callbacks[active_win_id](active_win_id, ch, scancode);
        } else {
            opt_log("HATA: Aktif pencerenin key callback'i NULL veya pencere kapali!");
        }
    }
}

static void set_title(gui_window_t *win, const char *text) {
    int i = 0;
    while (text[i] != '\0' && i < (int)sizeof(win->title) - 1) {
        win->title[i] = text[i];
        i++;
    }
    win->title[i] = '\0';
}

static void draw_wallpaper(void) {
    int w = (int)graphics_width();
    int h = (int)graphics_height();
    int horizon_y = (h * 62) / 100;

    for (int y = 0; y < horizon_y; y++) {
        int t = (y * 255) / (horizon_y > 0 ? horizon_y : 1);
        int r = 20 + ((255 - 20) * t) / 255;
        int g = 24 + ((140 - 24) * t) / 255;
        int b = 58 + ((90  - 58) * t) / 255;
        graphics_fill_rect(0, y, w, 1, graphics_rgb((uint8_t)r, (uint8_t)g, (uint8_t)b));
    }

    for (int y = horizon_y; y < h; y++) {
        int span = h - horizon_y;
        int t = ((y - horizon_y) * 255) / (span > 0 ? span : 1);
        int r = 60 - (40 * t) / 255;
        int g = 30 - (25 * t) / 255;
        int b = 70 - (30 * t) / 255;
        if (r < 10) r = 10;
        if (g < 8)  g = 8;
        if (b < 20) b = 20;
        graphics_fill_rect(0, y, w, 1, graphics_rgb((uint8_t)r, (uint8_t)g, (uint8_t)b));
    }

    int sun_x = w / 2 + (int)wave_table[(frame_counter / 6) % 16];
    int sun_y = horizon_y - h / 6;
    for (int ring = 5; ring >= 1; ring--) {
        int extra = ring * 10;
        uint32_t glow = graphics_rgb(255, (uint8_t)(150 + extra), (uint8_t)(80 + extra));
        ring_circle(sun_x, sun_y, 30 + ring * 8, glow);
    }
    fill_circle(sun_x, sun_y, 28, graphics_rgb(255, 214, 150));

    for (int line = 0; line < 2; line++) {
        int base_y = horizon_y + 10 + line * 14;
        uint32_t line_color = graphics_rgb(255, (uint8_t)(180 - line * 40), (uint8_t)(120 - line * 30));
        for (int x = 0; x < w; x += 4) {
            int idx = ((x / 4) + (int)(frame_counter / 4) + line * 3) % 16;
            int offset = wave_table[idx] / 4;
            graphics_fill_rect(x, base_y + offset, 3, 1, line_color);
        }
    }
}

static void draw_debug_font_status(void) {
    if (!graphics_available()) return;

    graphics_fill_rect(10, 40, 20, 20,
        g_debug_font_found ? graphics_rgb(0, 255, 0) : graphics_rgb(255, 0, 0));
    graphics_fill_rect(35, 40, 20, 20,
        (g_ui_font >= 0) ? graphics_rgb(0, 255, 0) : graphics_rgb(255, 0, 0));

    if (g_debug_font_found) {
        int size_bar = (int)(g_debug_font_size / 4096);
        if (size_bar > 200) size_bar = 200;
        if (size_bar < 1) size_bar = 1;
        graphics_fill_rect(10, 65, size_bar, 6, graphics_rgb(80, 160, 255));
    }
}

void ui_draw_text(const char *text, int x, int y,
                          uint32_t foreground, uint32_t background,
                          float font_size) {
    if (g_ui_font >= 0) {
        kayaos_ft_draw_text(g_ui_font, text, x, y, foreground, background, font_size);
        return;
    }

    int scale = (int)(font_size / 8.0f + 0.5f);
    if (scale < 1) scale = 1;
    graphics_draw_text(x, y, text, foreground, background, scale);
}

static void draw_topbar(void) {
    int w = (int)graphics_width();
    int pulse = (int)(frame_counter % 80);
    if (pulse > 40) pulse = 80 - pulse;
    uint8_t base = (uint8_t)(20 + pulse / 2);

    uint32_t bar_color = graphics_rgb(base, (uint8_t)(base + 4), (uint8_t)(base + 14));
    graphics_fill_rect(0, 0, w, GUI_TOPBAR_HEIGHT, bar_color);
    graphics_fill_rect(0, GUI_TOPBAR_HEIGHT - 1, w, 1, graphics_rgb(0, 0, 0));

    bool start_hover = (g_mouse_x >= GUI_START_LABEL_X && g_mouse_x < GUI_START_LABEL_X + GUI_START_LABEL_W &&
                         g_mouse_y >= GUI_START_LABEL_Y && g_mouse_y < GUI_START_LABEL_Y + GUI_START_LABEL_H);

    if (start_hover || g_start_menu_open) {
        graphics_fill_rect(GUI_START_LABEL_X - 6, 2, GUI_START_LABEL_W, GUI_TOPBAR_HEIGHT - 4,
            graphics_rgb(52, 56, 72));
    }

    ui_draw_text("KayaOS", GUI_START_LABEL_X, 5, graphics_rgb(255, 255, 255), bar_color, 14.0f);

    rtc_time_t current_time;
    rtc_get_time(&current_time);

    char clock_text[6];
    clock_text[0] = (char)('0' + (current_time.hour / 10));
    clock_text[1] = (char)('0' + (current_time.hour % 10));
    clock_text[2] = ':';
    clock_text[3] = (char)('0' + (current_time.minute / 10));
    clock_text[4] = (char)('0' + (current_time.minute % 10));
    clock_text[5] = '\0';
    
    ui_draw_text(clock_text, w - 60, 5, graphics_rgb(255, 255, 255), bar_color, 14.0f);
}

/* BAŞLAT MENÜSÜ LİSTELEME (VFS İLE GÜNCELLENDİ) */
static int scan_desktop_apps(start_menu_item_t *out, int max_items) {
    int count = 0;
	
    /* Terminal */
    if (count < max_items) {
        k_strlcpy(out[count].display_name, "Terminal", sizeof(out[count].display_name));
        k_strlcpy(out[count].full_path, "builtin:terminal", sizeof(out[count].full_path));
        out[count].icon = asset_find("terminal.bmp");
        count++;
    }

    /* Dosya Yöneticisi */
    if (count < max_items) {
        k_strlcpy(out[count].display_name, "Dosya Yoneticisi", sizeof(out[count].display_name));
        k_strlcpy(out[count].full_path, "builtin:filemanager", sizeof(out[count].full_path));
        out[count].icon = asset_find("folder.bmp");
        count++;
    }
	
    /* Notepad */
    if (count < max_items) {
        k_strlcpy(out[count].display_name, "Notepad", sizeof(out[count].display_name));
        k_strlcpy(out[count].full_path, "builtin:notepad", sizeof(out[count].full_path));
        out[count].icon = asset_find("notepad.bmp");
        count++;
    }

    /* Dynamic /desktop .opa Uygulamaları (VFS Katmanı Kullanılıyor) */
    vfs_dir_entry_t entries[GUI_START_MENU_MAX_ITEMS];
    int entry_count = vfs_list("/desktop", entries, GUI_START_MENU_MAX_ITEMS);

    if (entry_count > 0) {
        for (int i = 0; i < entry_count && count < max_items; i++) {
            if (entries[i].type == VFS_NODE_FILE) {
                size_t len = k_strlen(entries[i].name);
                if (len > 4 && k_streq(entries[i].name + len - 4, ".opa")) {
                    start_menu_item_t *item = &out[count];

                    size_t base_len = len - 4;
                    if (base_len >= sizeof(item->display_name)) {
                        base_len = sizeof(item->display_name) - 1;
                    }
                    k_memcpy(item->display_name, entries[i].name, base_len);
                    item->display_name[base_len] = '\0';

                    item->full_path[0] = '\0';
                    k_strlcpy(item->full_path, "/desktop/", sizeof(item->full_path));
                    size_t prefix_len = k_strlen(item->full_path);
                    k_strlcpy(item->full_path + prefix_len, entries[i].name,
                              sizeof(item->full_path) - prefix_len);

                    char icon_name[24];
                    size_t icon_base_len = base_len;
                    if (icon_base_len > sizeof(icon_name) - 5) {
                        icon_base_len = sizeof(icon_name) - 5;
                    }
                    k_memcpy(icon_name, entries[i].name, icon_base_len);
                    icon_name[icon_base_len] = '\0';
                    k_strlcpy(icon_name + icon_base_len, ".bmp", sizeof(icon_name) - icon_base_len);
                    item->icon = asset_find(icon_name);

                    count++;
                }
            }
        }
    }

    return count;
}

static void start_menu_geometry(int *out_x, int *out_y, int *out_h, int item_count) {
    *out_x = 10;
    *out_y = GUI_TOPBAR_HEIGHT + 6;
    *out_h = (item_count > 0 ? item_count : 1) * GUI_START_ITEM_H + 16;
}

static void draw_start_menu(int mouse_x, int mouse_y) {
    if (!g_start_menu_open) return;

    int count = g_start_menu_cache_count;

    int menu_x, menu_y, menu_h;
    start_menu_geometry(&menu_x, &menu_y, &menu_h, count);

    fill_rounded_rect_alpha(menu_x, menu_y, GUI_START_MENU_W, menu_h, 12,
        graphics_rgb(26, 27, 34), 235);
    graphics_draw_rounded_rect(menu_x, menu_y, GUI_START_MENU_W, menu_h, 12,
        graphics_rgb(55, 58, 72));

    if (count == 0) {
        ui_draw_text("Uygulama yok", menu_x + 14, menu_y + 14,
            graphics_rgb(150, 150, 160), graphics_rgb(26, 27, 34), 13.0f);
        return;
    }

    for (int i = 0; i < count; i++) {
        int item_y = menu_y + 8 + i * GUI_START_ITEM_H;
        bool hover = (mouse_x >= menu_x && mouse_x < menu_x + GUI_START_MENU_W &&
                      mouse_y >= item_y && mouse_y < item_y + GUI_START_ITEM_H);

        if (hover) {
            graphics_fill_rect(menu_x + 4, item_y, GUI_START_MENU_W - 8, GUI_START_ITEM_H,
                graphics_rgb(46, 49, 62));
        }

        int icon_x = menu_x + 12;
        int icon_y = item_y + (GUI_START_ITEM_H - GUI_START_ICON_SIZE) / 2;

        if (g_start_menu_cache[i].icon != 0) {
            bmp_draw(g_start_menu_cache[i].icon->data, g_start_menu_cache[i].icon->size, icon_x, icon_y, true);
        } else {
            graphics_draw_rounded_rect(icon_x, icon_y, GUI_START_ICON_SIZE, GUI_START_ICON_SIZE, 6,
                graphics_rgb(70, 74, 90));
        }

        ui_draw_text(g_start_menu_cache[i].display_name, icon_x + GUI_START_ICON_SIZE + 10,
            item_y + (GUI_START_ITEM_H - 16) / 2,
            graphics_rgb(225, 225, 232), graphics_rgb(26, 27, 34), 14.0f);
    }
}

static bool handle_start_menu_click(int mouse_x, int mouse_y) {
    int count = g_start_menu_cache_count;

    int menu_x, menu_y, menu_h;
    start_menu_geometry(&menu_x, &menu_y, &menu_h, count);

    if (mouse_x < menu_x || mouse_x >= menu_x + GUI_START_MENU_W) return false;

    for (int i = 0; i < count; i++) {
        int item_y = menu_y + 8 + i * GUI_START_ITEM_H;
        if (mouse_y >= item_y && mouse_y < item_y + GUI_START_ITEM_H) {
            if (k_streq(g_start_menu_cache[i].full_path, "builtin:terminal")) {
                kterm_open();
            } else if (k_streq(g_start_menu_cache[i].full_path, "builtin:filemanager")) {
                fm_open();
            } else if (k_streq(g_start_menu_cache[i].full_path, "builtin:notepad")) {
                launch_notepad();
            } else {
                opa_run(0, g_start_menu_cache[i].full_path);
            }
            return true;
        }
    }
    return false;
}

static void refresh_start_menu_cache(void) {
    g_start_menu_cache_count = scan_desktop_apps(g_start_menu_cache, GUI_START_MENU_MAX_ITEMS);
}

static bool handle_start_button_click(int mouse_x, int mouse_y) {
    if (mouse_x >= GUI_START_LABEL_X && mouse_x < GUI_START_LABEL_X + GUI_START_LABEL_W &&
        mouse_y >= GUI_START_LABEL_Y && mouse_y < GUI_START_LABEL_Y + GUI_START_LABEL_H) {
        
        g_start_menu_open = !g_start_menu_open;
        
        if (g_start_menu_open) {
            refresh_start_menu_cache();
        }
        return true;
    }
    return false;
}

void gui_notify(const char *text) {
    if (text == 0) return;
    k_strlcpy(g_notify_text, text, sizeof(g_notify_text));
    g_notify_timer = GUI_NOTIFY_DURATION_FRAMES;
}

static void draw_notification(void) {
    if (g_notify_timer == 0) return;
    g_notify_timer--;

    int w = (int)graphics_width();
    int h = (int)graphics_height();
    int box_w = 380;
    int box_h = 36;
    int box_x = w / 2 - box_w / 2;
    int box_y = h - GUI_STATUSBAR_HEIGHT - GUI_DOCK_HEIGHT - 22 - box_h;

    uint8_t alpha = (g_notify_timer < GUI_NOTIFY_FADE_FRAMES)
        ? (uint8_t)((g_notify_timer * 220) / GUI_NOTIFY_FADE_FRAMES)
        : 220;

    fill_rounded_rect_alpha(box_x, box_y, box_w, box_h, 10, graphics_rgb(28, 30, 38), alpha);
    ui_draw_text(g_notify_text, box_x + 14, box_y + 10,
        graphics_rgb(230, 230, 236), graphics_rgb(28, 30, 38), 13.0f);
}

static void draw_traffic_lights(int x, int y, int mouse_x, int mouse_y) {
    static const char* base_icons[3]  = { "close.bmp", "min.bmp", "max.bmp" };
    static const char* hover_icons[3] = { "closehover.bmp", "minhover.bmp", "maxhover.bmp" };

    for (int i = 0; i < 3; i++) {
        int cx = x + 14 + (i * 18);
        int cy = y + GUI_TITLEBAR_HEIGHT / 2;
        
        int dx = mouse_x - cx;
        int dy = mouse_y - cy;
        bool hover = (dx * dx + dy * dy) <= 49;

        const char* icon_to_draw = hover ? hover_icons[i] : base_icons[i];
        bmp_draw_asset(icon_to_draw, cx - 7, cy - 7, true); 
    }
}

#define SHADOW_SIZE 15 

static void draw_window_shadow(int x, int y, int w, int h) {
    bmp_draw_asset("Shadow_Corner_TopLeft.bmp", x - SHADOW_SIZE, y - SHADOW_SIZE, true);
    bmp_draw_asset("Shadow_Corner_TopRight.bmp", x + w, y - SHADOW_SIZE, true);
    bmp_draw_asset("Shadow_Corner_BottomLeft.bmp", x - SHADOW_SIZE, y + h, true);
    bmp_draw_asset("Shadow_Corner_BottomRight.bmp", x + w, y + h, true);

    bmp_draw_stretched_asset("Shadow_Edge_Top.bmp", x, y - SHADOW_SIZE, w, SHADOW_SIZE, true);
    bmp_draw_stretched_asset("Shadow_Edge_Bottom.bmp", x, y + h, w, SHADOW_SIZE, true);
    bmp_draw_stretched_asset("Shadow_Edge_Left.bmp", x - SHADOW_SIZE, y, SHADOW_SIZE, h, true);
    bmp_draw_stretched_asset("Shadow_Edge_Right.bmp", x + w, y, SHADOW_SIZE, h, true);
}

void gui_draw_button(gui_widget_t *btn, int win_x, int win_y) {
    if (!btn) return;
    int abs_x = win_x + btn->x;
    int abs_y = win_y + btn->y;

    uint32_t bg_color;
    uint32_t border_color = graphics_rgb(70, 75, 95);

    if (btn->is_pressed) {
        bg_color = graphics_rgb(35, 75, 180);
    } else if (btn->is_hovered) {
        bg_color = graphics_rgb(65, 120, 240);
    } else {
        bg_color = graphics_rgb(48, 52, 68);
    }

    fill_rounded_rect_alpha(abs_x, abs_y, btn->w, btn->h, 6, bg_color, 255);
    graphics_draw_rounded_rect(abs_x, abs_y, btn->w, btn->h, 6, border_color);

    int text_len = (int)k_strlen(btn->label);
    int text_x = abs_x + (btn->w - (text_len * 8)) / 2; 
    if (text_x < abs_x + 6) text_x = abs_x + 6;
    int text_y = abs_y + (btn->h - 14) / 2;

    ui_draw_text(btn->label, text_x, text_y, graphics_rgb(240, 240, 245), bg_color, 14.0f);
}

void gui_draw_textbox(gui_widget_t *tb, int win_x, int win_y) {
    if (!tb) return;
    int abs_x = win_x + tb->x;
    int abs_y = win_y + tb->y;

    uint32_t bg_color = graphics_rgb(20, 21, 26);
    uint32_t border_color = tb->is_focused ? graphics_rgb(52, 120, 246) : graphics_rgb(60, 64, 78);

    graphics_fill_rect(abs_x, abs_y, tb->w, tb->h, bg_color);
    graphics_draw_rounded_rect(abs_x, abs_y, tb->w, tb->h, 4, border_color);

    const char *disp_text = (tb->text[0] != '\0') ? tb->text : tb->label;
    uint32_t text_color = (tb->text[0] != '\0') ? graphics_rgb(230, 230, 235) : graphics_rgb(100, 105, 120);

    ui_draw_text(disp_text, abs_x + 8, abs_y + (tb->h - 12) / 2, text_color, bg_color, 13.0f);

    if (tb->is_focused && (frame_counter / 30) % 2 == 0) {
        int cursor_x = abs_x + 8 + ((int)k_strlen(tb->text) * 8);
        graphics_fill_rect(cursor_x, abs_y + 5, 2, tb->h - 10, graphics_rgb(255, 255, 255));
    }
}

static void draw_windows(int mouse_x, int mouse_y) {
    for (int i = 0; i < window_count; i++) {
        gui_window_t *win = &windows[z_order[i]];
        if (!win->open) continue;
        bool focused = (i == window_count - 1);

        draw_window_shadow(win->x, win->y, win->w, win->h);

        uint32_t body = graphics_rgb(36, 38, 46);
        graphics_draw_rounded_rect(win->x, win->y, win->w, win->h, 14, body);

        uint32_t title_bg = focused ? graphics_rgb(46, 49, 60) : graphics_rgb(32, 33, 40);
        graphics_fill_rect(win->x + 14, win->y, win->w - 28, GUI_TITLEBAR_HEIGHT, title_bg);
        graphics_fill_rect(win->x, win->y + GUI_TITLEBAR_HEIGHT, win->w, 1, graphics_rgb(15, 15, 20));

        draw_traffic_lights(win->x, win->y, mouse_x, mouse_y);

        ui_draw_text(win->title, win->x + 70, win->y + 6,
            focused ? graphics_rgb(235, 235, 240) : graphics_rgb(150, 150, 158),
            title_bg, 14.0f);

        int content_y = win->y + GUI_TITLEBAR_HEIGHT + 1;
        int content_h = win->h - GUI_TITLEBAR_HEIGHT - 2;
        if (content_h > 0) {
            int content_x = win->x + 2;
            int content_w = win->w - 4;
            graphics_set_clip(content_x, content_y, content_w, content_h);
            graphics_fill_rect(content_x, content_y, content_w, content_h, graphics_rgb(26, 27, 33));

            if (win->is_app) {
                if (win->draw_cb != NULL) {
                    win->draw_cb(win, content_x, content_y, content_w, content_h);
                }
            } else {
                ui_draw_text("KayaOS bare-metal cekirdek uzerinde calisiyor.",
                    win->x + 16, content_y + 16,
                    graphics_rgb(210, 210, 215), graphics_rgb(26, 27, 33), 16.0f);
            }

            graphics_reset_clip();
        }
    }
}

static void draw_dock(int mouse_x, int mouse_y) {
    int w = (int)graphics_width();
    int h = (int)graphics_height();
    int total_width = GUI_DOCK_ICONS * GUI_DOCK_ICON + (GUI_DOCK_ICONS - 1) * GUI_DOCK_GAP;
    int dock_x = w / 2 - total_width / 2;
    int dock_y = h - GUI_STATUSBAR_HEIGHT - GUI_DOCK_HEIGHT - 10;

    fill_rounded_rect_alpha(dock_x - 16, dock_y, total_width + 32, GUI_DOCK_HEIGHT, 18,
        graphics_rgb(28, 30, 38), 190);

    hovered_dock_icon = -1;

    for (int i = 0; i < GUI_DOCK_ICONS; i++) {
        int icon_x = dock_x + i * (GUI_DOCK_ICON + GUI_DOCK_GAP);
        int icon_y = dock_y + (GUI_DOCK_HEIGHT - GUI_DOCK_ICON) / 2;
        int cx = icon_x + GUI_DOCK_ICON / 2;
        int cy = icon_y + GUI_DOCK_ICON / 2;

        int dx = mouse_x - cx;
        int dy = mouse_y - cy;
        bool hover = (dx * dx + dy * dy) <= (GUI_DOCK_ICON / 2) * (GUI_DOCK_ICON / 2);
        bool has_app = (g_dock_apps[i].opa_path != 0);

        int lift = (hover && has_app) ? 6 : 0;
        if (hover && has_app) hovered_dock_icon = i;

        uint32_t body_color = has_app
            ? (hover ? graphics_rgb(70, 110, 200) : graphics_rgb(50, 54, 66))
            : graphics_rgb(36, 37, 44);

        graphics_draw_rounded_rect(icon_x, icon_y - lift, GUI_DOCK_ICON, GUI_DOCK_ICON, 10, body_color);

        if (has_app) {
            char initial[2] = { g_dock_apps[i].label[0], '\0' };
            ui_draw_text(initial, cx - 5, icon_y - lift + GUI_DOCK_ICON / 2 - 9,
                graphics_rgb(235, 238, 245), body_color, 16.0f);
        }
    }
}

static bool handle_dock_click(int mouse_x, int mouse_y) {
    int w = (int)graphics_width();
    int h = (int)graphics_height();
    int total_width = GUI_DOCK_ICONS * GUI_DOCK_ICON + (GUI_DOCK_ICONS - 1) * GUI_DOCK_GAP;
    int dock_x = w / 2 - total_width / 2;
    int dock_y = h - GUI_STATUSBAR_HEIGHT - GUI_DOCK_HEIGHT - 10;

    if (mouse_y < dock_y || mouse_y >= dock_y + GUI_DOCK_HEIGHT) return false;

    for (int i = 0; i < GUI_DOCK_ICONS; i++) {
        if (g_dock_apps[i].opa_path == 0) continue;
        int icon_x = dock_x + i * (GUI_DOCK_ICON + GUI_DOCK_GAP);
        if (mouse_x >= icon_x && mouse_x < icon_x + GUI_DOCK_ICON) {
            opa_run(0, g_dock_apps[i].opa_path);
            return true;
        }
    }
    return false;
}

static void draw_statusbar(void) {
    int w = (int)graphics_width();
    int h = (int)graphics_height();
    int bar_y = h - GUI_STATUSBAR_HEIGHT;

    graphics_fill_rect(0, bar_y, w, GUI_STATUSBAR_HEIGHT, graphics_rgb(18, 19, 24));
    graphics_fill_rect(0, bar_y, w, 1, graphics_rgb(60, 62, 74));

    ui_draw_text("VFS Hazir", 10, bar_y + 4, 
        graphics_rgb(160, 220, 160), graphics_rgb(18, 19, 24), 13.0f);

    char win_count_text[8];
    win_count_text[0] = 'P';
    win_count_text[1] = ':';
    win_count_text[2] = (char)('0' + (window_count % 10));
    win_count_text[3] = '\0';
    
    ui_draw_text(win_count_text, w - 50, bar_y + 4, 
        graphics_rgb(180, 180, 190), graphics_rgb(18, 19, 24), 13.0f);
}

/* GÜNCELLENEN İMLEÇ ÇİZİM FONKSİYONU */
static void draw_cursor(int x, int y, bool pressed) {
    (void)pressed;
    bmp_draw_asset("cursor.bmp", x, y, true);
}

/* ---- GENEL GUI API'SI ---- */
void gui_init(void) {
    window_count = 0;
    frame_counter = 0;
    last_mouse_down = false;
    g_mouse_down = false;
    hovered_dock_icon = -1;
    g_start_menu_open = false;
    g_notify_timer = 0;
    g_notify_text[0] = '\0';

    for (int i = 0; i < GUI_MAX_WINDOWS; i++) {
        windows[i].open = false;
        windows[i].dragging = false;
        windows[i].is_app = false;
        windows[i].draw_cb = NULL;
        z_order[i] = -1;
    }

    kserial_init();
    kserial_write("\n[KayaOS/GUI] gui_init basladi\n");

    bool ft_init_ok = kayaos_ft_init();
    kserial_write("[FT] kayaos_ft_init(): ");
    kserial_write(ft_init_ok ? "OK\n" : "BASARISIZ\n");

    const asset_t* font_asset = asset_find("a.ttf"); 
    kserial_write("[FT] asset_find(\"a.ttf\"): ");
    kserial_write(font_asset != NULL ? "BULUNDU, boyut=" : "BULUNAMADI (NULL)\n");
    if (font_asset != NULL) {
        kserial_write_dec((uint32_t)font_asset->size);
        kserial_write(" byte\n");
    }
    
    if (font_asset != NULL) {
        g_ui_font = kayaos_ft_load_font_from_memory(font_asset->data, font_asset->size);
    } else {
        g_ui_font = -1; 
    }
    kserial_write("[FT] kayaos_ft_load_font_from_memory() handle: ");
    kserial_write_dec((uint32_t)(g_ui_font < 0 ? 0xFFFFFFFFu : (uint32_t)g_ui_font));
    kserial_write(g_ui_font >= 0 ? "  (BASARILI)\n" : "  (-1, BASARISIZ)\n");

    kserial_write("[GFX] graphics_available(): ");
    kserial_write(graphics_available() ? "TRUE\n" : "FALSE\n");
    kserial_write_dec(graphics_width());
    kserial_write("x");
    kserial_write_dec(graphics_height());
    kserial_write("\n");

    g_debug_font_found = (font_asset != NULL);
    g_debug_font_size  = (font_asset != NULL) ? font_asset->size : 0;

    /* Terminal baslatiliyor */
    kterm_open();

    kserial_write("[GUI] Start menu hazir -- Dahili uygulamalar ve /desktop klasorundeki .opa dosyalari listelenecek.\n");
}

void gui_update(int mouse_x, int mouse_y, bool mouse_clicked) {
    frame_counter++;

    g_mouse_x = mouse_x;
    g_mouse_y = mouse_y;
    g_mouse_down = mouse_clicked;

    bool click_just_pressed = mouse_clicked && !last_mouse_down;

    int dragging_idx = -1;
    for (int i = 0; i < window_count; i++) {
        if (windows[z_order[i]].dragging) { dragging_idx = z_order[i]; break; }
    }

    if (dragging_idx != -1) {
        if (mouse_clicked) {
            windows[dragging_idx].x = mouse_x - windows[dragging_idx].drag_off_x;
            windows[dragging_idx].y = mouse_y - windows[dragging_idx].drag_off_y;
            windows[dragging_idx].y = clampi(windows[dragging_idx].y, GUI_TOPBAR_HEIGHT,
                (int)graphics_height() - 100);
        } else {
            windows[dragging_idx].dragging = false;
        }
        last_mouse_down = mouse_clicked;
        return;
    }

    if (click_just_pressed) {
        if (handle_start_button_click(mouse_x, mouse_y)) {
            last_mouse_down = mouse_clicked;
            return;
        }

        if (g_start_menu_open) {
            handle_start_menu_click(mouse_x, mouse_y);
            g_start_menu_open = false;
            last_mouse_down = mouse_clicked;
            return;
        }

        if (handle_dock_click(mouse_x, mouse_y)) {
            last_mouse_down = mouse_clicked;
            return;
        }

        for (int i = window_count - 1; i >= 0; i--) {
            gui_window_t *win = &windows[z_order[i]];

            if (mouse_x >= win->x && mouse_x < win->x + win->w &&
                mouse_y >= win->y && mouse_y < win->y + win->h) {

                bring_to_front(i);

                if (mouse_y < win->y + GUI_TITLEBAR_HEIGHT) {
                    int close_cx = win->x + 14;
                    int close_cy = win->y + GUI_TITLEBAR_HEIGHT / 2;
                    int dx = mouse_x - close_cx;
                    int dy = mouse_y - close_cy;

                    if (dx * dx + dy * dy <= 49) {
                        close_window((int)(win - windows));
                    } else {
                        win->dragging = true;
                        win->drag_off_x = mouse_x - win->x;
                        win->drag_off_y = mouse_y - win->y;
                    }
                } else {
                    int win_id = (int)(win - windows);
                    int rel_x = mouse_x - win->x;
                    int rel_y = mouse_y - (win->y + GUI_TITLEBAR_HEIGHT + 1);
                    fm_handle_mouse(win_id, rel_x, rel_y, 0x01);
                    notepad_handle_mouse(win_id, rel_x, rel_y, 0x01);
                }
                break;
            }
        }
    }

    last_mouse_down = mouse_clicked;
}

void gui_draw(void) {
    if (!graphics_available()) {
        return;
    }
    
    draw_wallpaper();
    draw_debug_font_status(); 
    draw_topbar();
    draw_windows(g_mouse_x, g_mouse_y);
    draw_dock(g_mouse_x, g_mouse_y);
    draw_statusbar();
    draw_start_menu(g_mouse_x, g_mouse_y); 
    draw_notification();
    draw_cursor(g_mouse_x, g_mouse_y, g_mouse_down);
    graphics_present();
}

int opa_window_create(const char *title, int custom_w, int custom_h, gui_draw_cb_t draw_cb) {
    if (window_count >= GUI_MAX_WINDOWS) {
        gui_notify("Pencere limiti doldu!");
        return -1;
    }

    int idx = -1;
    for (int i = 0; i < GUI_MAX_WINDOWS; i++) {
        if (!windows[i].open) { idx = i; break; }
    }
    if (idx < 0) return -1;

    int screen_w = (int)graphics_width();
    int screen_h = (int)graphics_height();

    int win_w = (custom_w > 0) ? custom_w : 400;
    int win_h = (custom_h > 0) ? custom_h : 280;

    int cascade = (window_count % GUI_MAX_WINDOWS) * 20;

    windows[idx].x = screen_w / 2 - win_w / 2 + cascade;
    windows[idx].y = screen_h / 2 - win_h / 2 + cascade;
    windows[idx].w = win_w;
    windows[idx].h = win_h;
    windows[idx].open = true;
    windows[idx].dragging = false;
    windows[idx].is_app = true;
    windows[idx].draw_cb = draw_cb;
    set_title(&windows[idx], (title != 0) ? title : "OPA App");

    z_order[window_count] = idx;
    window_count++;
    bring_to_front(window_count - 1);

    return idx;
}