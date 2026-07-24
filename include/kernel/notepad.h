#ifndef NOTEPAD_H
#define NOTEPAD_H

#include <stdint.h>
#include <stdbool.h>
#include "kernel/gui.h"

#define NOTEPAD_MAX_BUFFER 65536  /* 64 KB Maksimum Metin Boyutu */
#define NOTEPAD_LINE_HEIGHT 18
#define NOTEPAD_CHAR_WIDTH  8

#define KEY_BACKSPACE 0x0E
#define KEY_ENTER     0x1C
#define KEY_DELETE    0x53
#define KEY_HOME      0x47
#define KEY_END       0x4F
#define KEY_UP        0x48
#define KEY_DOWN      0x50
#define KEY_LEFT      0x4B
#define KEY_RIGHT     0x4D

/* Notepad Uygulama Durum Yapısı */
typedef struct {
    int win_id;
    bool active;
    bool modified;

    char filepath[256];
    char filename[64];

    char buffer[NOTEPAD_MAX_BUFFER];
    int text_len;

    int cursor_pos;     /* Metin içindeki indis (0 ... text_len) */
    int scroll_y;       /* Satır kaydırma offseti */
    int scroll_x;       /* Sütun kaydırma offseti */

    int current_line;
    int current_col;
} notepad_state_t;

/* Ana Fonksiyonlar */
void notepad_open(const char *path);
void notepad_close(void);
void notepad_new(void);

/* Dosya I/O */
bool notepad_load_file(const char *path);
bool notepad_save_file(const char *path);

/* Arayüz ve Etkileşim */
void notepad_draw(gui_window_t *win, int cx, int cy, int cw, int ch);
void notepad_handle_mouse(int win_id, int mouse_x, int mouse_y, uint8_t buttons);
void notepad_handle_key(uint8_t scancode, char ascii);

/* Metin Düzenleme Operasyonları */
void notepad_insert_char(char c);
void notepad_delete_backspace(void);
void notepad_delete_suppr(void);
void notepad_move_cursor(int delta);
void notepad_move_cursor_line(int line_delta);
void notepad_on_key(int win_id, char ascii, uint8_t scancode);

#endif /* NOTEPAD_H */