#include "kernel.h"

size_t strlen(const char* str) {
    size_t len = 0;
    while (str[len])
        len++;
    return len;
}

int strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

int strncmp(const char* s1, const char* s2, size_t n) {
    while (n && *s1 && (*s1 == *s2)) {
        s1++;
        s2++;
        n--;
    }
    if (n == 0)
        return 0;
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

char* strcpy(char* dest, const char* src) {
    char* ret = dest;
    while ((*dest++ = *src++));
    return ret;
}

char* strncpy(char* dest, const char* src, size_t n) {
    char* ret = dest;
    while (n && (*dest++ = *src++))
        n--;
    while (n--)
        *dest++ = '\0';
    return ret;
}

void* memset(void* dest, int val, size_t len) {
    unsigned char* ptr = dest;
    while (len-- > 0)
        *ptr++ = val;
    return dest;
}

void* memcpy(void* dest, const void* src, size_t len) {
    char* d = dest;
    const char* s = src;
    while (len--)
        *d++ = *s++;
    return dest;
}

void reverse(char* str, int length) {
    int start = 0;
    int end = length - 1;
    while (start < end) {
        char temp = str[start];
        str[start] = str[end];
        str[end] = temp;
        start++;
        end--;
    }
}

void itoa(int value, char* str, int base) {
    int i = 0;
    int is_negative = 0;
    
    if (value == 0) {
        str[i++] = '0';
        str[i] = '\0';
        return;
    }
    
    if (value < 0 && base == 10) {
        is_negative = 1;
        value = -value;
    }
    
    while (value != 0) {
        int rem = value % base;
        str[i++] = (rem > 9) ? (rem - 10) + 'a' : rem + '0';
        value = value / base;
    }
    
    if (is_negative)
        str[i++] = '-';
    
    str[i] = '\0';
    reverse(str, i);
}
