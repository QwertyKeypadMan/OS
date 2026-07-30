#ifndef SETTINGS_H
#define SETTINGS_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    bool active;
    int  win_id;
    int  current_tab;        /* 0: Gorunum (Ileride 1: Sistem, 2: Hakkindah eklenebilir) */
    
    /* Gorunum Secimleri */
    int  selected_cursor;    /* 0: cursor.bmp, 1: cursorwhite.bmp */
    int  selected_wallpaper; /* 0: wallpaper.bmp, 1: wl2.bmp, 2: wl3.bmp */
} settings_state_t;

void settings_open(void);
void settings_close(void);
void settings_handle_mouse(int win_id, int mouse_x, int mouse_y, uint8_t buttons);

#endif /* SETTINGS_H */