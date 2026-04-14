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

/* LED configuration */
#define LIGHT_GPIO                 5       // LED control pin
#define LED_ON_HIGH               true    // LED on when pin is HIGH
#define LED_ON_LOW                false   // LED on when pin is LOW

/* Light intensity levels */
#define LIGHT_DEFAULT_ON  1
#define LIGHT_DEFAULT_OFF 0

/**
 * @brief Initialize light driver.
 *
 * This function initializes the GPIO for LED output.
 */
void light_driver_init(void);

/**
 * @brief Set light power (on/off).
 *
 * @param power  The light power to be set
 */
void light_driver_set_power(bool power);

/**
 * @brief Get current light power state.
 *
 * @return true if light is on, false if light is off
 */
bool light_driver_get_power(void);

/**
 * @brief Set light color (RGB).
 *
 * @param red   Red intensity (0-255)
 * @param green Green intensity (0-255)
 * @param blue  Blue intensity (0-255)
 */
void light_driver_set_rgb(uint8_t red, uint8_t green, uint8_t blue);

/**
 * @brief Set light level (0-255).
 *
 * @param level Light level (0-255)
 */
void light_driver_set_level(uint8_t level);

/**
 * @brief Toggle light state.
 *
 * @return New light state after toggle
 */
bool light_driver_toggle(void);

#ifdef __cplusplus
}
#endif
