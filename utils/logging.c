#include "logging.h"
#include <stdio.h> // For printf

// Function to log events to the console
void log_to_console(const char *event) {
    // In a real OS, this might write to a log file or a dedicated logging device
    printf("LOG: %s\n", event); // Simple console logging for demonstration
}