#include <stdint.h>
#include <stdbool.h>
#include "io.h"          // Include port I/O functions (outb, inb)
#include "interrupts.h"  // Include interrupt handling
#include "print.h"       // For debugging (print to screen or log)

// PS/2 Controller Ports
#define MOUSE_DATA_PORT       0x60
#define MOUSE_COMMAND_PORT    0x64

// PS/2 Mouse Commands
#define ENABLE_MOUSE          0xF4
#define SET_DEFAULTS          0xF6

// Mouse packet structure
typedef struct {
    bool left_button;
    bool right_button;
    bool middle_button;
    int8_t x_offset;
    int8_t y_offset;
} mouse_packet_t;

// Global variables
static uint8_t mouse_cycle = 0;   // Keeps track of packet cycle (0, 1, 2)
static int8_t mouse_packet[3];   // Stores raw packet bytes
static mouse_packet_t mouse_data; // Decoded mouse data

// Function prototypes
void mouse_wait(uint8_t type);
void mouse_write(uint8_t data);
uint8_t mouse_read();
void mouse_handler();
void mouse_init();

// Wait for mouse readiness
void mouse_wait(uint8_t type) {
    uint16_t timeout = 100000;
    if (type == 0) { // Wait for input buffer to be clear
        while (timeout-- && (inb(MOUSE_COMMAND_PORT) & 0x02));
    } else { // Wait for output buffer to be ready
        while (timeout-- && !(inb(MOUSE_COMMAND_PORT) & 0x01));
    }
}

// Write a byte to the mouse
void mouse_write(uint8_t data) {
    mouse_wait(0);                   // Wait for input buffer to be clear
    outb(MOUSE_COMMAND_PORT, 0xD4);  // Tell the controller to send to the mouse
    mouse_wait(0);                   // Wait for input buffer to be clear again
    outb(MOUSE_DATA_PORT, data);     // Write the data to the mouse
}

// Read a byte from the mouse
uint8_t mouse_read() {
    mouse_wait(1);                   // Wait for output buffer to be ready
    return inb(MOUSE_DATA_PORT);     // Read the byte from the data port
}

// Mouse interrupt handler
void mouse_handler() {
    uint8_t status = inb(0x64);      // Read status byte
    if (!(status & 0x20)) return;    // Check if the mouse sent data

    int8_t data = inb(MOUSE_DATA_PORT); // Read data byte
    switch (mouse_cycle) {
        case 0:
            // First byte: Ignore if bit 3 is not set
            if (!(data & 0x08)) break;
            mouse_packet[0] = data;
            mouse_cycle++;
            break;
        case 1:
            // Second byte
            mouse_packet[1] = data;
            mouse_cycle++;
            break;
        case 2:
            // Third byte
            mouse_packet[2] = data;

            // Decode packet
            mouse_data.left_button = mouse_packet[0] & 0x01;
            mouse_data.right_button = mouse_packet[0] & 0x02;
            mouse_data.middle_button = mouse_packet[0] & 0x04;
            mouse_data.x_offset = mouse_packet[1];
            mouse_data.y_offset = mouse_packet[2];

            // Debug output
            print("Mouse: L=%d R=%d M=%d X=%d Y=%d\n",
                  mouse_data.left_button, mouse_data.right_button,
                  mouse_data.middle_button, mouse_data.x_offset, mouse_data.y_offset);

            mouse_cycle = 0; // Reset cycle
            break;
    }
}

// Initialize the mouse
void mouse_init() {
    // Enable the auxiliary device (mouse)
    mouse_wait(0);
    outb(MOUSE_COMMAND_PORT, 0xA8);

    // Enable interrupts
    mouse_wait(0);
    outb(MOUSE_COMMAND_PORT, 0x20);
    uint8_t status = (inb(MOUSE_DATA_PORT) | 2);
    mouse_wait(0);
    outb(MOUSE_COMMAND_PORT, 0x60);
    outb(MOUSE_DATA_PORT, status);

    // Set default settings
    mouse_write(SET_DEFAULTS);
    mouse_read(); // Acknowledge

    // Enable the mouse
    mouse_write(ENABLE_MOUSE);
    mouse_read(); // Acknowledge

    // Register the interrupt handler
    register_interrupt_handler(12, mouse_handler); // IRQ12 for PS/2 mouse
    print("Mouse initialized.\n");
}
