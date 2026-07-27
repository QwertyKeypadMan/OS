#include "kernel/gui.h"
#include "kernel/graphics.h"

static void kaya_designer_ui_draw(struct gui_window *win, int cx, int cy, int cw, int ch)
{
    (void)win;
    (void)cw;
    (void)ch;
    graphics_fill_rect(cx, cy, cw, ch, graphics_rgb(26, 27, 33));
    graphics_draw_rounded_rect(cx + 16, cy + 32, 112, 32, 6, graphics_rgb(63, 140, 242));
    ui_draw_text("Button", cx + 26, cy + 41, graphics_rgb(245, 245, 248), graphics_rgb(63, 140, 242), 14.0f);
    graphics_draw_rounded_rect(cx + 16, cy + 80, 112, 32, 6, graphics_rgb(63, 140, 242));
    ui_draw_text("Button", cx + 26, cy + 89, graphics_rgb(245, 245, 248), graphics_rgb(63, 140, 242), 14.0f);
    graphics_draw_rounded_rect(cx + 16, cy + 128, 112, 32, 6, graphics_rgb(63, 140, 242));
    ui_draw_text("Button", cx + 26, cy + 137, graphics_rgb(245, 245, 248), graphics_rgb(63, 140, 242), 14.0f);
    graphics_fill_rect(cx + 160, cy + 32, 528, 416, graphics_rgb(24, 27, 34));
    graphics_draw_rect(cx + 160, cy + 32, 528, 416, graphics_rgb(70, 78, 96));
    ui_draw_text("ListView", cx + 168, cy + 40, graphics_rgb(190, 200, 215), graphics_rgb(24, 27, 34), 13.0f);
}

void kaya_designer_ui_open(void)
{
    opa_window_create("Window", 720, 480, kaya_designer_ui_draw);
}
