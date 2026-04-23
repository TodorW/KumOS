#include "users.h"
#include "kstring.h"
#include "vga.h"
#include "fat12.h"
#include <stdint.h>

static user_t users[USER_MAX];
static int    current_uid = 0;
static int    num_users   = 0;

static uint32_t simple_hash(const char *s) {
    uint32_t h = 5381;
    while (*s) { h = ((h<<5)+h) ^ (uint32_t)*s++; }
    return h;
}

static void hash_to_str(uint32_t h, char *out) {
    const char *hex = "0123456789abcdef";
    for (int i=7;i>=0;i--) { out[i]=hex[h&0xF]; h>>=4; }
    out[8]=0;
}

void users_init(void) {
    kmemset(users, 0, sizeof(users));
    num_users = 0;
    current_uid = 0;

    users_add("root",  "kumos", 1);
    users_add("user",  "user",  0);
    users_add("guest", "",      0);
}

int users_add(const char *name, const char *pass, int is_root) {
    if (num_users >= USER_MAX) return -1;
    user_t *u = &users[num_users];
    kstrncpy(u->name, name, USER_NAME_LEN-1);
    uint32_t h = simple_hash(pass);
    hash_to_str(h, u->pass_hash);
    u->uid = (uint32_t)num_users;
    u->is_root = is_root;
    u->active = 1;
    kstrcpy(u->home, is_root ? "/disk" : "/disk");
    num_users++;
    return (int)u->uid;
}

int users_check_pass(const char *name, const char *pass) {
    for (int i=0;i<num_users;i++) {
        if (!users[i].active) continue;
        if (kstrcmp(users[i].name, name) == 0) {
            char h[9]; hash_to_str(simple_hash(pass), h);
            if (kstrcmp(h, users[i].pass_hash)==0) return (int)users[i].uid;
            return -1;
        }
    }
    return -1;
}

int users_login(const char *name, const char *pass) {
    int uid = users_check_pass(name, pass);
    if (uid >= 0) current_uid = uid;
    return uid;
}

user_t *users_get_current(void) {
    for (int i=0;i<num_users;i++)
        if (users[i].active && (int)users[i].uid==current_uid) return &users[i];
    return &users[0];
}

void users_set_current(int uid) { current_uid = uid; }

void users_list(void) {
    vga_set_color(VGA_YELLOW,VGA_BLACK);
    vga_puts("\n  UID  NAME      ROOT  HOME\n\n");
    vga_set_color(VGA_WHITE,VGA_BLACK);
    for (int i=0;i<num_users;i++) {
        if (!users[i].active) continue;
        vga_put_dec(users[i].uid); vga_puts("    ");
        vga_puts(users[i].name);
        for (int j=(int)kstrlen(users[i].name);j<10;j++) vga_putchar(' ');
        vga_puts(users[i].is_root?"yes   ":"no    ");
        vga_puts(users[i].home);
        vga_putchar('\n');
    }
    vga_putchar('\n');
}
