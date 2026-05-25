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

/* Zigbee RF channel (11–26); primary scan mask is (1 << channel) */
#define ESP_ZB_PRIMARY_CHANNEL  25

/**
 * @brief Initialize Zigbee motion light component.
 *
 * This function initializes the Zigbee stack, creates the necessary clusters
 * (Basic, Identify, On/Off, Occupancy Sensing), and starts the Zigbee task.
 *
 * @param wake_events Event group to signal when the Zigbee track is ready for sleep
 * @param ready_bit   Bit to set when joined and occupancy report is queued
 * @return Task handle on success, NULL on failure
 */
TaskHandle_t zigbee_motion_init(EventGroupHandle_t wake_events, EventBits_t ready_bit);


/**
 * @brief Send occupancy report to Zigbee network.
 *
 * When joined, updates the Occupancy Sensing cluster and queues a report to the
 * coordinator (fire-and-forget on the Zigbee stack thread).
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
 * @brief True when the coordinator On/Off state allows the motion LED strip.
 *
 * Persisted in NVS so deep-sleep wake cycles respect OFF before Zigbee rejoins.
 */
bool zigbee_motion_light_enabled(void);

/**
 * @brief True while occupancy is queued before join.
 */
bool zigbee_motion_occupancy_intent_pending(void);

#ifdef __cplusplus
}
#endif
