#ifndef KSTRING_H
#define KSTRING_H

#include <stddef.h>
#include <stdint.h>

size_t kstrlen(const char *s);
int    kstrcmp(const char *a, const char *b);
int    kstrncmp(const char *a, const char *b, size_t n);
char  *kstrcpy(char *dst, const char *src);
char  *kstrncpy(char *dst, const char *src, size_t n);
char  *kstrcat(char *dst, const char *src);
void  *kmemset(void *s, int c, size_t n);
void  *kmemcpy(void *dst, const void *src, size_t n);
int    kstartswith(const char *s, const char *prefix);
void   kitoa(uint32_t val, char *buf, int base);

#endif