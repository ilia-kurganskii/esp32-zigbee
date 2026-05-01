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

/* Default schedule: 22:00 - 06:00 (night hours) */
#define TIME_SCHEDULE_DEFAULT_START_HOUR    22
#define TIME_SCHEDULE_DEFAULT_START_MIN     0
#define TIME_SCHEDULE_DEFAULT_END_HOUR      6
#define TIME_SCHEDULE_DEFAULT_END_MIN       0

/* Timezone offset in seconds from UTC.
 * Change this to match your local timezone.
 * Examples:
 *   UTC+1 (CET):   3600
 *   UTC+2 (CEST):  7200
 *   UTC-5 (EST):  -18000
 *   UTC+3 (MSK):   10800
 */
#define TIME_SCHEDULE_DEFAULT_TZ_OFFSET     3600

/* Zigbee epoch: seconds between Unix epoch (1970-01-01) and Zigbee epoch (2000-01-01) */
#define ZIGBEE_EPOCH_OFFSET                 946684800UL

/**
 * @brief Initialize time schedule component.
 *
 * Loads schedule configuration from NVS.
 * Must be called after nvs_flash_init().
 */
esp_err_t time_schedule_init(void);

/**
 * @brief Check if system time has been synced via Zigbee.
 *
 * Uses RTC memory to persist through deep sleep.
 *
 * @return true if time has been synced at least once
 */
bool time_schedule_is_time_synced(void);

/**
 * @brief Set system time from Zigbee Time Cluster.
 *
 * Zigbee time is seconds since 2000-01-01T00:00:00 UTC.
 * Sets the system clock so gettimeofday() returns correct time
 * even after deep sleep.
 *
 * @param zigbee_utc_seconds  Zigbee UTC time (seconds since 2000-01-01)
 */
esp_err_t time_schedule_sync_time(uint32_t zigbee_utc_seconds);

/**
 * @brief Check if current time falls within the active schedule.
 *
 * - Returns true if schedule is disabled
 * - Returns true if time has not been synced yet (fail-open)
 * - Otherwise checks local time against configured active window
 *
 * @return true if LED should be active
 */
bool time_schedule_is_active(void);

/**
 * @brief Set active hours (local time, 24h format).
 *
 * If start > end, wraps around midnight (e.g., 22:00 - 06:00).
 * Saved to NVS for persistence across power cycles.
 */
esp_err_t time_schedule_set_hours(uint8_t start_hour, uint8_t start_min,
                                   uint8_t end_hour, uint8_t end_min);

/**
 * @brief Enable or disable the time schedule.
 *
 * When disabled, LED always activates on motion.
 */
esp_err_t time_schedule_set_enabled(bool enabled);

/**
 * @brief Get current local hour for debugging.
 *
 * @return current hour (0-23) or -1 if time is not synced
 */
int time_schedule_get_current_hour(void);

#ifdef __cplusplus
}
#endif
