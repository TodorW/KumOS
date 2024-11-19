#include <stdint.h>
#include <stddef.h>

#define SERIAL_PORT_COM1 0x3F8
#define SERIAL_LINE_ENABLE_DLAB 0x80
#define SERIAL_DATA_BITS_8 0x03
#define SERIAL_FIFO_ENABLE 0xC7
#define SERIAL_MODEM_READY 0x03
#define SERIAL_LINE_STATUS 5
#define SERIAL_LINE_STATUS_EMPTY 0x20

static inline void outb(uint16_t port, uint8_t value) {
    asm volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

void serial_init(uint16_t port) {
    outb(port + 1, 0x00);
    outb(port + 3, SERIAL_LINE_ENABLE_DLAB);
    outb(port, 0x01);
    outb(port + 1, 0x00);
    outb(port + 3, SERIAL_DATA_BITS_8);
    outb(port + 2, SERIAL_FIFO_ENABLE);
    outb(port + 4, SERIAL_MODEM_READY);
}

int serial_is_transmit_empty(uint16_t port) {
    return inb(port + SERIAL_LINE_STATUS) & SERIAL_LINE_STATUS_EMPTY;
}

void serial_write(uint16_t port, char c) {
    while (!serial_is_transmit_empty(port));
    outb(port, c);
}

void serial_write_string(uint16_t port, const char* str) {
    while (*str) {
        serial_write(port, *str++);
    }
}
