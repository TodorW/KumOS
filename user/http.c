#include "kumos_libc.h"

#define SYS_TCP_CONNECT 40
#define SYS_TCP_SEND    41
#define SYS_TCP_RECV    42
#define SYS_TCP_CLOSE   43

static inline int sys_tcp_connect(uint32_t ip, uint16_t port) {
    int r; __asm__ volatile("int $0x80":"=a"(r):"a"(SYS_TCP_CONNECT),"b"(ip),"c"(port)); return r;
}
static inline int sys_tcp_send(int s, const void *buf, uint16_t len) {
    int r; __asm__ volatile("int $0x80":"=a"(r):"a"(SYS_TCP_SEND),"b"(s),"c"(buf),"d"(len)); return r;
}
static inline int sys_tcp_recv(int s, void *buf, uint16_t len) {
    int r; __asm__ volatile("int $0x80":"=a"(r):"a"(SYS_TCP_RECV),"b"(s),"c"(buf),"d"(len)); return r;
}
static inline void sys_tcp_close(int s) {
    __asm__ volatile("int $0x80"::"a"(SYS_TCP_CLOSE),"b"(s));
}

int main(void) {
    uint32_t ip = (10<<24)|(0<<16)|(2<<8)|2;
    uint16_t port = 80;

    printf("\n  http — KumOS HTTP client\n");
    printf("  Connecting to %u.%u.%u.%u:%u...\n",
           (ip>>24)&0xFF,(ip>>16)&0xFF,(ip>>8)&0xFF,ip&0xFF, port);

    int s = sys_tcp_connect(ip, port);
    if (s < 0) {
        puts("  Connection failed.");
        puts("  (Start a server: python3 -m http.server 80)");
        return 1;
    }

    puts("  Connected!");
    const char *req = "GET / HTTP/1.0\r\nHost: 10.0.2.2\r\n\r\n";
    sys_tcp_send(s, req, (uint16_t)strlen(req));

    char buf[512]; int total=0;
    puts("  Response:\n");
    for (int tries=0; tries<100; tries++) {
        int n = sys_tcp_recv(s, buf, 511);
        if (n > 0) {
            buf[n]=0; fputs(buf); total+=n;
        }
        sleep(20);
        if (total > 0 && n == 0) break;
    }

    printf("\n  Received %d bytes.\n\n", total);
    sys_tcp_close(s);
    return 0;
}
