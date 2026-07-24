#ifndef KERNEL_KEYBOARD_H
#define KERNEL_KEYBOARD_H

#include <stdbool.h>

char keyboard_read_char(void);
bool keyboard_try_read_char(char *out);

#endif
