// kernel.c
//#include "scheduler.h" 
//#include "interrupts.h" 
//#include "syscalls.h" 
//#include "process.h" to be added
#include "memory.h"
#include "error_handling.h" 
#include "statistics.h"     
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

// Global variable to track kernel statistics
typedef struct {
    unsigned long context_switches;
    unsigned long processes_created;
    unsigned long memory_allocated; // in bytes
    unsigned long boot_time;        // in milliseconds
    unsigned long cpu_time;         // in milliseconds
    unsigned long io_operations;    // Count of I/O operations
} KernelStats;

static KernelStats kernel_stats = {0};

// Kernel version information structure
typedef struct {
    const char *version;
    const char *release_date;
    const char *author;
} KernelVersionInfo;

// Define the kernel version information
static KernelVersionInfo kernel_version_info = {
    .version = "1.0.0",
    .release_date = "2023-10-01",
    .author = "Your Name"
};

// Log levels
typedef enum {
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR,
    LOG_DEBUG
} LogLevel;

// Function to log kernel events
void log_event(LogLevel level, const char *event) {
    const char *level_strings[] = { "INFO", "WARN", "ERROR", "DEBUG" };

    char log_message[512];
    snprintf(log_message, sizeof(log_message), "[%s] [%s] %s",
             get_current_timestamp(), level_strings[level], event);

    // Log to console
    printf("%s\n", log_message);
}

// Function to get the current timestamp as a string
const char* get_current_timestamp() {
    static char timestamp[20];
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);
    return timestamp;
}

// Function to handle kernel panic
void kernel_panic(const char *message) {
    log_event(LOG_ERROR, message);
    while (1) {
        // Halt the system
    }
}

// Function to initialize kernel components
void initialize_kernel_components() {
    if (memory_init() == -1) {
        kernel_panic("Memory initialization failed.");
    }
    log_event(LOG_INFO, "Memory management initialized.");

    if (process_init() == -1) {
        kernel_panic("Process management initialization failed.");
    }
    log_event(LOG_INFO, "Process management initialized.");

    if (interrupts_init() == -1) {
        kernel_panic("Interrupt initialization failed.");
    }
    log_event(LOG_INFO, "Interrupts initialized.");

    if (scheduler_init() == -1) {
        kernel_panic("Scheduler initialization failed.");
    }
    log_event(LOG_INFO, "Scheduler initialized.");
}

// Main kernel function
void kernel_main() {
    log_event(LOG_INFO, "Kernel is starting...");

    // Measure boot time
    struct timespec start_time, end_time;
    clock_gettime(CLOCK_MONOTONIC, &start_time);

    // Initialize kernel components
    initialize_kernel_components();

    clock_gettime(CLOCK_MONOTONIC, &end_time);
    kernel_stats.boot_time = (end_time.tv_sec - start_time.tv_sec) * 1000 +
                             (end_time.tv_nsec - start_time.tv_nsec) / 1000000;

    char boot_time_message[128];
    snprintf(boot_time_message, sizeof(boot_time_message), "Boot time: %lu ms", kernel_stats.boot_time);
    log_event(LOG_INFO, boot_time_message);

    log_event(LOG_INFO, "Starting first process...");
    if (create_process("init") == -1) {
        kernel_panic("Failed to create the initial process.");
    }

    // Main loop
    while (1) {
        schedule();
        kernel_stats.context_switches++;
    }
}
