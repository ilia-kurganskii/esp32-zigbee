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
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static led_strip_handle_t s_led_strip;
static uint8_t s_red = 255, s_green = 255, s_blue = 255;
static float s_level = 1;

void light_driver_set_power(bool power)
{
    
    ESP_ERROR_CHECK(led_strip_set_pixel(s_led_strip, 0, s_red * power, s_green * power, s_blue * power));
    ESP_ERROR_CHECK(led_strip_refresh(s_led_strip));
}

void light_driver_set_rgb(uint8_t red, uint8_t green, uint8_t blue)
{
    s_red = red;
    s_green = green;
    s_blue = blue;
    for (int i = 0; i < CONFIG_EXAMPLE_STRIP_LED_NUMBER; i++) {
        ESP_ERROR_CHECK(led_strip_set_pixel(s_led_strip, i, (uint8_t)(s_red * s_level), 
                                          (uint8_t)(s_green * s_level), (uint8_t)(s_blue * s_level)));
    }
    ESP_ERROR_CHECK(led_strip_refresh(s_led_strip));
}

void light_driver_set_level(uint8_t level)
{
    s_level = level / 255.0;
    // for all leds
    for (int i = 0; i < CONFIG_EXAMPLE_STRIP_LED_NUMBER; i++) {
        ESP_ERROR_CHECK(led_strip_set_pixel(s_led_strip, i, (uint8_t)(s_red * s_level), 
                                          (uint8_t)(s_green * s_level), (uint8_t)(s_blue * s_level)));
    }
    ESP_ERROR_CHECK(led_strip_refresh(s_led_strip));
}

void light_driver_init(bool power)
{
    led_strip_config_t led_strip_conf = {
        .max_leds = CONFIG_EXAMPLE_STRIP_LED_NUMBER,
        .strip_gpio_num = CONFIG_EXAMPLE_STRIP_LED_GPIO,
    };
    led_strip_rmt_config_t rmt_conf = {
        .resolution_hz = 10 * 1000 * 1000, // 10MHz
    };
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&led_strip_conf, &rmt_conf, &s_led_strip));
    light_driver_set_power(power);
}

void light_driver_blink(uint8_t red, uint8_t green, uint8_t blue, 
                       uint8_t times, uint32_t on_time_ms, uint32_t off_time_ms)
{
    // Store current LED state
    uint8_t prev_red = s_red;
    uint8_t prev_green = s_green;
    uint8_t prev_blue = s_blue;
    
    // Set new color
    light_driver_set_rgb(red, green, blue);
    
    // Perform blinking
    for (int i = 0; i < times; i++) {
        light_driver_set_power(true);
        vTaskDelay(pdMS_TO_TICKS(on_time_ms));
        light_driver_set_power(false);
        if (i < times - 1) { // Don't delay after the last blink
            vTaskDelay(pdMS_TO_TICKS(off_time_ms));
        }
    }

    light_driver_set_power(0);


}

void light_driver_blink_white(uint8_t times, uint32_t delay_ms)
{
    light_driver_blink(LED_COLOR_INIT, times, delay_ms, delay_ms);
}
