#include "kernel/keyboard.h"

#include <stdbool.h>
#include <stdint.h>

#include "kernel/io.h"
#include "kernel/kstring.h"
#include "kernel/gui.h"

#define KEYBOARD_DATA_PORT 0x60
#define KEYBOARD_STATUS_PORT 0x64
#define KEYBOARD_STATUS_OUTPUT_FULL 0x01
#define KEYBOARD_STATUS_AUX_DATA 0x20

static bool shift_pressed = false;
static bool caps_lock = false;

static const char keymap[128] = {
    [0x01] = 27,   /* ESC */
    [0x02] = '1',
    [0x03] = '2',
    [0x04] = '3',
    [0x05] = '4',
    [0x06] = '5',
    [0x07] = '6',
    [0x08] = '7',
    [0x09] = '8',
    [0x0A] = '9',
    [0x0B] = '0',
    [0x0C] = '-',
    [0x0D] = '=',
    [0x0E] = '\b',
    [0x0F] = '\t',
    [0x10] = 'q',
    [0x11] = 'w',
    [0x12] = 'e',
    [0x13] = 'r',
    [0x14] = 't',
    [0x15] = 'y',
    [0x16] = 'u',
    [0x17] = 'i',
    [0x18] = 'o',
    [0x19] = 'p',
    [0x1A] = '[',
    [0x1B] = ']',
    [0x1C] = '\n',
    [0x1E] = 'a',
    [0x1F] = 's',
    [0x20] = 'd',
    [0x21] = 'f',
    [0x22] = 'g',
    [0x23] = 'h',
    [0x24] = 'j',
    [0x25] = 'k',
    [0x26] = 'l',
    [0x27] = ';',
    [0x28] = '\'',
    [0x29] = '`',
    [0x2B] = '\\',
    [0x2C] = 'z',
    [0x2D] = 'x',
    [0x2E] = 'c',
    [0x2F] = 'v',
    [0x30] = 'b',
    [0x31] = 'n',
    [0x32] = 'm',
    [0x33] = ',',
    [0x34] = '.',
    [0x35] = '/',
    [0x39] = ' ',
};

static const char shift_keymap[128] = {
    [0x02] = '!',
    [0x03] = '@',
    [0x04] = '#',
    [0x05] = '$',
    [0x06] = '%',
    [0x07] = '^',
    [0x08] = '&',
    [0x09] = '*',
    [0x0A] = '(',
    [0x0B] = ')',
    [0x0C] = '_',
    [0x0D] = '+',
    [0x10] = 'Q',
    [0x11] = 'W',
    [0x12] = 'E',
    [0x13] = 'R',
    [0x14] = 'T',
    [0x15] = 'Y',
    [0x16] = 'U',
    [0x17] = 'I',
    [0x18] = 'O',
    [0x19] = 'P',
    [0x1A] = '{',
    [0x1B] = '}',
    [0x1E] = 'A',
    [0x1F] = 'S',
    [0x20] = 'D',
    [0x21] = 'F',
    [0x22] = 'G',
    [0x23] = 'H',
    [0x24] = 'J',
    [0x25] = 'K',
    [0x26] = 'L',
    [0x27] = ':',
    [0x28] = '"',
    [0x29] = '~',
    [0x2B] = '|',
    [0x2C] = 'Z',
    [0x2D] = 'X',
    [0x2E] = 'C',
    [0x2F] = 'V',
    [0x30] = 'B',
    [0x31] = 'N',
    [0x32] = 'M',
    [0x33] = '<',
    [0x34] = '>',
    [0x35] = '?',
    [0x39] = ' ',
};

static bool translate_scancode(uint8_t raw_scancode, char *out_char, uint8_t *out_scancode)
{
    bool released = (raw_scancode & 0x80) != 0;
    uint8_t scancode = raw_scancode & 0x7F;

    if (out_scancode) {
        *out_scancode = scancode;
    }

    /* Shift tuşları kontrolü */
    if (scancode == 0x2A || scancode == 0x36) {
        shift_pressed = !released;
        return false;
    }

    /* Tuş bırakıldıysa (Release event) karakter üretme */
    if (released) {
        return false;
    }

    /* Caps Lock kontrolü */
    if (scancode == 0x3A) {
        caps_lock = !caps_lock;
        return false;
    }

    char ch = keymap[scancode];
    if (ch == '\0') {
        return false;
    }

    if (k_isalpha(ch)) {
        if (shift_pressed ^ caps_lock) {
            ch = k_toupper(ch);
        }
    } else if (shift_pressed && shift_keymap[scancode] != '\0') {
        ch = shift_keymap[scancode];
    }

    if (out_char) {
        *out_char = ch;
    }
    return true;
}

/* GUI için hem karakteri hem de scancode'u okuyan güvenli döngüsel okuma */
bool keyboard_try_read_event(char *out_char, uint8_t *out_scancode)
{
    uint8_t status = inb(KEYBOARD_STATUS_PORT);
    if ((status & KEYBOARD_STATUS_OUTPUT_FULL) == 0) {
        return false;
    }

    /* Fare verilerini filtrele (AUX_DATA) */
    if ((status & KEYBOARD_STATUS_AUX_DATA) != 0) {
        return false;
    }

    uint8_t raw_scancode = inb(KEYBOARD_DATA_PORT);
    return translate_scancode(raw_scancode, out_char, out_scancode);
}

/* Eski kütüphane uyumluluğu için */
bool keyboard_try_read_char(char *out)
{
    return keyboard_try_read_event(out, NULL);
}

char keyboard_read_char(void)
{
    for (;;) {
        char ch;
        if (keyboard_try_read_char(&ch)) {
            return ch;
        }
        __asm__ __volatile__("pause");
    }
}

/* Eğer ileride IRQ1 Kesmesi (Interrupt) kullanmak istersen: */
void keyboard_interrupt_handler(void) {
    uint8_t status = inb(KEYBOARD_STATUS_PORT);
    if ((status & KEYBOARD_STATUS_OUTPUT_FULL) != 0 && (status & KEYBOARD_STATUS_AUX_DATA) == 0) {
        uint8_t raw_scancode = inb(KEYBOARD_DATA_PORT);
        char ch;
        uint8_t scancode;
        if (translate_scancode(raw_scancode, &ch, &scancode)) {
            gui_dispatch_key(ch, scancode);
        }
    }
}