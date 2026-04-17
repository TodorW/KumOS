#include "sched.h"
#include "timer.h"
#include "vga.h"
#include "kstring.h"
#include "kmalloc.h"
#include "gdt.h"
#include <stdint.h>

static task_t   tasks[SCHED_MAX_TASKS];
static int      task_count  = 0;
static int      current_idx = 0;
static int      next_pid    = 1;
static uint32_t tick_accum  = 0;

extern void switch_context(uint32_t *old_esp, uint32_t new_esp);

static void task_setup_stack(task_t *t, void (*entry)(void)) {

    uint32_t *sp = (uint32_t *)((uint8_t *)t->stack + t->stack_size);

    *--sp = 0x10;
    uint32_t esp_val = (uint32_t)sp + 20;
    *--sp = esp_val;
    *--sp = 0x202;
    *--sp = 0x08;
    *--sp = (uint32_t)entry;

    *--sp = 0;
    *--sp = 0;
    *--sp = 0;
    *--sp = 0;
    *--sp = 0;
    *--sp = 0;
    *--sp = 0;
    *--sp = 0;

    *--sp = 0x10;

    t->esp = (uint32_t)sp;
}

static void idle_task(void) {
    while (1) {
        __asm__ volatile ("hlt");
    }
}

void sched_init(void) {
    kmemset(tasks, 0, sizeof(tasks));
    task_count  = 0;
    current_idx = 0;
    tick_accum  = 0;

    tasks[0].pid        = next_pid++;
    tasks[0].state      = TASK_RUNNING;
    tasks[0].stack      = 0;
    tasks[0].kum_level  = 1;
    tasks[0].stack_size = 0;
    tasks[0].parent_pid = 0;
    kstrcpy(tasks[0].name, "kshell");
    task_count = 1;

    uint32_t *idle_stack = kmalloc(SCHED_STACK_SIZE);
    if (idle_stack) {
        tasks[1].pid        = next_pid++;
        tasks[1].state      = TASK_READY;
        tasks[1].stack      = idle_stack;
        tasks[1].stack_size = SCHED_STACK_SIZE;
        tasks[1].kum_level  = 1;
        tasks[1].parent_pid = 0;
        kstrcpy(tasks[1].name, "idle");
        task_setup_stack(&tasks[1], idle_task);
        task_count = 2;
    }
}

int sched_spawn(const char *name, void (*entry)(void), int kum_level) {
    if (task_count >= SCHED_MAX_TASKS) return -1;

    uint32_t *stack = kmalloc(SCHED_STACK_SIZE);
    if (!stack) return -1;

    int slot = -1;
    for (int i = 0; i < SCHED_MAX_TASKS; i++) {
        if (tasks[i].state == TASK_DEAD || tasks[i].state == TASK_ZOMBIE
            || tasks[i].pid == 0) {
            slot = i; break;
        }
    }
    if (slot < 0) slot = task_count;
    if (slot >= SCHED_MAX_TASKS) { kfree(stack); return -1; }

    tasks[slot].pid        = next_pid++;
    tasks[slot].state      = TASK_READY;
    tasks[slot].stack      = stack;
    tasks[slot].stack_size = SCHED_STACK_SIZE;
    tasks[slot].ticks      = 0;
    tasks[slot].exit_code  = 0;
    tasks[slot].kum_level  = kum_level;
    tasks[slot].parent_pid = tasks[current_idx].pid;
    kstrcpy(tasks[slot].name, name);
    task_setup_stack(&tasks[slot], entry);

    if (slot >= task_count) task_count = slot + 1;
    return tasks[slot].pid;
}

void sched_exit(void) {
    sched_exit_code(0);
}

void sched_exit_code(int code) {
    __asm__ volatile ("cli");
    tasks[current_idx].exit_code = code;

    int has_parent = 0;
    for (int i = 0; i < task_count; i++) {
        if (tasks[i].pid == tasks[current_idx].parent_pid
            && tasks[i].state != TASK_DEAD
            && tasks[i].state != TASK_ZOMBIE) {
            has_parent = 1; break;
        }
    }
    if (has_parent) {
        tasks[current_idx].state = TASK_ZOMBIE;

        if (tasks[current_idx].stack) {
            kfree(tasks[current_idx].stack);
            tasks[current_idx].stack = 0;
        }
    } else {
        tasks[current_idx].state = TASK_DEAD;
        if (tasks[current_idx].stack) {
            kfree(tasks[current_idx].stack);
            tasks[current_idx].stack = 0;
        }
    }
    __asm__ volatile ("sti");
    sched_yield();
}

void sched_sleep(uint32_t ms) {
    __asm__ volatile ("cli");
    tasks[current_idx].state       = TASK_SLEEPING;
    tasks[current_idx].sleep_until = timer_ticks() + (ms * 100 / 1000);
    __asm__ volatile ("sti");
    sched_yield();
}

static int pick_next(void) {
    int start = (current_idx + 1) % task_count;
    int i = start;
    do {

        if (tasks[i].state == TASK_SLEEPING &&
            timer_ticks() >= tasks[i].sleep_until) {
            tasks[i].state = TASK_READY;
        }
        if (tasks[i].state == TASK_READY || tasks[i].state == TASK_RUNNING)
            return i;
        i = (i + 1) % task_count;
    } while (i != start);
    return 0;
}

void sched_yield(void) {
    __asm__ volatile ("cli");
    int next = pick_next();
    if (next == current_idx) { __asm__ volatile ("sti"); return; }

    int prev = current_idx;
    if (tasks[prev].state == TASK_RUNNING)
        tasks[prev].state = TASK_READY;
    tasks[next].state = TASK_RUNNING;
    current_idx = next;

    tss_set_kernel_stack((uint32_t)tasks[next].stack + tasks[next].stack_size);

    uint32_t *old_esp_ptr = &tasks[prev].esp;
    uint32_t  new_esp     =  tasks[next].esp;

    __asm__ volatile ("sti");
    switch_context(old_esp_ptr, new_esp);
}

void sched_tick(registers_t *r) {
    (void)r;
    tasks[current_idx].ticks++;
    tick_accum++;

    for (int i = 0; i < task_count; i++) {
        if (tasks[i].state == TASK_SLEEPING &&
            timer_ticks() >= tasks[i].sleep_until) {
            tasks[i].state = TASK_READY;
        }
    }

    if (tick_accum >= SCHED_QUANTUM) {
        tick_accum = 0;
        int next = pick_next();
        if (next != current_idx) {
            int prev = current_idx;
            if (tasks[prev].state == TASK_RUNNING)
                tasks[prev].state = TASK_READY;
            tasks[next].state = TASK_RUNNING;
            current_idx = next;
            tss_set_kernel_stack(
                (uint32_t)tasks[next].stack + tasks[next].stack_size);

            uint32_t *old_ptr = &tasks[prev].esp;
            uint32_t  new_esp =  tasks[next].esp;
            switch_context(old_ptr, new_esp);
        }
    }
}

task_t *sched_current(void) {
    return &tasks[current_idx];
}

task_t *sched_get_task(int pid) {
    for (int i = 0; i < task_count; i++)
        if (tasks[i].pid == pid && tasks[i].state != TASK_DEAD)
            return &tasks[i];
    return 0;
}

static const char *tstate(task_state_t s) {
    switch(s) {
        case TASK_RUNNING:  return "RUN  ";
        case TASK_READY:    return "READY";
        case TASK_SLEEPING: return "SLEEP";
        case TASK_DEAD:     return "DEAD ";
        case TASK_ZOMBIE:   return "ZOMBI";
        default:            return "?    ";
    }
}

void sched_list(void) {
    vga_set_color(VGA_YELLOW, VGA_BLACK);
    vga_puts("  PID  STATE  TICKS   KUM  NAME\n");
    vga_puts("  ---  -----  -----   ---  ----\n");
    vga_set_color(VGA_WHITE, VGA_BLACK);
    for (int i = 0; i < task_count; i++) {
        if (tasks[i].state == TASK_DEAD && tasks[i].pid == 0) continue;
        vga_puts("  ");
        vga_put_dec(tasks[i].pid);
        vga_puts("    ");
        vga_puts(tstate(tasks[i].state));
        vga_puts("  ");
        vga_put_dec(tasks[i].ticks);
        vga_puts("    ");
        vga_puts(tasks[i].kum_level ? "yes" : "no ");
        vga_puts("  ");
        if (i == current_idx) {
            vga_set_color(VGA_GREEN, VGA_BLACK);
            vga_puts(tasks[i].name);
            vga_puts(" *");
            vga_set_color(VGA_WHITE, VGA_BLACK);
        } else {
            vga_puts(tasks[i].name);
        }
        vga_putchar('\n');
    }
}

int sched_waitpid(int pid) {
    while (1) {
        for (int i = 0; i < task_count; i++) {
            if (tasks[i].pid == pid) {
                if (tasks[i].state == TASK_ZOMBIE || tasks[i].state == TASK_DEAD) {
                    int code = tasks[i].exit_code;
                    tasks[i].state = TASK_DEAD;
                    return code;
                }
                break;
            }
        }

        int found = 0;
        for (int i = 0; i < task_count; i++)
            if (tasks[i].pid == pid) { found = 1; break; }
        if (!found) return 0;

        sched_yield();
    }
}

int sched_wait(int *exit_code) {
    int my_pid = tasks[current_idx].pid;
    while (1) {
        for (int i = 0; i < task_count; i++) {
            if (tasks[i].parent_pid == my_pid
                && (tasks[i].state == TASK_ZOMBIE
                    || tasks[i].state == TASK_DEAD)) {
                int code = tasks[i].exit_code;
                int pid  = tasks[i].pid;
                tasks[i].state = TASK_DEAD;
                if (exit_code) *exit_code = code;
                return pid;
            }
        }

        int children = 0;
        for (int i = 0; i < task_count; i++)
            if (tasks[i].parent_pid == my_pid
                && tasks[i].state != TASK_DEAD)
                children++;
        if (!children) return -1;
        sched_yield();
    }
}