#ifndef USERSPACE_H
#define USERSPACE_H

#include <stdint.h>

int user_spawn(const char *name, void (*entry)(void));

void uprog_hello(void);
void uprog_counter(void);
void uprog_top(void);
void uprog_echo(void);
void uprog_cat(void);

typedef struct { const char *name; void (*entry)(void); } uprog_t;
extern uprog_t uprog_list[];
extern int     uprog_count;

#endif