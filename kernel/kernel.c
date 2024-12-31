// kernel.c
#include "kernel/scheduler/scheduler.h"
#include "kernel/interrupts/interrupts.h"
#include "kernel/syscalls/syscalls.h"
#include "kernel/process_management/process.h"
#include "kernel/memory_management/memory.h"
#include "utils/error_handling.h" // New error handling header
#include "utils/statistics.h"     // New statistics header
#include <string.h>               // For string manipulation
#include <stdlib.h>               // For malloc and free
#include <stdio.h>                // For snprintf
#include <time.h>                 // For time functions

// Global variable to track kernel statistics
typedef struct {
    unsigned long context_switches;
    unsigned long processes_created;
    unsigned long memory_allocated; // in bytes
    unsigned long boot_time;         // in milliseconds
    unsigned long cpu_time;          // in milliseconds
    unsigned long io_operations;     // Count of I/O operations
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

// Log levels
typedef enum {
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR,
    LOG_DEBUG
} LogLevel;

// Error codes
typedef enum {
    ERR_NONE = 0,
    ERR_INVALID_CONFIG,
    ERR_PROCESS_CREATION_FAILED,
    ERR_MEMORY_INIT_FAILED,
    ERR_INTERRUPT_INIT_FAILED,
    ERR_SCHEDULER_INIT_FAILED,
    ERR_UNKNOWN
} ErrorCode;

// Function to get the current timestamp as a string
const char* get_current_timestamp() {
    static char timestamp[20];
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);
    return timestamp;
}

// Function to log kernel events with improved logging
void log_event(LogLevel level, const char *event) {
    const char *level_strings[] = { "INFO", "WARN", "ERROR", "DEBUG" };
    
    // Format the log message with a timestamp
    char log_message[512];
    snprintf(log_message, sizeof(log_message), "[%s] [%s] %s", 
             get_current_timestamp(), level_strings[level], event);
    
    // Log to console
    log_to_console(log_message);
    
    // Optionally, log to a file (append mode)
    FILE *log_file = fopen("kernel.log", "a");
    if (log_file) {
        fprintf(log_file, "%s\n", log_message);
        fclose(log_file);
    }
}

// Function to handle kernel errors
void handle_error(ErrorCode error_code, const char *context_message) {
    switch (error_code) {
        case ERR_INVALID_CONFIG:
            log_event(LOG_ERROR, "Invalid configuration detected.");
            break;
        case ERR_PROCESS_CREATION_FAILED:
            log_event(LOG_ERROR, "Failed to create a process.");
            break;
        case ERR_MEMORY_INIT_FAILED:
            log_event(LOG_ERROR, "Memory initialization failed.");
            break;
        case ERR_INTERRUPT_INIT_FAILED:
            log_event(LOG_ERROR, "Interrupt initialization failed.");
            break;
        case ERR_SCHEDULER_INIT_FAILED:
            log_event(LOG_ERROR, "Scheduler initialization failed.");
            break;
        default:
            log_event(LOG_ERROR, "An unknown error occurred.");
            break;
    }
    if (context_message) {
        log_event(LOG_ERROR, context_message);
    }
    // Perform any necessary cleanup or shutdown
    kernel_panic ("Kernel panic due to unrecoverable error.");
}

// Function to handle kernel panic
void kernel_panic(const char *message) {
    log_event(LOG_ERROR, message);
 // Log the panic message to a file (append mode)
    FILE *panic_file = fopen("kernel_panic.log", "a");
    if (panic_file) {
        fprintf(panic_file, "%s\n", message);
        fclose(panic_file);
    }
    // Halt the system
    while (1) {
        // Halt the CPU or enter a low-power state
    }
}

// Function to format and log kernel version information
void log_kernel_version() {
    char version_message[256];
    snprintf(version_message, sizeof(version_message),
             "Kernel Version: %s, Release Date: %s, Author: %s",
             kernel_version_info.version,
             kernel_version_info.release_date,
             kernel_version_info.author);
    log_event(LOG_INFO, version_message);
}

// Function to start the first process
void start_first_process() {
    log_event(LOG_INFO, "Starting the first process...");
    if (create_process("init") == -1) {
        handle_error(ERR_PROCESS_CREATION_FAILED, "Failed to create the initial process.");
    }
}

// Function to load kernel configuration
void load_kernel_config(const char *config_file) {
    log_event(LOG_INFO, "Loading kernel configuration...");

    // Example of setting configuration with validation
    int new_max_processes = 256; // Example modification
    int new_memory_limit = 1024;  // Example modification
    int new_scheduler_type = 1;    // Example modification

    if (new_max_processes <= 0) {
        handle_error(ERR_INVALID_CONFIG, "Invalid maximum processes value.");
        return;
    }
    if (new_memory_limit <= 0) {
        handle_error(ERR_INVALID_CONFIG, "Invalid memory limit value.");
        return;
    }
    if (new_scheduler_type < 0 || new_scheduler_type > 1) {
        handle_error(ERR_INVALID_CONFIG, "Invalid scheduler type value.");
        return;
    }

    kernel_config.max_processes = new_max_processes;
    kernel_config.memory_limit = new_memory_limit;
    kernel_config.scheduler_type = new_scheduler_type;

    // Log the configuration changes
    char config_message[256];
    snprintf(config_message, sizeof(config_message),
             "Kernel Config - Max Processes: %d, Memory Limit: %d MB, Scheduler Type: %d",
             kernel_config.max_processes,
             kernel_config.memory_limit,
             kernel_config.scheduler_type);
    log_event(LOG_INFO, config_message);
}

// Function to initialize kernel components
void initialize_kernel_components() {
    // Initialize memory management
    if (memory_init() == -1) {
        handle_error(ERR_MEMORY_INIT_FAILED, "Memory initialization failed.");
        return;
    }
    log_event(LOG_INFO, "Memory management initialized.");

    // Initialize the process management system
    if (process_init() == -1) {
        handle_error(ERR_PROCESS_CREATION_FAILED, "Process management initialization failed.");
        return;
    }
    log_event(LOG_INFO, "Process management initialized.");

    // Initialize interrupts
    if (interrupts_init() == -1) {
        handle_error(ERR_INTERRUPT_INIT_FAILED, "Interrupt initialization failed.");
        return;
    }
    log_event(LOG_INFO, "Interrupts initialized.");

    // Initialize the scheduler
    if (scheduler_init() == -1) {
        handle_error(ERR_SCHEDULER_INIT_FAILED, "Scheduler initialization failed.");
        return;
    }
    log_event(LOG_INFO, "Scheduler initialized.");
}

// Function to measure boot time
void measure_boot_time() {
    struct timespec start_time, end_time;
    clock_gettime(CLOCK_MONOTONIC, &start_time);
    // Initialize kernel components
    initialize_kernel_components();
    clock_gettime(CLOCK_MONOTONIC, &end_time);
    kernel_stats.boot_time = (end_time.tv_sec - start_time.tv_sec) * 1000 +
                             (end_time.tv_nsec - start_time.tv_nsec) / 1000000;
    log_event(LOG_INFO, "Boot time measurement: %ld ms", kernel_stats.boot_time);
}

// Function to track CPU time usage
void track_cpu_time() {
    struct timespec current_time;
    clock_gettime(CLOCK_MONOTONIC, &current_time);
    kernel_stats.cpu_time = (current_time.tv_sec * 1000) + (current_time.tv_nsec / 1000000);
    log_event(LOG_INFO, "CPU time usage: %ld ms", kernel_stats.cpu_time);
}

// Function to track I/O operations
void track_io_operations() {
    kernel_stats.io_operations++;
    log_event(LOG_INFO, "I/O operation count: %ld", kernel_stats.io_operations);
}

// Debugging hook for logging specific events
void debug_hook(const char *event) {
    log_event(LOG_DEBUG, event);
}

// Dynamic kernel parameter modification function
void modify_kernel_parameter (const char *parameter, int value) {
    if (strcmp(parameter, "max_processes") == 0) {
        kernel_config.max_processes = value;
    } else if (strcmp(parameter, "memory_limit") == 0) {
        kernel_config.memory_limit = value;
    } else if (strcmp(parameter, "scheduler_type") == 0) {
        kernel_config.scheduler_type = value;
    } else {
        log_event(LOG_WARN, "Unknown kernel parameter modification request.");
    }
}

// Main kernel function
void kernel_main() {
    log_kernel_version();

    // Load kernel configuration
    load_kernel_config("kernel_config.txt"); // Example config file

    // Measure boot time
    measure_boot_time();

    // Start the first process (usually the shell or init)
    start_first_process();

    // Main loop of the kernel
    while (1) {
        // Scheduler will handle process switching
        schedule();
        
        // Increment context switch count
        kernel_stats.context_switches++;
        
        // Track CPU time usage
        track_cpu_time();
        
        // Track I/O operations
        track_io_operations();
    }
}
