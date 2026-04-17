#ifndef SCHED_H
#define SCHED_H

#include <stdint.h>
#include "idt.h"

#define SCHED_STACK_SIZE  4096
#define SCHED_MAX_TASKS   16
#define SCHED_QUANTUM     10

typedef enum {
    TASK_RUNNING  = 0,
    TASK_READY    = 1,
    TASK_SLEEPING = 2,
    TASK_DEAD     = 3,
    TASK_ZOMBIE   = 4,
} task_state_t;

typedef struct {
    uint32_t     esp;
    uint32_t    *stack;
    uint32_t     stack_size;
    task_state_t state;
    int          pid;
    int          parent_pid;
    int          exit_code;
    char         name[32];
    uint32_t     ticks;
    uint32_t     sleep_until;
    int          kum_level;
} task_t;

void    sched_init(void);
int     sched_spawn(const char *name, void (*entry)(void), int kum_level);
void    sched_exit(void);
void    sched_exit_code(int code);
void    sched_sleep(uint32_t ms);
void    sched_yield(void);
void    sched_tick(registers_t *r);
task_t *sched_current(void);
task_t *sched_get_task(int pid);
int     sched_waitpid(int pid);
int     sched_wait(int *exit_code);
void    sched_list(void);

#endif