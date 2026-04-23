#ifndef USERS_H
#define USERS_H

#include <stdint.h>

#define USER_MAX       8
#define USER_NAME_LEN 16
#define USER_PASS_LEN 32
#define USER_HOME_LEN 32

typedef struct {
    char     name[USER_NAME_LEN];
    char     pass_hash[USER_PASS_LEN];
    uint32_t uid;
    char     home[USER_HOME_LEN];
    int      is_root;
    int      active;
} user_t;

void users_init(void);
int  users_login(const char *name, const char *pass);
int  users_add(const char *name, const char *pass, int is_root);
user_t *users_get_current(void);
void users_set_current(int uid);
void users_list(void);
int  users_check_pass(const char *name, const char *pass);

#endif
