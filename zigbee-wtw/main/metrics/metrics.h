#pragma once
#ifndef METRICS_H
#define METRICS_H

#include <stdint.h>

// Enum for metric IDs
typedef enum {
    METRIC_ZIGBEE_CMD_RECEIVED,
    METRIC_OTA_STARTED,
    METRIC_COUNT  // Keep this last for array sizing
} metric_id_t;

// Function declarations
void metrics_init(void);
void metrics_increment(metric_id_t metric_id);
uint32_t metrics_get(metric_id_t metric_id);

#endif // METRICS_H