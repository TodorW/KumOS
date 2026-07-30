#ifndef PKGNET_H
#define PKGNET_H
#include <stdint.h>

void     pkgnet_set_repo(uint32_t ip, uint16_t port);
uint32_t pkgnet_repo_ip(void);
uint16_t pkgnet_repo_port(void);
int      pkgnet_parse_addr(const char *s, uint32_t *ip, uint16_t *port);

int pkgnet_update(void);

int         pkgnet_count(void);
const char *pkgnet_name_at(int i);
const char *pkgnet_fname_at(int i);
const char *pkgnet_desc_at(int i);

int pkgnet_find(const char *name);
int pkgnet_install(const char *name);

#endif
