#ifndef PKG_H
#define PKG_H

int pkg_count(void);
const char *pkg_name_at(int i);
const char *pkg_fname_at(int i);
const char *pkg_desc_at(int i);
int pkg_is_installed(int i);

int pkg_install(const char *name);
int pkg_remove(const char *name);

#endif
