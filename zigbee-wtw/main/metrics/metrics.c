#include <stdint.h>
#include "metrics.h"

// Static array to hold counter values
static uint32_t metric_counters[METRIC_COUNT];

// Initialize all metrics counters to zero
void metrics_init(void) {
    for (int i = 0; i < METRIC_COUNT; i++) {
        metric_counters[i] = 0;
    }
}

// Increment a specific metric counter
void metrics_increment(metric_id_t metric_id) {
    if (metric_id < METRIC_COUNT) {
        metric_counters[metric_id]++;
    }
}

// Get the value of a specific metric counter
uint32_t metrics_get(metric_id_t metric_id) {
    if (metric_id < METRIC_COUNT) {
        return metric_counters[metric_id];
    }
    return 0;
}