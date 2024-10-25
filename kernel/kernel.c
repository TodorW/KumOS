// kernel.c
#include "kernel/scheduler/scheduler.h"
#include "kernel/interrupts/interrupts.h"
#include "kernel/syscalls/syscalls.h"
#include "kernel/process_management/process.h"
#include "kernel/memory_management/memory.h"
#include "utils/logging.h"        // New logging header
#include "utils/error_handling.h" // New error handling header
#include "utils/statistics.h"     // New statistics header

// Global variable to track kernel statistics
static KernelStats kernel_stats = {0};

// Function to log kernel events
void log_event(const char *event) {
    // Log the event (this could be to a console, file, etc.)
    log_to_console(event);
}

// Function to handle kernel errors
void handle_error(const char *error_message) {
    log_event(error_message);
    // Perform any necessary cleanup or shutdown
    graceful_shutdown();
}

// Function to start the first process
void start_first_process() {
    // Log the event of starting the first process
    log_event("Starting the first process...");
    
    // Normally, this would be the shell or init process
    if (create_process("init") == -1) {
        handle_error("Failed to create the initial process.");
    }
}

// Function to gracefully shut down the kernel
void graceful_shutdown() {
    log_event("Shutting down the kernel gracefully...");
    // Perform any necessary cleanup here
    // For example, freeing resources, saving state, etc.
    
    // Exit the kernel (in a real OS, this would involve hardware-specific calls)
    while (1) {
        // Halt the CPU or enter a low-power state
    }
}

void kernel_main() {
    // Initialize memory management
    memory_init();
    log_event("Memory management initialized.");

    // Initialize the process management system
    process_init();
    log_event("Process management initialized.");

    // Initialize interrupts
    interrupts_init();
    log_event("Interrupts initialized.");

    // Initialize the scheduler
    scheduler_init();
    log_event("Scheduler initialized.");

    // Start the first process (usually the shell or init)
    start_first_process();

    // Main loop of the kernel
    while (1) {
        // Scheduler will handle process switching
        schedule();
        
        // Increment context switch count
        kernel_stats.context_switches++;
    }
}