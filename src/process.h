#ifndef PROCESS_H
#define PROCESS_H

#include <stdint.h>

#define MAX_PROCESSES 16
#define PROC_NAME_LEN 32

typedef enum {
    PROC_RUNNING = 0,
    PROC_SLEEPING,
    PROC_ZOMBIE,
    PROC_IDLE,
} proc_state_t;

typedef struct {
    int         pid;
    char        name[PROC_NAME_LEN];
    proc_state_t state;
    uint32_t    memory_kb;
    uint32_t    ticks;
    int         kum_level;
} process_t;

void      proc_init(void);
int       proc_spawn(const char *name, uint32_t mem_kb, int kum_level);
void      proc_kill(int pid);
process_t *proc_get(int pid);
int       proc_count(void);
void      proc_tick(void);
void      proc_list_all(void);

#endif