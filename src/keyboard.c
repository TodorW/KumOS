
#include "keyboard.h"
#include "idt.h"
#include "vga.h"
#include <stdint.h>

#define KB_DATA     0x60
#define KB_STATUS   0x64
#define KB_CMD      0x64
#define PIC1_CMD    0x20
#define PIC1_DATA   0x21
#define PIC_EOI     0x20

#define KB_OBF      0x01
#define KB_IBF      0x02

#define KB_BUF  256
static volatile char buf[KB_BUF];
static volatile int  bhead = 0;
static volatile int  btail = 0;

static inline void outb(uint16_t p, uint8_t v) {
    __asm__ volatile("outb %0,%1"::"a"(v),"Nd"(p));
}
static inline uint8_t inb(uint16_t p) {
    uint8_t v; __asm__ volatile("inb %1,%0":"=a"(v):"Nd"(p)); return v;
}
static inline void io_wait(void) { outb(0x80,0); outb(0x80,0); }

static int wait_obf(void) {
    for (int i = 0; i < 100000; i++) {
        if (inb(KB_STATUS) & KB_OBF) return 1;
        io_wait();
    }
    return 0;
}

static int wait_ibf_clear(void) {
    for (int i = 0; i < 100000; i++) {
        if (!(inb(KB_STATUS) & KB_IBF)) return 1;
        io_wait();
    }
    return 0;
}
static void flush(void) {
    for (int i = 0; i < 16 && (inb(KB_STATUS) & KB_OBF); i++)
        inb(KB_DATA);
}
static void cmd(uint8_t c) { wait_ibf_clear(); outb(KB_CMD, c); io_wait(); }
static void dat(uint8_t d) { wait_ibf_clear(); outb(KB_DATA, d); io_wait(); }
static uint8_t resp(void) { return wait_obf() ? inb(KB_DATA) : 0xFF; }

static const char sc_lo[128] = {
    0, 27,'1','2','3','4','5','6','7','8','9','0','-','=','\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
    0,'a','s','d','f','g','h','j','k','l',';','\'','`',
    0,'\\','z','x','c','v','b','n','m',',','.','/',0,
    '*',0,' ',0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};
static const char sc_hi[128] = {
    0, 27,'!','@','#','$','%','^','&','*','(',')','_','+','\b',
    '\t','Q','W','E','R','T','Y','U','I','O','P','{','}','\n',
    0,'A','S','D','F','G','H','J','K','L',':','"','~',
    0,'|','Z','X','C','V','B','N','M','<','>','?',0,
    '*',0,' ',
};

static volatile int shift=0, caps=0, ctrl=0, ext=0;

static void process_scancode(uint8_t sc) {
    if (sc == 0xE0 || sc == 0xE1) { ext = 1; return; }

    if (sc & 0x80) {
        sc &= 0x7F;
        if (!ext) {
            if (sc==0x2A||sc==0x36) shift=0;
            if (sc==0x1D) ctrl=0;
        }
        ext = 0; return;
    }

    if (ext) {
        ext = 0;
        if (sc==0x1D) { ctrl=1; return; }
        return;
    }

    if (sc==0x2A||sc==0x36) { shift=1; return; }
    if (sc==0x3A)            { caps=!caps; return; }
    if (sc==0x1D)            { ctrl=1; return; }
    if (sc==0x38)            return;
    if (sc>=128)             return;

    char c = (shift && sc < (int)sizeof(sc_hi)) ? sc_hi[sc] : sc_lo[sc];
    if (!c) return;

    if (caps) {
        if (c>='a'&&c<='z') c-=32;
        else if (c>='A'&&c<='Z') c+=32;
    }
    if (ctrl&&c>='a'&&c<='z') c-=96;
    if (ctrl&&c>='A'&&c<='Z') c-=64;

    int next = (bhead+1)%KB_BUF;
    if (next != btail) { buf[bhead]=c; bhead=next; }
}

static void kb_irq(registers_t *r) {
    (void)r;
    if (!(inb(KB_STATUS) & KB_OBF)) return;
    process_scancode(inb(KB_DATA));
}

static char poll_once(void) {
    if (!(inb(KB_STATUS) & KB_OBF)) return 0;
    uint8_t sc = inb(KB_DATA);
    if (sc & 0x80) {
        sc &= 0x7F;
        if (sc==0x2A||sc==0x36) shift=0;
        if (sc==0x1D) ctrl=0;
        return 0;
    }

    process_scancode(sc);

    if (bhead != btail) {
        char c = buf[btail]; btail=(btail+1)%KB_BUF; return c;
    }
    return 0;
}

void keyboard_init(void) {
    bhead = btail = 0;
    shift = caps = ctrl = ext = 0;

    cmd(0xAD); cmd(0xA7);
    flush();

    cmd(0x20);
    uint8_t cfg = resp();
    cfg |=  0x01;
    cfg &= ~0x40;
    cmd(0x60); dat(cfg);

    cmd(0xAE); io_wait();

    flush();
    dat(0xFF);
    uint8_t ack = resp();
    if (ack == 0xFA || ack == 0xAA) {
        uint8_t r2 = resp();
        (void)r2;
    }
    flush();

    dat(0xF0); resp();
    dat(0x01); resp();

    dat(0xF4); resp();
    flush();

    uint8_t mask = inb(PIC1_DATA);
    mask &= ~0x03;
    outb(PIC1_DATA, mask);
    io_wait();

    irq_register(1, kb_irq);
}

char keyboard_getchar(void) {

    if (bhead != btail) {
        char c = buf[btail]; btail=(btail+1)%KB_BUF; return c;
    }

    return poll_once();
}

char keyboard_getchar_blocking(void) {
    for (;;) {

        if (bhead != btail) {
            char c = buf[btail]; btail=(btail+1)%KB_BUF; return c;
        }

        char c = poll_once();
        if (c) return c;

        __asm__ volatile("sti; hlt; cli");

    }
}

int keyboard_getline(char *out, int maxlen) {
    int i = 0;
    for (;;) {
        char c = keyboard_getchar_blocking();
        if (!c) continue;
        if (c == '\n') { out[i]=0; vga_putchar('\n'); return i; }
        if (c == '\b') { if (i>0) { i--; vga_putchar('\b'); } }
        else if (i < maxlen-1) { out[i++]=c; vga_putchar(c); }
    }
}

int keyboard_ctrl_held(void) { return ctrl; }
int keyboard_alt_held(void)  { return 0;   }
int keyboard_has_input(void) { return bhead != btail; }
