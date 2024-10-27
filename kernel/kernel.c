// kernel.c
#include "kernel/scheduler/scheduler.h"
#include "kernel/interrupts/interrupts.h"
#include "kernel/syscalls/syscalls.h"
#include "kernel/process_management/process.h"
#include "kernel/memory_management/memory.h"
#include "utils/logging.h"        // New logging header
#include "utils/error_handling.h" // New error handling header
#include "utils/statistics.h"     // New statistics header
#include <string.h>               // For string manipulation

// Global variable to track kernel statistics
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

// Kernel configuration structure
typedef struct {
    int max_processes;
    int memory_limit; // in MB
    int scheduler_type; // 0 for round-robin, 1 for priority-based, etc.
} KernelConfig;

// Default kernel configuration
static KernelConfig kernel_config = {
    .max_processes = 128,
    .memory_limit = 512,
    .scheduler_type = 0
};

// Function to log kernel events
void log_event(const char *event) {
    // Log the event (this could be to a console, file, etc.)
    log_to_console(event);
}

// Function to log kernel version information
void log_kernel_version() {
    char version_message[256];
    snprintf(version_message, sizeof(version_message),
             "Kernel Version: %s, Release Date: %s, Author: %s",
             kernel_version_info.version,
             kernel_version_info.release_date,
             kernel_version_info.author);
    log_event(version_message);
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

// Function to load kernel configuration
void load_kernel_config(const char *config_file) {
    // For simplicity, we'll just use hardcoded values instead of file reading
    // In a real implementation, you would read from a file and parse it
    log_event("Loading kernel configuration...");
    
    // Example of setting configuration
    kernel_config.max_processes = 256; // Example modification
    kernel_config.memory_limit = 1024;  // Example modification
    kernel_config.scheduler_type = 1;    // Example modification

    // Log the configuration changes
    char config_message[256];
    snprintf(config_message, sizeof(config_message),
             "Kernel Config - Max Processes: %d, Memory Limit: %d MB, Scheduler Type: %d",
             kernel_config.max_processes,
             kernel_config.memory_limit,
             kernel_config.scheduler_type);
    log_event(config_message);
}

// Main kernel function
void kernel_main() {
    // Log the kernel version information
    log_kernel_version();

    // Load kernel configuration
    load_kernel_config("kernel_config.txt"); // Example config file

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