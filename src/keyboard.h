#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>

void keyboard_init(void);
char keyboard_getchar(void);
char keyboard_getchar_blocking(void);
int  keyboard_getline(char *buf, int maxlen);
int  keyboard_ctrl_held(void);
int  keyboard_alt_held(void);

int keyboard_has_input(void);

#endif
