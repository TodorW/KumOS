#include "kumos_libc.h"

static const char *fortunes[] = {
    "A clean disk is a happy disk.",
    "The pipe you fix today will clog again tomorrow.",
    "Real programmers count from zero.",
    "Somewhere, a ring-3 process is segfaulting for you.",
    "Fetched over the network. Yes, really.",
    "The kernel giveth, and the kernel taketh away.",
    "You will soon debug a bug you already fixed once.",
    "Ship it. Then fix the fallout.",
};

int main(void) {
    uint32_t seed = uptime() + (uint32_t)getpid() * 7919u;
    int n = (int)(sizeof(fortunes)/sizeof(fortunes[0]));
    int idx = (int)(seed % (uint32_t)n);

    printf("\n");
    printf("  ( fortune )\n");
    printf("  %s\n\n", fortunes[idx]);
    return 0;
}
