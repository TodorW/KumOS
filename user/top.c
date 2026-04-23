#include "kumos_libc.h"

#define SYS_LISTDIR_PROC 14

static int read_proc(const char *name, char *buf, int sz) {
    char path[64]; strcpy(path, "/proc/"); strcat(path, name);
    int fd = _syscall(SYS_OPEN, (int)path, O_RDONLY, 0);
    if (fd < 0) return -1;
    int n = fread(fd, buf, (uint32_t)(sz-1));
    close(fd);
    if (n > 0) buf[n]=0;
    return n;
}

int main(void) {
    char buf[1024];
    int passes = 0;

    puts("\n  top — KumOS process monitor  (5 passes, 1s interval)\n");

    while (passes++ < 5) {
        puts("  ----------------------------------------");
        printf("  Uptime: %us\n\n", uptime());

        if (read_proc("meminfo", buf, sizeof(buf)) > 0) {
            printf("  %s", buf);
        }

        puts("\n  Processes:\n");
        if (read_proc("ps", buf, sizeof(buf)) > 0) {
            char *p = buf;
            while (*p) {
                char *nl = strchr(p, '\n');
                if (nl) *nl = 0;
                printf("    %s\n", p);
                p = nl ? nl+1 : p+strlen(p);
            }
        }

        puts("");
        sleep(1000);
    }
    return 0;
}
