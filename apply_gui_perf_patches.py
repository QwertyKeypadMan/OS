#!/usr/bin/env python3
"""
KayaOS GUI - Oncelik 2 Yama Scripti
------------------------------------
WSL'de proje kokunde calistir:
    python3 apply_gui_priority2_patches.py /mnt/c/Users/armutt/Desktop/OS

Ne yapiyor:
  A) gui.c      -> Dock tamamen kaldiriliyor (dead code -- zaten hicbir
                    zaman gorsel olarak cizilmiyordu, sadece bos tiklama
                    kontrolu vardi)
  B) graphics.c -> graphics_draw_rounded_rect() ve fill_rounded_rect_alpha()
                    artik anti-aliased: kose kenarlarindaki "merdiven"
                    gorunumu kayboluyor
  C) gui.c      -> Pencereler artik anlik belirmiyor; opa_window_create()
                    ile acilan her pencere ~8 karede kucukten/saydamdan
                    tam boy/opaka "materialize" oluyor (kapanis animasyonu
                    KASITLI OLARAK eklenmedi -- draw_cb/close_callback
                    use-after-free riski var, hobi OS'te referans sayimi
                    olmadan guvenli degil; istersen ayri konusuruz)

Her yama tam string eslesmesi arar; bulamazsa/coklu bulursa atlar ve
neden atladigini yazar.
"""
import sys, os, glob

def find_one(root, filename):
    matches = glob.glob(os.path.join(root, "**", filename), recursive=True)
    if not matches:
        print(f"[UYARI] {filename} bulunamadi, atlaniyor.")
        return None
    if len(matches) > 1:
        print(f"[UYARI] {filename} birden fazla yerde bulundu, ilki kullaniliyor: {matches[0]}")
    return matches[0]

def apply_patch(path, old, new, label):
    with open(path, "r", encoding="utf-8") as f:
        content = f.read()
    count = content.count(old)
    if count == 0:
        print(f"  [ATLANDI] {label}: eski metin bulunamadi (dosya farkli olabilir)")
        return False
    if count > 1:
        print(f"  [ATLANDI] {label}: eski metin {count} kere goruldu (belirsiz), elle uygula")
        return False
    content = content.replace(old, new, 1)
    with open(path, "w", encoding="utf-8") as f:
        f.write(content)
    print(f"  [OK] {label}")
    return True

def main():
    if len(sys.argv) < 2:
        print("Kullanim: python3 apply_gui_priority2_patches.py <proje_kok_dizini>")
        sys.exit(1)
    root = sys.argv[1]

    gui_c = find_one(root, "gui.c")
    graphics_c = find_one(root, "graphics.c")

    # =================================================================
    # A) DOCK'U TAMAMEN KALDIR (gui.c)
    # =================================================================
    if gui_c:
        print(f"\n== gui.c: DOCK KALDIRMA ({gui_c}) ==")

        old = """#define GUI_MAX_WINDOWS        4
#define GUI_TOPBAR_HEIGHT      26
#define GUI_TITLEBAR_HEIGHT    28
#define GUI_DOCK_HEIGHT        64
#define GUI_DOCK_ICON          44
#define GUI_DOCK_GAP           14
#define GUI_DOCK_ICONS         4
#define GUI_STATUSBAR_HEIGHT   22
#define GUI_CURSOR_H           20"""
        new = """#define GUI_MAX_WINDOWS        4
#define GUI_TOPBAR_HEIGHT      26
#define GUI_TITLEBAR_HEIGHT    28
#define GUI_STATUSBAR_HEIGHT   22
#define GUI_CURSOR_H           20
#define GUI_WINDOW_ANIM_FRAMES 8   /* YENİ: pencere açılış animasyonu kare sayısı */"""
        apply_patch(gui_c, old, new, "GUI_DOCK_* makrolari silindi, GUI_WINDOW_ANIM_FRAMES eklendi")

        old = """static int hovered_dock_icon = -1;

static int g_mouse_x = 0;"""
        new = """
static int g_mouse_x = 0;"""
        apply_patch(gui_c, old, new, "hovered_dock_icon degiskeni silindi")

        old = """    last_mouse_down = false;
    g_mouse_down = false;
    hovered_dock_icon = -1;
    g_start_menu_open = false;"""
        new = """    last_mouse_down = false;
    g_mouse_down = false;
    g_start_menu_open = false;"""
        apply_patch(gui_c, old, new, "gui_init() icindeki hovered_dock_icon resetlemesi silindi")

        old = """typedef struct {
    const char *label;
    const char *opa_path;
} opa_app_entry_t;

static const opa_app_entry_t g_dock_apps[GUI_DOCK_ICONS] = {
    { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },
};

static int clampi(int v, int lo, int hi) {"""
        new = """static int clampi(int v, int lo, int hi) {"""
        apply_patch(gui_c, old, new, "opa_app_entry_t / g_dock_apps silindi")

        old = """static bool handle_dock_click(int mouse_x, int mouse_y) {
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

"""
        new = ""
        apply_patch(gui_c, old, new, "handle_dock_click() fonksiyonu silindi")

        old = """        if (handle_dock_click(mouse_x, mouse_y)) {
            last_mouse_down = mouse_clicked;
            return;
        }

        if (handle_statusbar_click(mouse_x, mouse_y)) {"""
        new = """        if (handle_statusbar_click(mouse_x, mouse_y)) {"""
        apply_patch(gui_c, old, new, "handle_dock_click() cagri noktasi silindi")

        old = """    int box_y = h - GUI_STATUSBAR_HEIGHT - GUI_DOCK_HEIGHT - 22 - box_h;"""
        new = """    int box_y = h - GUI_STATUSBAR_HEIGHT - 26 - box_h; /* dock artik yok, bosluk daraltildi */"""
        apply_patch(gui_c, old, new, "draw_notification() konumu dock'suz duzene gore ayarlandi")

    # =================================================================
    # B) ANTI-ALIASING (graphics.c)
    # =================================================================
    if graphics_c:
        print(f"\n== graphics.c: ANTI-ALIASING ({graphics_c}) ==")

        old = """void graphics_draw_triangle(int x0, int y0, int x1, int y1, int x2, int y2, uint32_t color) {
    graphics_draw_line(x0, y0, x1, y1, color);
    graphics_draw_line(x1, y1, x2, y2, color);
    graphics_draw_line(x2, y2, x0, y0, color);
}"""
        new = """void graphics_draw_triangle(int x0, int y0, int x1, int y1, int x2, int y2, uint32_t color) {
    graphics_draw_line(x0, y0, x1, y1, color);
    graphics_draw_line(x1, y1, x2, y2, color);
    graphics_draw_line(x2, y2, x0, y0, color);
}

/* REIS FIX (AA): tek bir pikseli arka planla "coverage" (0.0-1.0) oraninda
 * karistirarak yazar. coverage>=1 ise dogrudan (blend'siz) yazar -- yani
 * govde/kenar gibi tam-kapsama pikselleri hicbir ekstra maliyete girmiyor,
 * sadece kose kavis siniri (~1 piksellik bant) bu blend yolunu kullaniyor. */
static void draw_pixel_aa(int x, int y, uint32_t color, float coverage) {
    if (coverage <= 0.0f) return;
    if (coverage >= 1.0f) {
        graphics_draw_pixel(x, y, color);
        return;
    }
    if (!fb_ready || x < clip_x0 || x >= clip_x1 || y < clip_y0 || y >= clip_y1) return;

    uint8_t *target = backbuffer_enabled ? back_buffer : framebuffer;
    uint32_t bypp = fb_bpp / 8;
    uint8_t *p = target + (uint32_t)y * fb_pitch + (uint32_t)x * bypp;

    uint32_t bg = unpack_color(read_native_pixel(p));
    uint32_t blended = blend_rgb(bg, color, (uint32_t)(coverage * 255.0f));
    write_native_pixel(p, pack_color(blended));

    if (backbuffer_enabled) {
        mark_dirty(x, y, x + 1, y + 1);
    }
}

/* REIS FIX (AA): is_inside_rounded_rect'in bool halinin yerine, kose
 * bolgelerinde 0.0-1.0 arasi yumusak bir "coverage" dondurur -- boylece
 * kose kenarlari sert "merdiven" gibi degil, anti-aliased gorunur. Duz
 * kenar/govde pikselleri her zaman 1.0 dondurur. */
static float rounded_rect_coverage(int px, int py, int x, int y, int w, int h, int r) {
    if (px < x || px >= x + w || py < y || py >= y + h) return 0.0f;

    int ccx = 0, ccy = 0;
    bool in_corner = true;

    if (px < x + r && py < y + r)                { ccx = x + r;     ccy = y + r; }
    else if (px >= x + w - r && py < y + r)       { ccx = x + w - r; ccy = y + r; }
    else if (px < x + r && py >= y + h - r)       { ccx = x + r;     ccy = y + h - r; }
    else if (px >= x + w - r && py >= y + h - r)  { ccx = x + w - r; ccy = y + h - r; }
    else { in_corner = false; }

    if (!in_corner) return 1.0f;

    int dx = px - ccx;
    int dy = py - ccy;
    float dist = k_sqrt((float)(dx * dx + dy * dy));
    float coverage = (float)r - dist + 0.5f;
    if (coverage < 0.0f) coverage = 0.0f;
    if (coverage > 1.0f) coverage = 1.0f;
    return coverage;
}"""
        apply_patch(graphics_c, old, new, "draw_pixel_aa() ve rounded_rect_coverage() eklendi")

        old = """    /* Üst ve alt köşe bantları */
    for (int band = 0; band < 2; band++) {
        for (int row = 0; row < r; row++) {
            int cy = (band == 0) ? row : (h - r + row);
            int local_cy = (band == 0) ? row : (r - 1 - row);
            int dy = r - 1 - local_cy;
            int py = y + cy;

            /* Bu satırda dairenin içine giren en küçük sütunu bul (r küçük
             * olduğundan bu döngü ucuzdur, örn. r=16 için en fazla 16 adım) */
            int inset = r;
            for (int cx = 0; cx < r; cx++) {
                int dx = r - 1 - cx;
                if (dx * dx + dy * dy <= r * r) { inset = cx; break; }
            }

            for (int cx = inset; cx < r; cx++) {
                int dx = r - 1 - cx;
                if (dx * dx + dy * dy <= r * r) {
                    graphics_draw_pixel(x + cx, py, color);              /* sol köşe */
                    graphics_draw_pixel(x + w - 1 - cx, py, color);      /* sağ köşe (ayna) */
                }
            }

            if (w - 2 * r > 0) {
                graphics_fill_rect(x + r, py, w - 2 * r, 1, color);      /* ortadaki düz kısım */
            }
        }
    }
}"""
        new = """    /* Üst ve alt köşe bantları (REİS FIX: artık anti-aliased) */
    for (int band = 0; band < 2; band++) {
        for (int row = 0; row < r; row++) {
            int cy = (band == 0) ? row : (h - r + row);
            int py = y + cy;

            for (int cx = 0; cx < r; cx++) {
                float coverage = rounded_rect_coverage(x + cx, py, x, y, w, h, r);
                if (coverage > 0.0f) {
                    draw_pixel_aa(x + cx, py, color, coverage);              /* sol köşe */
                    draw_pixel_aa(x + w - 1 - cx, py, color, coverage);      /* sağ köşe (ayna) */
                }
            }

            if (w - 2 * r > 0) {
                graphics_fill_rect(x + r, py, w - 2 * r, 1, color);      /* ortadaki düz kısım */
            }
        }
    }
}"""
        apply_patch(graphics_c, old, new, "graphics_draw_rounded_rect() koseleri anti-aliased yapildi")

        old = """/* Alpha destekli Rounded Rect çizici */
void fill_rounded_rect_alpha(int x, int y, int w, int h, int r, uint32_t color, uint8_t alpha) {
    for (int iy = y; iy < y + h; iy++) {
        for (int ix = x; ix < x + w; ix++) {
            if (is_inside_rounded_rect(ix, iy, x, y, w, h, r)) {
                uint32_t bg = graphics_get_pixel(ix, iy); // Arka planı oku
                uint32_t mixed = blend_colors(color, bg, alpha); // Karıştır
                graphics_draw_pixel(ix, iy, mixed);
            }
        }
    }
}"""
        new = """/* Alpha destekli Rounded Rect çizici (REİS FIX: artık köşe kenarlarında
 * anti-aliased -- coverage, verilen alpha ile çarpılıyor, yani hem
 * saydamlık hem yumuşak kenar aynı anda çalışıyor). */
void fill_rounded_rect_alpha(int x, int y, int w, int h, int r, uint32_t color, uint8_t alpha) {
    for (int iy = y; iy < y + h; iy++) {
        for (int ix = x; ix < x + w; ix++) {
            float coverage = rounded_rect_coverage(ix, iy, x, y, w, h, r);
            if (coverage <= 0.0f) continue;
            uint32_t bg = graphics_get_pixel(ix, iy); // Arka planı oku
            uint8_t effective_alpha = (uint8_t)((float)alpha * coverage);
            uint32_t mixed = blend_colors(color, bg, effective_alpha); // Karıştır
            graphics_draw_pixel(ix, iy, mixed);
        }
    }
}"""
        apply_patch(graphics_c, old, new, "fill_rounded_rect_alpha() anti-aliased yapildi")

    # =================================================================
    # C) PENCERE ACILIS ANIMASYONU (gui.c)
    # =================================================================
    if gui_c:
        print(f"\n== gui.c: PENCERE ACILIS ANIMASYONU ({gui_c}) ==")

        old = """typedef struct {
    bool is_minimized;
    bool is_maximized;
    int saved_x;
    int saved_y;
    int saved_w;
    int saved_h;
} win_extra_t;"""
        new = """typedef struct {
    bool is_minimized;
    bool is_maximized;
    int saved_x;
    int saved_y;
    int saved_w;
    int saved_h;
    int anim_frame; /* YENİ: açılış animasyonu kare sayacı, bkz. GUI_WINDOW_ANIM_FRAMES */
} win_extra_t;"""
        apply_patch(gui_c, old, new, "win_extra_t'ye anim_frame eklendi")

        old = """    g_win_extra[idx].is_minimized = false;
    g_win_extra[idx].is_maximized = false;

    z_order[window_count] = idx;"""
        new = """    g_win_extra[idx].is_minimized = false;
    g_win_extra[idx].is_maximized = false;
    g_win_extra[idx].anim_frame = 0; /* YENİ: yeni pencere her zaman animasyonla acilir */

    z_order[window_count] = idx;"""
        apply_patch(gui_c, old, new, "opa_window_create() anim_frame sifirlamasi eklendi")

        old = """static void draw_windows(int mouse_x, int mouse_y) {
    for (int i = 0; i < window_count; i++) {
        int win_id = z_order[i];
        gui_window_t *win = &windows[win_id];
        
        if (!win->open || g_win_extra[win_id].is_minimized) continue;
        
        bool focused = (i == window_count - 1);

        draw_window_shadow(win->x, win->y, win->w, win->h);"""
        new = """static void draw_windows(int mouse_x, int mouse_y) {
    for (int i = 0; i < window_count; i++) {
        int win_id = z_order[i];
        gui_window_t *win = &windows[win_id];
        
        if (!win->open || g_win_extra[win_id].is_minimized) continue;
        
        bool focused = (i == window_count - 1);

        /* YENİ: Açılış animasyonu -- ilk GUI_WINDOW_ANIM_FRAMES karede
         * pencere küçükten/saydamdan tam boy/opaka "materialize" oluyor.
         * Bu süre boyunca icerik (draw_cb) hic cagirilmiyor -- sadece
         * govde + baslik gosteriliyor, boylece uygulamanin kendi cizimini
         * olceklendirmesi gerekmiyor. Animasyon bitince asagidaki normal
         * tam cizime geciliyor. */
        if (g_win_extra[win_id].anim_frame < GUI_WINDOW_ANIM_FRAMES) {
            float t = (float)g_win_extra[win_id].anim_frame / (float)GUI_WINDOW_ANIM_FRAMES;
            float scale = 0.85f + 0.15f * t;
            uint8_t alpha = (uint8_t)(80.0f + 175.0f * t);

            int anim_w = (int)((float)win->w * scale);
            int anim_h = (int)((float)win->h * scale);
            int anim_x = win->x + (win->w - anim_w) / 2;
            int anim_y = win->y + (win->h - anim_h) / 2;
            int anim_title_h = (anim_h < GUI_TITLEBAR_HEIGHT) ? anim_h : GUI_TITLEBAR_HEIGHT;

            draw_window_shadow(anim_x, anim_y, anim_w, anim_h);
            fill_rounded_rect_alpha(anim_x, anim_y, anim_w, anim_h, 14,
                                     graphics_rgb(36, 38, 46), alpha);
            fill_rounded_rect_alpha(anim_x, anim_y, anim_w, anim_title_h, 14,
                                     graphics_rgb(46, 49, 60), alpha);
            ui_draw_text(win->title, anim_x + 14, anim_y + 6,
                graphics_rgb(235, 235, 240), graphics_rgb(46, 49, 60), 13.0f);

            g_win_extra[win_id].anim_frame++;
            continue;
        }

        draw_window_shadow(win->x, win->y, win->w, win->h);"""
        apply_patch(gui_c, old, new, "draw_windows() acilis animasyonu eklendi")

    print("\nBitti. make ile derle, QEMU'da dene.")
    print("Eger [ATLANDI] satirlari gorduysen o kismi elle uygulaman gerekebilir.")

if __name__ == "__main__":
    main()