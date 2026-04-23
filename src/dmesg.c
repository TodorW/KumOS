#include "dmesg.h"
#include "vga.h"
#include "timer.h"
#include "kstring.h"
#include <stdint.h>

#define DMESG_BUF  16384
#define DMESG_LINES  256

static char     dmesg_buf[DMESG_BUF];
static uint32_t dmesg_head = 0;
static uint32_t dmesg_count = 0;

void dmesg_init(void) {
    kmemset(dmesg_buf, 0, DMESG_BUF);
    dmesg_head = 0;
    dmesg_count = 0;
}

static void buf_append(const char *s) {
    while (*s) {
        dmesg_buf[dmesg_head % DMESG_BUF] = *s++;
        dmesg_head++;
        dmesg_count++;
    }
}

void dmesg_log(const char *msg) {
    char ts[16];
    uint32_t sec = timer_seconds();
    uint32_t ms  = (timer_ticks() % 100) * 10;
    char *p = ts + 15; *p-- = 0;
    char tmp[8]; int i = 6; tmp[7]=0;
    uint32_t v = sec;
    if (!v) tmp[i--]='0'; else while(v){tmp[i--]='0'+v%10;v/=10;}
    buf_append("[");
    buf_append(tmp+i+1);
    buf_append(".");
    if (ms < 100) buf_append("0");
    if (ms < 10)  buf_append("0");
    v=ms; i=6; if(!v)tmp[i--]='0'; else while(v){tmp[i--]='0'+v%10;v/=10;}
    buf_append(tmp+i+1);
    buf_append("] ");
    buf_append(msg);
    if (msg[kstrlen(msg)-1] != '\n') buf_append("\n");
    (void)p;
    (void)ts;
}

void dmesg_printf(const char *fmt, ...) {
    char buf[256];
    int pos = 0;
    __builtin_va_list ap; __builtin_va_start(ap, fmt);
    while (*fmt && pos < 254) {
        if (*fmt != '%') { buf[pos++] = *fmt++; continue; }
        fmt++;
        if (*fmt=='s') { char *s=__builtin_va_arg(ap,char*); if(!s)s=""; while(*s&&pos<254)buf[pos++]=*s++; }
        else if (*fmt=='d') { int d=__builtin_va_arg(ap,int); char t[12];int i=10;t[11]=0;if(d<0){buf[pos++]='-';d=-d;}if(!d){buf[pos++]='0';}else{unsigned u=(unsigned)d;while(u){t[i--]='0'+u%10;u/=10;}while(++i<=10)buf[pos++]=t[i];} }
        else if (*fmt=='u') { unsigned u=__builtin_va_arg(ap,unsigned); char t[12];int i=10;t[11]=0;if(!u){buf[pos++]='0';}else{while(u){t[i--]='0'+u%10;u/=10;}while(++i<=10)buf[pos++]=t[i];} }
        else if (*fmt=='x') { unsigned u=__builtin_va_arg(ap,unsigned); const char*h="0123456789abcdef"; char t[12];int i=10;t[11]=0;if(!u){buf[pos++]='0';}else{while(u){t[i--]=h[u&0xF];u>>=4;}while(++i<=10)buf[pos++]=t[i];} }
        else if (*fmt=='c') { buf[pos++]=(char)__builtin_va_arg(ap,int); }
        fmt++;
    }
    buf[pos]=0;
    __builtin_va_end(ap);
    dmesg_log(buf);
}

void dmesg_print_all(void) {
    uint32_t start = (dmesg_count > DMESG_BUF) ? (dmesg_head % DMESG_BUF) : 0;
    uint32_t total = (dmesg_count > DMESG_BUF) ? DMESG_BUF : dmesg_head;
    for (uint32_t i = 0; i < total; i++) {
        char c = dmesg_buf[(start + i) % DMESG_BUF];
        if (c) vga_putchar(c);
    }
}

int dmesg_read(char *buf, uint32_t sz) {
    uint32_t start = (dmesg_count > DMESG_BUF) ? (dmesg_head % DMESG_BUF) : 0;
    uint32_t total = (dmesg_count > DMESG_BUF) ? DMESG_BUF : dmesg_head;
    if (total >= sz) total = sz - 1;
    for (uint32_t i = 0; i < total; i++)
        buf[i] = dmesg_buf[(start + i) % DMESG_BUF];
    buf[total] = 0;
    return (int)total;
}
