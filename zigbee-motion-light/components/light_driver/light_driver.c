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

#include "esp_log.h"
#include "led_strip.h"
#include "light_driver.h"

static const char *TAG = "LIGHT_DRIVER";

static led_strip_handle_t s_led_strip;
static uint8_t s_red = 0, s_green = 0, s_blue = 0;
static float s_level = 1;

// Forward declarations
static void light_driver_refresh(void);

void light_driver_set_power(bool power)
{
    s_level = power;
    ESP_LOGI(TAG, "Power set to %s", power ? "ON" : "OFF");
    light_driver_refresh();
}

void light_driver_set_rgb(uint8_t red, uint8_t green, uint8_t blue)
{
    s_red = red;
    s_green = green;
    s_blue = blue;
    light_driver_refresh();
}

void light_driver_set_pixel(uint8_t index, uint8_t red, uint8_t green, uint8_t blue)
{
    if (index >= CONFIG_EXAMPLE_STRIP_LED_NUMBER) return;
    ESP_ERROR_CHECK(led_strip_set_pixel(s_led_strip, index, red, green, blue));
    ESP_ERROR_CHECK(led_strip_refresh(s_led_strip));
}

void light_driver_set_level(uint8_t level)
{
    s_level = level / 255.0;
    ESP_LOGI(TAG, "Level set to %u (%.2f)", level, s_level);
    light_driver_refresh();
}

static void light_driver_refresh(void)
{
    // Apply current RGB and level values to all pixels
    for (int i = 0; i < CONFIG_EXAMPLE_STRIP_LED_NUMBER; i++) {
        ESP_ERROR_CHECK(led_strip_set_pixel(s_led_strip, i, 
            (uint8_t)(s_red * s_level),
            (uint8_t)(s_green * s_level), 
            (uint8_t)(s_blue * s_level)));
    }
    ESP_ERROR_CHECK(led_strip_refresh(s_led_strip));
}


void light_driver_init()
{
    ESP_LOGI(TAG, "Initializing LED strip - GPIO: %d, LEDs: %d", CONFIG_EXAMPLE_STRIP_LED_GPIO, CONFIG_EXAMPLE_STRIP_LED_NUMBER);
    
    led_strip_config_t led_strip_conf = {
        .max_leds = CONFIG_EXAMPLE_STRIP_LED_NUMBER,
        .strip_gpio_num = CONFIG_EXAMPLE_STRIP_LED_GPIO,
    };
    led_strip_rmt_config_t rmt_conf = {
        .resolution_hz = 10 * 1000 * 1000, // 10MHz
    };
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&led_strip_conf, &rmt_conf, &s_led_strip));
    light_driver_refresh();
    
    ESP_LOGI(TAG, "LED strip initialized successfully");
}
