#include "signal.h"
#include "sched.h"
#include "vga.h"
#include "kstring.h"
#include <stdint.h>

static sigset_t proc_signals[SCHED_MAX_TASKS];

void signal_init(void) {
    kmemset(proc_signals, 0, sizeof(proc_signals));
}

static int pid_to_idx(int pid) {
    for (int i = 0; i < SCHED_MAX_TASKS; i++) {
        task_t *t = sched_get_task(pid);
        if (t && t->pid == pid) return i;
    }
    return -1;
}

int signal_send(int pid, int sig) {
    if (sig <= 0 || sig >= NSIG) return -1;

    if (sig == SIGKILL) {
        task_t *t = sched_get_task(pid);
        if (!t) return -1;
        sched_exit_code(-1);
        return 0;
    }

    task_t *t = sched_get_task(pid);
    if (!t) return -1;

    int idx = 0;
    for (int i = 0; i < SCHED_MAX_TASKS; i++) {
        task_t *ti = sched_get_task(i + 1);
        if (ti && ti->pid == pid) { idx = i; break; }
    }

    proc_signals[idx].pending |= (1u << sig);

    if (sig == SIGINT || sig == SIGTERM || sig == SIGKILL) {
        task_t *task = sched_get_task(pid);
        if (task) task->state = TASK_ZOMBIE;
    }
    return 0;
}

int signal_set_handler(int sig, sighandler_t handler) {
    if (sig <= 0 || sig >= NSIG) return -1;
    task_t *cur = sched_current();
    int idx = 0;
    for (int i = 0; i < SCHED_MAX_TASKS; i++) {
        task_t *t = sched_get_task(i+1);
        if (t && t->pid == cur->pid) { idx = i; break; }
    }
    proc_signals[idx].handlers[sig] = handler;
    return 0;
}

void signal_check(void) {
    task_t *cur = sched_current();
    int idx = 0;
    for (int i = 0; i < SCHED_MAX_TASKS; i++) {
        task_t *t = sched_get_task(i+1);
        if (t && t->pid == cur->pid) { idx = i; break; }
    }
    uint32_t pending = proc_signals[idx].pending & ~proc_signals[idx].blocked;
    if (!pending) return;
    for (int sig = 1; sig < NSIG; sig++) {
        if (!(pending & (1u << sig))) continue;
        proc_signals[idx].pending &= ~(1u << sig);
        sighandler_t h = proc_signals[idx].handlers[sig];
        if ((uintptr_t)h == SIG_IGN) continue;
        if ((uintptr_t)h == SIG_DFL || h == 0) {
            if (sig == SIGCHLD || sig == SIGCONT) continue;
            sched_exit_code(128 + sig);
            return;
        }
        h(sig);
    }
}

int signal_pending(void) {
    task_t *cur = sched_current();
    int idx = 0;
    for (int i = 0; i < SCHED_MAX_TASKS; i++) {
        task_t *t = sched_get_task(i+1);
        if (t && t->pid == cur->pid) { idx = i; break; }
    }
    return (proc_signals[idx].pending & ~proc_signals[idx].blocked) != 0;
}
