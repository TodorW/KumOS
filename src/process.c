#include "process.h"
#include "vga.h"
#include "kstring.h"

static process_t proc_table[MAX_PROCESSES];
static int       next_pid = 1;
static int       proc_count_val = 0;

void proc_init(void) {
    kmemset(proc_table, 0, sizeof(proc_table));
    proc_count_val = 0;
    next_pid = 1;

    proc_table[0].pid       = 0;
    proc_table[0].state     = PROC_IDLE;
    proc_table[0].memory_kb = 4;
    proc_table[0].kum_level = 1;
    kstrcpy(proc_table[0].name, "idle");
    proc_count_val = 1;

    proc_table[1].pid       = 1;
    proc_table[1].state     = PROC_RUNNING;
    proc_table[1].memory_kb = 128;
    proc_table[1].kum_level = 1;
    kstrcpy(proc_table[1].name, "kumos");
    proc_count_val = 2;
    next_pid = 2;
}

int proc_spawn(const char *name, uint32_t mem_kb, int kum_level) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (proc_table[i].state == PROC_ZOMBIE || (proc_table[i].pid == 0 && i > 0)) {

        }
    }
    if (proc_count_val >= MAX_PROCESSES) return -1;

    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (proc_table[i].state == PROC_ZOMBIE) {
            proc_table[i].pid       = next_pid++;
            proc_table[i].state     = PROC_RUNNING;
            proc_table[i].memory_kb = mem_kb;
            proc_table[i].kum_level = kum_level;
            proc_table[i].ticks     = 0;
            kstrcpy(proc_table[i].name, name);
            proc_count_val++;
            return proc_table[i].pid;
        }
    }
    if (proc_count_val < MAX_PROCESSES) {
        int i = proc_count_val;
        proc_table[i].pid       = next_pid++;
        proc_table[i].state     = PROC_RUNNING;
        proc_table[i].memory_kb = mem_kb;
        proc_table[i].kum_level = kum_level;
        proc_table[i].ticks     = 0;
        kstrcpy(proc_table[i].name, name);
        proc_count_val++;
        return proc_table[i].pid;
    }
    return -1;
}

void proc_kill(int pid) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (proc_table[i].pid == pid) {
            proc_table[i].state = PROC_ZOMBIE;
            proc_count_val--;
            return;
        }
    }
}

process_t *proc_get(int pid) {
    for (int i = 0; i < MAX_PROCESSES; i++)
        if (proc_table[i].pid == pid) return &proc_table[i];
    return 0;
}

int proc_count(void) { return proc_count_val; }

void proc_tick(void) {
    for (int i = 0; i < MAX_PROCESSES; i++)
        if (proc_table[i].state == PROC_RUNNING)
            proc_table[i].ticks++;
}

static const char *state_str(proc_state_t s) {
    switch(s) {
        case PROC_RUNNING:  return "RUN ";
        case PROC_SLEEPING: return "SLEEP";
        case PROC_ZOMBIE:   return "DEAD ";
        case PROC_IDLE:     return "IDLE ";
        default:            return "?    ";
    }
}

void proc_list_all(void) {
    vga_set_color(VGA_YELLOW, VGA_BLACK);
    vga_puts("  PID  STATE  MEM(KB)  KUM  NAME\n");
    vga_puts("  ---  -----  -------  ---  ----\n");
    vga_set_color(VGA_WHITE, VGA_BLACK);
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (proc_table[i].state == PROC_ZOMBIE && proc_table[i].pid == 0) continue;
        if (proc_table[i].pid == 0 && i > 0) continue;
        vga_puts("  ");
        vga_put_dec(proc_table[i].pid);
        vga_puts("    ");
        vga_puts(state_str(proc_table[i].state));
        vga_puts("  ");
        vga_put_dec(proc_table[i].memory_kb);
        vga_puts("     ");
        vga_puts(proc_table[i].kum_level ? "yes" : "no ");
        vga_puts("  ");
        vga_puts(proc_table[i].name);
        vga_putchar('\n');
    }
}