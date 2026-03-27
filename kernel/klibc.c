#include "klibc.h"
#include "vga.h"
#include <stdarg.h>
#include <stdint.h>

size_t kstrlen(const char *s) {
    size_t n = 0;
    while (s[n]) n++;
    return n;
}

int kstrcmp(const char *a, const char *b) {
    while (*a && (*a == *b)) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}

int kstrncmp(const char *a, const char *b, size_t n) {
    while (n-- && *a && (*a == *b)) { a++; b++; }
    if (!n) return 0;
    return (unsigned char)*a - (unsigned char)*b;
}

char *kstrcpy(char *dst, const char *src) {
    char *d = dst;
    while ((*d++ = *src++));
    return dst;
}

char *kstrncpy(char *dst, const char *src, size_t n) {
    size_t i;
    for (i = 0; i < n && src[i]; i++) dst[i] = src[i];
    for (; i < n; i++) dst[i] = 0;
    return dst;
}

char *kstrcat(char *dst, const char *src) {
    char *d = dst + kstrlen(dst);
    while ((*d++ = *src++));
    return dst;
}

char *kstrchr(const char *s, int c) {
    while (*s) { if (*s == c) return (char*)s; s++; }
    return (c == 0) ? (char*)s : 0;
}

char *kstrrchr(const char *s, int c) {
    const char *last = 0;
    while (*s) { if (*s == c) last = s; s++; }
    return (char*)last;
}

void *kmemset(void *dst, int c, size_t n) {
    uint8_t *d = dst;
    while (n--) *d++ = (uint8_t)c;
    return dst;
}

void *kmemcpy(void *dst, const void *src, size_t n) {
    uint8_t *d = dst;
    const uint8_t *s = src;
    while (n--) *d++ = *s++;
    return dst;
}

int kmemcmp(const void *a, const void *b, size_t n) {
    const uint8_t *x = a, *y = b;
    while (n--) { if (*x != *y) return *x - *y; x++; y++; }
    return 0;
}

void kitoa(int val, char *buf, int base) {
    if (val < 0 && base == 10) {
        *buf++ = '-';
        val = -val;
    }
    kutoa((unsigned)val, buf, base);
}

void kutoa(unsigned val, char *buf, int base) {
    const char *digits = "0123456789ABCDEF";
    char tmp[32];
    int i = 0;
    if (val == 0) { buf[0] = '0'; buf[1] = 0; return; }
    while (val) { tmp[i++] = digits[val % base]; val /= base; }
    int j = 0;
    while (i--) buf[j++] = tmp[i];
    buf[j] = 0;
}

int katoi(const char *s) {
    int neg = 0, result = 0;
    if (*s == '-') { neg = 1; s++; }
    while (*s >= '0' && *s <= '9') result = result * 10 + (*s++ - '0');
    return neg ? -result : result;
}

int kvsprintf(char *buf, const char *fmt, va_list args) {
    char *out = buf;
    for (; *fmt; fmt++) {
        if (*fmt != '%') { *out++ = *fmt; continue; }
        fmt++;
        char tmp[64];
        switch (*fmt) {
        case 'd': kitoa(va_arg(args, int), tmp, 10);    goto copy;
        case 'u': kutoa(va_arg(args, unsigned), tmp, 10); goto copy;
        case 'x': kutoa(va_arg(args, unsigned), tmp, 16); goto copy;
        case 'p': {
            *out++ = '0'; *out++ = 'x';
            kutoa((unsigned)(uintptr_t)va_arg(args, void*), tmp, 16);
            goto copy;
        }
        case 's': {
            const char *s = va_arg(args, const char*);
            if (!s) s = "(null)";
            while (*s) *out++ = *s++;
            continue;
        }
        case 'c': *out++ = (char)va_arg(args, int); continue;
        case '%': *out++ = '%'; continue;
        default:  *out++ = '%'; *out++ = *fmt; continue;
        }
    copy:
        for (char *t = tmp; *t; t++) *out++ = *t;
    }
    *out = 0;
    return (int)(out - buf);
}

int ksprintf(char *buf, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int n = kvsprintf(buf, fmt, args);
    va_end(args);
    return n;
}

void kprintf(const char *fmt, ...) {
    char buf[512];
    va_list args;
    va_start(args, fmt);
    kvsprintf(buf, fmt, args);
    va_end(args);
    vga_puts(buf);
}
