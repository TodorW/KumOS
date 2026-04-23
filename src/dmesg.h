#ifndef DMESG_H
#define DMESG_H
#include <stdint.h>
void dmesg_init(void);
void dmesg_log(const char *msg);
void dmesg_printf(const char *fmt, ...);
void dmesg_print_all(void);
int  dmesg_read(char *buf, uint32_t sz);
#endif
