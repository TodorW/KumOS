#pragma once
#include <stdint.h>

void keyboard_init(void);
char keyboard_getchar(void);  /* blocking read */
int  keyboard_haschar(void);  /* non-blocking check */
