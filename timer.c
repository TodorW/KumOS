#include "kernel.h"

static volatile uint32_t system_ticks = 0;
static volatile uint32_t seconds = 0;
static volatile uint32_t tick_counter = 0;

void timer_init(void) {
    system_ticks = 0;
    seconds = 0;
    tick_counter = 0;
}

void timer_tick(void) {
    tick_counter++;
    // Simulate ~100Hz timing
    if (tick_counter >= 50000) {  //Adjustable based on actual timer frequency
        tick_counter = 0;
        system_ticks++;
        if (system_ticks >= 100) {
            system_ticks = 0;
            seconds++;
        }
    }
}

uint32_t timer_get_seconds(void) {
    return seconds;
}

uint32_t timer_get_ticks(void) {
    return system_ticks;
}

void timer_sleep_ticks(uint32_t ticks) {
    uint32_t end = system_ticks + ticks;
    while (system_ticks < end) {
        timer_tick();
    }
}

// Format uptime as HH:MM:SS
void timer_format_uptime(char* buffer) {
    uint32_t total_seconds = seconds;
    uint32_t hours = total_seconds / 3600;
    uint32_t minutes = (total_seconds % 3600) / 60;
    uint32_t secs = total_seconds % 60;
    
    // Format: HH:MM:SS
    buffer[0] = '0' + (hours / 10);
    buffer[1] = '0' + (hours % 10);
    buffer[2] = ':';
    buffer[3] = '0' + (minutes / 10);
    buffer[4] = '0' + (minutes % 10);
    buffer[5] = ':';
    buffer[6] = '0' + (secs / 10);
    buffer[7] = '0' + (secs % 10);
    buffer[8] = '\0';
}

uint32_t timer_ticks(void) {
    return seconds * 100 + system_ticks;
}

void timer_wait(uint32_t ticks) {
    timer_sleep_ticks(ticks);
}
