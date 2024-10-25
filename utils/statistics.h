#ifndef STATISTICS_H
#define STATISTICS_H

// Structure to hold kernel statistics
typedef struct {
    unsigned long context_switches; // Count of context switches
    // Add more statistics as needed
} KernelStats;

// Function to initialize kernel statistics
void init_kernel_stats(KernelStats *stats);

#endif // STATISTICS_H