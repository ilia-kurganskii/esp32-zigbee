/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Motion light endpoint */
#define MOTION_LIGHT_ENDPOINT   10

/**
 * @brief Initialize Zigbee motion light component.
 *
 * This function initializes the Zigbee stack, creates the necessary clusters
 * (Basic, Identify, On/Off, Occupancy Sensing), and starts the Zigbee task.
 *
 * @param wake_events Event group to signal when the Zigbee track is ready for sleep
 * @param ready_bit   Bit to set when joined, occupancy applied, and monitor done
 * @return Task handle on success, NULL on failure
 */
TaskHandle_t zigbee_motion_init(EventGroupHandle_t wake_events, EventBits_t ready_bit);


/**
 * @brief Send occupancy report to Zigbee network.
 *
 * When joined, updates the Occupancy Sensing cluster on state change only.
 * When not yet joined, stores the latest requested state and flushes after join.
 *
 * @param occupied true if motion detected, false otherwise
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t zigbee_motion_send_occupancy_report(bool occupied);

/**
 * @brief Publish occupancy cluster value even when it matches the last report (heartbeat).
 */
esp_err_t zigbee_motion_publish_occupancy_refresh(bool occupied);


/**
 * @brief True after join (steering success or reboot on network).
 */
bool zigbee_motion_is_joined(void);



/**
 * @brief True while a requested occupancy value is still pending (queued before join, or must retry after a failed apply).
 */
bool zigbee_motion_occupancy_intent_pending(void);

/**
 * @brief Check if Zigbee task has finished.
 *
 * @return true if Zigbee task is finished, false otherwise
 */
bool zigbee_motion_is_finished(void);

#ifdef __cplusplus
}
#endif
