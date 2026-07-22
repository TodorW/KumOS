#include "kumos_libc.h"

int main(void) {
    printf("\n  [echo] Type something (ring-3 read via syscall):\n  > ");
    char buf[128];
    int n = read(buf, 127);
    buf[n] = 0;
    printf("  You typed: '%s'\n\n", buf);
    return 0;
}
