#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>

/* String functions */
size_t   kstrlen(const char *s);
int      kstrcmp(const char *a, const char *b);
int      kstrncmp(const char *a, const char *b, size_t n);
char    *kstrcpy(char *dst, const char *src);
char    *kstrncpy(char *dst, const char *src, size_t n);
char    *kstrcat(char *dst, const char *src);
char    *kstrchr(const char *s, int c);
char    *kstrrchr(const char *s, int c);
void    *kmemset(void *dst, int c, size_t n);
void    *kmemcpy(void *dst, const void *src, size_t n);
int      kmemcmp(const void *a, const void *b, size_t n);

/* Formatted output */
int      kvsprintf(char *buf, const char *fmt, va_list args);
int      ksprintf(char *buf, const char *fmt, ...);
void     kprintf(const char *fmt, ...);

/* Number utilities */
void     kitoa(int val, char *buf, int base);
void     kutoa(unsigned val, char *buf, int base);
int      katoi(const char *s);
