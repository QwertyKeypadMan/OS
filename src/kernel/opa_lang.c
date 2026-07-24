#include "kernel/gui.h"
#include "kernel/graphics.h"
#include "kernel/opa.h"
#include "kernel/ramfs.h"
#include "kernel/kstring.h"
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

/* gui_window_t bilinmiyorsa struct forward declaration */
struct gui_window;

/* Harici fonksiyon bildirimleri (gui.c içinde tanımlı) */
extern void fill_circle(int cx, int cy, int r, uint32_t color);
extern void ui_draw_text(const char *text, int x, int y, uint32_t foreground, uint32_t background, float font_size);

/* =========================================================================
 * GUI.C NATIVE WIDGET ENTEGRASYONU
 * ========================================================================= */
typedef enum {
    GUI_WIDGET_BUTTON,
    GUI_WIDGET_TEXTBOX
} gui_widget_type_t;

typedef struct gui_widget {
    gui_widget_type_t type;
    int x, y, w, h;             /* Pencere içi göreceli (relative) koordinatlar */
    char label[64];             /* Button metni veya Textbox placeholder */
    char text[128];             /* Textbox canlı içeriği */
    bool is_hovered;            /* Fare üstünde mi? */
    bool is_pressed;            /* Basıldı mı? */
    bool is_focused;            /* Odakta mı? */
    void (*on_click)(void);     /* Callback */
} gui_widget_t;

/* gui.c içindeki yerel bileşen çizim fonksiyonları */
extern void gui_draw_button(gui_widget_t *btn, int win_x, int win_y);
extern void gui_draw_textbox(gui_widget_t *tb, int win_x, int win_y);

/* =========================================================================
 * GÖRSEL DİL KOMUT SETİ (OPA GUI SCRIPT BYTECODE)
 * ========================================================================= */
typedef enum {
    CMD_CREATE_WINDOW,
    CMD_DRAW_TEXT,
    CMD_DRAW_RECT,     
    CMD_DRAW_CIRCLE,   
    CMD_WIDGET_BUTTON,   // Dinamik Native Buton
    CMD_WIDGET_TEXTBOX,  // Dinamik Native Textbox
    CMD_NOTIFY,
    CMD_END
} opa_cmd_type_t;

typedef struct {
    opa_cmd_type_t type;
    int x, y, w, h;    
    char text[64];
    uint32_t color;
    
    /* Native Widget durum nesnesi */
    gui_widget_t widget;
} opa_instruction_t;

#define MAX_INSTRUCTIONS 64

typedef struct {
    opa_instruction_t code[MAX_INSTRUCTIONS];
    int count;
    int active_win_id;
} opa_script_program_t;

/* Global betik durumu */
static opa_script_program_t g_current_prog;

/* =========================================================================
 * PENCERE İÇİ ÇİZİM CALLBACK ENTEGRASYONU
 * ========================================================================= */
static void opa_lang_draw_callback(struct gui_window *win, int cx, int cy, int cw, int ch) {
    (void)win; (void)cw; (void)ch;

    for (int i = 0; i < g_current_prog.count; i++) {
        opa_instruction_t *inst = &g_current_prog.code[i];

        if (inst->type == CMD_DRAW_TEXT) {
            ui_draw_text(inst->text, cx + inst->x, cy + inst->y, inst->color, graphics_rgb(26, 27, 33), 14.0f);
        }
        else if (inst->type == CMD_DRAW_RECT) {
            graphics_fill_rect(cx + inst->x, cy + inst->y, inst->w, inst->h, inst->color);
        }
        else if (inst->type == CMD_DRAW_CIRCLE) {
            fill_circle(cx + inst->x, cy + inst->y, inst->w, inst->color);
        }
        else if (inst->type == CMD_WIDGET_BUTTON) {
            /* gui.c içerisindeki canlı, hover/active efektli buton render motorunu çağırıyoruz */
            gui_draw_button(&inst->widget, cx, cy);
        }
        else if (inst->type == CMD_WIDGET_TEXTBOX) {
            /* gui.c içerisindeki odaklanan ve imleci yanıp sönen kutu motorunu çağırıyoruz */
            gui_draw_textbox(&inst->widget, cx, cy);
        }
    }
}

/* =========================================================================
 * PARSER & SCRIPT RUNNER
 * ========================================================================= */
void opa_window_on_closed(int win_id) {
    if (g_current_prog.active_win_id == win_id) {
        g_current_prog.active_win_id = -1;
        g_current_prog.count = 0;
    }
}

void fill_circle(int cx, int cy, int r, uint32_t color) {
    for (int dy = -r; dy <= r; dy++) {
        int remaining = r * r - dy * dy;
        if (remaining < 0) continue;
        int dx = 0;
        while ((dx + 1) * (dx + 1) <= remaining) dx++;
        graphics_fill_rect(cx - dx, cy + dy, dx * 2 + 1, 1, color);
    }
}

bool opa_run_gui_script(const char *script_data) {
    g_current_prog.count = 0;
    g_current_prog.active_win_id = -1;

    char win_title[32] = "OPA Visual App";
    int win_w = 400, win_h = 300;

    const char *line = script_data;
    while (*line != '\0' && g_current_prog.count < MAX_INSTRUCTIONS) {
        if (k_strncmp(line, "WINDOW ", 7) == 0) {
            g_current_prog.code[g_current_prog.count].type = CMD_CREATE_WINDOW;
            g_current_prog.count++;
        } 
        else if (k_strncmp(line, "TEXT ", 5) == 0) {
            opa_instruction_t *inst = &g_current_prog.code[g_current_prog.count];
            inst->type = CMD_DRAW_TEXT;
            
            inst->x = 10; 
            inst->y = 30;
            
            const char *p = line + 5;
            int idx = 0;
            while (*p != '\n' && *p != '\r' && *p != '\0' && idx < (int)sizeof(inst->text) - 1) {
                inst->text[idx++] = *p++;
            }
            inst->text[idx] = '\0';
            
            if (idx == 0) {
                k_strlcpy(inst->text, "KayaOS GUI Script", sizeof(inst->text));
            }

            inst->color = graphics_rgb(220, 220, 225);
            g_current_prog.count++;
        }
        else if (k_strncmp(line, "BUTTON ", 7) == 0) {
            opa_instruction_t *inst = &g_current_prog.code[g_current_prog.count];
            inst->type = CMD_WIDGET_BUTTON;
            
            /* Native Widget yapılandırması */
            inst->widget.type = GUI_WIDGET_BUTTON;
            inst->widget.x = 20; 
            inst->widget.y = 60; 
            inst->widget.w = 140; 
            inst->widget.h = 32;
            inst->widget.is_hovered = false;
            inst->widget.is_pressed = false;
            inst->widget.is_focused = false;
            inst->widget.on_click = NULL;

            const char *p = line + 7;
            int idx = 0;
            while (*p != '\n' && *p != '\r' && *p != '\0' && idx < (int)sizeof(inst->widget.label) - 1) {
                inst->widget.label[idx++] = *p++;
            }
            inst->widget.label[idx] = '\0';

            g_current_prog.count++;
        }
        else if (k_strncmp(line, "TEXTBOX ", 8) == 0) {
            opa_instruction_t *inst = &g_current_prog.code[g_current_prog.count];
            inst->type = CMD_WIDGET_TEXTBOX;
            
            /* Native Widget yapılandırması */
            inst->widget.type = GUI_WIDGET_TEXTBOX;
            inst->widget.x = 20; 
            inst->widget.y = 110; 
            inst->widget.w = 220; 
            inst->widget.h = 30;
            inst->widget.is_hovered = false;
            inst->widget.is_pressed = false;
            inst->widget.is_focused = false;
            inst->widget.text[0] = '\0';

            const char *p = line + 8;
            int idx = 0;
            while (*p != '\n' && *p != '\r' && *p != '\0' && idx < (int)sizeof(inst->widget.label) - 1) {
                inst->widget.label[idx++] = *p++;
            }
            inst->widget.label[idx] = '\0';

            g_current_prog.count++;
        }

        while (*line != '\n' && *line != '\0') line++;
        if (*line == '\n') line++;
    }

    int win_id = opa_window_create(win_title, win_w, win_h, (void*)opa_lang_draw_callback);
    if (win_id < 0) {
        gui_notify("opa: Pencere olusturulamadi!");
        return false;
    }

    g_current_prog.active_win_id = win_id;
    return true;
}