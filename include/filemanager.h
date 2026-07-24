#ifndef FILEMANAGER_H
#define FILEMANAGER_H

#include "kernel/gui.h"
#include "kernel/graphics.h"
#include "kernel/ramfs.h"
#include <stdint.h>
#include <stdbool.h>

/* Modül Başlatma ve Kapatma API */
void fm_open(void);
void fm_close(void);

/* Modül Fonksiyonları */
void fm_draw(gui_window_t *win, int cx, int cy, int cw, int ch);
void fm_update(void);
void fm_draw_toolbar(int cx, int cy, int cw);
void fm_draw_sidebar(int cx, int cy, int ch);
void fm_draw_file_list(int cx, int cy, int cw, int ch);
void fm_draw_statusbar(int cx, int cy, int cw, int ch);
void fm_handle_mouse(int win_id, int mouse_x, int mouse_y, uint8_t buttons);
void fm_change_directory(int target_node, const char *path);
void fm_select_item(int index);
void fm_open_selected(void);

#endif /* FILEMANAGER_H */