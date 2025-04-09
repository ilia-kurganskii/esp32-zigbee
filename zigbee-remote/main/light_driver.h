/*
 * SPDX-FileCopyrightText: 2021-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: LicenseRef-Included
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form, except as embedded into a Espressif Systems
 *    integrated circuit in a product or a software update for such product,
 *    must reproduce the above copyright notice, this list of conditions and
 *    the following disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 *    may be used to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * 4. Any software provided in binary form under this license must not be reverse
 *    engineered, decompiled, modified and/or disassembled.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* light intensity level */
#define LIGHT_DEFAULT_ON  1
#define LIGHT_DEFAULT_OFF 0

/* LED strip configuration */
#define CONFIG_EXAMPLE_STRIP_LED_GPIO   8
#define CONFIG_EXAMPLE_STRIP_LED_NUMBER 2

/* LED colors for different states */
#define LED_COLOR_INIT      0xFF, 0xFF, 0xFF  // White
#define LED_COLOR_SUCCESS   0x00, 0xFF, 0x00  // Green
#define LED_COLOR_ERROR     0xFF, 0x00, 0x00  // Red
#define LED_COLOR_STEERING  0x00, 0x00, 0xFF  // Blue
#define LED_COLOR_WARNING   0xFF, 0xA5, 0x00  // Orange
#define LED_COLOR_SLEEP     0x80, 0x00, 0xFF  // Purple for sleep


/**
 * @brief Initialize the LED driver
 * 
 * @param power Initial power state
 */
void light_driver_init(bool power);

/**
 * @brief Set LED power state
 * 
 * @param power true for ON, false for OFF
 */
void light_driver_set_power(bool power);

/**
 * @brief Set LED RGB color
 * 
 * @param red Red component (0-255)
 * @param green Green component (0-255)
 * @param blue Blue component (0-255)
 */
void light_driver_set_rgb(uint8_t red, uint8_t green, uint8_t blue);

/**
 * @brief Set LED brightness level
 * 
 * @param level Brightness level (0-255)
 */
void light_driver_set_level(uint8_t level);

/**
 * @brief Blink LED with specified color and pattern
 * 
 * @param red Red component (0-255)
 * @param green Green component (0-255)
 * @param blue Blue component (0-255)
 * @param times Number of blinks
 * @param on_time_ms Time LED stays on in milliseconds
 * @param off_time_ms Time LED stays off in milliseconds
 */
void light_driver_blink(uint8_t red, uint8_t green, uint8_t blue, 
                       uint8_t times, uint32_t on_time_ms, uint32_t off_time_ms);

/**
 * @brief Quick helper function to blink LED in white color
 * 
 * @param times Number of blinks
 * @param delay_ms Time for both on and off states
 */
void light_driver_blink_white(uint8_t times, uint32_t delay_ms);

#ifdef __cplusplus
} // extern "C"
#endif
