#include "keyboard.h"
#include <stdint.h>

#define KB_DATA_PORT   0x60
#define KB_STATUS_PORT 0x64
#define KB_BUF_SIZE    256

static char kb_buf[KB_BUF_SIZE];
static int  kb_read  = 0;
static int  kb_write = 0;
static int  shift_held = 0;
static int  caps_lock  = 0;

static inline uint8_t inb(uint16_t port) {
    uint8_t val;
    __asm__ volatile ("inb %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

/* US QWERTY scancode set 1 */
static const char sc_normal[] = {
    0,  27, '1','2','3','4','5','6','7','8','9','0','-','=','\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
    0,  'a','s','d','f','g','h','j','k','l',';','\'','`',
    0, '\\','z','x','c','v','b','n','m',',','.','/', 0,
    '*', 0, ' '
};

static const char sc_shift[] = {
    0,  27, '!','@','#','$','%','^','&','*','(',')','_','+','\b',
    '\t','Q','W','E','R','T','Y','U','I','O','P','{','}','\n',
    0,  'A','S','D','F','G','H','J','K','L',':','"','~',
    0,  '|','Z','X','C','V','B','N','M','<','>','?', 0,
    '*', 0, ' '
};

static void kb_push(char c) {
    int next = (kb_write + 1) % KB_BUF_SIZE;
    if (next != kb_read) {
        kb_buf[kb_write] = c;
        kb_write = next;
    }
}

/* Poll keyboard and decode scancode */
static void kb_poll(void) {
    if (!(inb(KB_STATUS_PORT) & 0x01)) return;  /* no data */
    uint8_t sc = inb(KB_DATA_PORT);

    /* Key release */
    if (sc & 0x80) {
        uint8_t released = sc & 0x7F;
        if (released == 0x2A || released == 0x36) shift_held = 0;
        return;
    }

    /* Shift keys */
    if (sc == 0x2A || sc == 0x36) { shift_held = 1; return; }
    /* Caps Lock toggle */
    if (sc == 0x3A) { caps_lock ^= 1; return; }

    if (sc >= sizeof(sc_normal)) return;

    char c;
    if (shift_held)
        c = sc_shift[sc];
    else {
        c = sc_normal[sc];
        /* Apply caps lock to letters only */
        if (caps_lock && c >= 'a' && c <= 'z') c -= 32;
    }
    if (c) kb_push(c);
}

void keyboard_init(void) {
    kb_read = kb_write = 0;
    shift_held = caps_lock = 0;
}

int keyboard_haschar(void) {
    kb_poll();
    return kb_read != kb_write;
}

char keyboard_getchar(void) {
    while (!keyboard_haschar()) {}
    char c = kb_buf[kb_read];
    kb_read = (kb_read + 1) % KB_BUF_SIZE;
    return c;
}
