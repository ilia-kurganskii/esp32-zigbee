/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Motion light endpoint */
#define MOTION_LIGHT_ENDPOINT   10

/**
 * @brief Initialize Zigbee motion light component.
 *
 * This function initializes the Zigbee stack, creates the necessary clusters
 * (Basic, Identify, On/Off, Time, Occupancy Sensing), and starts the Zigbee task.
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t zigbee_motion_init(void);

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
 * @brief Check if Zigbee time sync is complete.
 *
 * @return true if time has been synced, false otherwise
 */
bool zigbee_motion_is_time_synced(void);

/**
 * @brief True after join (steering success or reboot on network).
 */
bool zigbee_motion_is_joined(void);

/**
 * @brief Check if Zigbee sync has failed.
 *
 * @return true if sync failed, false otherwise
 */
bool zigbee_motion_is_sync_failed(void);

/**
 * @brief Wait for Zigbee time sync with timeout.
 *
 * Waits for time sync to complete or timeout.
 *
 * @param timeout_ms Timeout in milliseconds
 * @return ESP_OK if sync completed, ESP_ERR_TIMEOUT if timeout, error otherwise
 */
esp_err_t zigbee_motion_wait_for_sync(uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif
