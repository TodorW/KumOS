#ifndef DHCP_H
#define DHCP_H
#include <stdint.h>
int dhcp_request(void);
uint32_t dhcp_get_ip(void);
uint32_t dhcp_get_gateway(void);
uint32_t dhcp_get_dns(void);
#endif
