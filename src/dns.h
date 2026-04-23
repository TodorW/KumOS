#ifndef DNS_H
#define DNS_H
#include <stdint.h>
uint32_t dns_resolve(const char *hostname);
void     dns_init(uint32_t server_ip);
#endif
