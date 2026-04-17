/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Motion sensor configuration */
#define MOTION_SENSOR_GPIO         17       // Motion sensor input pin
#define MOTION_DETECTED_HIGH       true    // Motion detected when pin is HIGH
#define MOTION_DETECTED_LOW        false   // Motion detected when pin is LOW

/* AM312 PIR sensor specifications */
#define AM312_VOLTAGE_MIN          2.7     // Minimum operating voltage (V)
#define AM312_VOLTAGE_MAX          12.0    // Maximum operating voltage (V)
#define AM312_CURRENT_IDLE          0.1     // Idle current consumption (mA)
#define AM312_DELAY_TIME            2.0     // Output delay time (seconds)
#define AM312_BLOCKING_TIME         2.0     // Blocking time after trigger (seconds)
#define AM312_RANGE_MIN             3.0     // Minimum detection range (meters)
#define AM312_RANGE_MAX             5.0     // Maximum detection range (meters)
#define AM312_FOV                   100     // Field of view (degrees)

/**
 * @brief Initialize motion sensor driver.
 *
 * This function initializes the GPIO for motion sensor input.
 * Sets up interrupt for motion detection.
 */
void motion_driver_init(void);

/**
 * @brief Get current motion detection state.
 *
 * @return true if motion is detected, false otherwise
 */
bool motion_driver_get_state(void);

/**
 * @brief Check if motion was detected (clears the flag).
 *
 * This function checks if motion was detected since the last call
 * and clears the motion detected flag.
 *
 * @return true if motion was detected, false otherwise
 */
bool motion_driver_was_motion_detected(void);

/**
 * @brief Register motion detection callback.
 *
 * @param callback Function to call when motion is detected
 */
typedef void (*motion_callback_t)(void);
void motion_driver_set_callback(motion_callback_t callback);

/**
 * @brief Configure GPIO for deep sleep wake-up.
 *
 * This function configures the motion sensor GPIO as a wake-up source
 * for deep sleep mode. The device will wake up when motion is detected.
 */
void motion_driver_configure_deep_sleep_wakeup(void);

#ifdef __cplusplus
}
#endif
