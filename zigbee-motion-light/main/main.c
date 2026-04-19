/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 *
 * LED Test Example
 *
 * This example code is in the Public Domain (or CC0 licensed, at your option.)
 *
 * Unless required by applicable law or agreed to in writing, this
 * software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
 * CONDITIONS OF ANY KIND, either express or implied.
 */

#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_task_wdt.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "light_driver.h"
#include "motion_driver.h"

static const char *TAG = "MOTION_LIGHT";

void app_main(void)
{
    ESP_LOGI(TAG, "Starting AM312 motion detection with deep sleep");

    /* Initialize LED */
    light_driver_init();
    light_driver_set_power(LIGHT_DEFAULT_ON);

    /* Check wake-up reason */
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();

    if (wakeup_reason == ESP_SLEEP_WAKEUP_GPIO) {
        ESP_LOGI(TAG, "Woke up from deep sleep due to motion detection on GPIO %d", MOTION_SENSOR_GPIO);
        light_driver_set_rgb(0, 255, 0);  // Green - motion detected
    } else if (wakeup_reason == ESP_SLEEP_WAKEUP_TIMER) {
        ESP_LOGI(TAG, "Woke up from timer");
        light_driver_set_rgb(255, 0, 0);  // Red - timer wakeup
    } else {
        ESP_LOGI(TAG, "Not a deep sleep wakeup (normal boot or other reason: %d)", wakeup_reason);
        light_driver_set_rgb(255, 0, 0);  // Red - normal boot
    }

    /* Initialize motion driver */
    motion_driver_init();

    ESP_LOGI(TAG, "Motion driver initialized on GPIO %d", MOTION_SENSOR_GPIO);

    /* Configure deep sleep wake-up from motion sensor */
    motion_driver_configure_deep_sleep_wakeup();

    /* Configure timer wakeup for 10 seconds */
    ESP_ERROR_CHECK(esp_sleep_enable_timer_wakeup(10000000));

    const char *wakeup_str = (wakeup_reason == ESP_SLEEP_WAKEUP_GPIO) ? "GPIO" :
                             (wakeup_reason == ESP_SLEEP_WAKEUP_TIMER) ? "TIMER" :
                             (wakeup_reason == ESP_SLEEP_WAKEUP_UNDEFINED) ? "UNDEFINED" : "OTHER";
    ESP_LOGI(TAG, "Wakeup: %s, GPIO %d level: %d", wakeup_str, MOTION_SENSOR_GPIO, gpio_get_level(MOTION_SENSOR_GPIO));

    /* LED animation: bounce one LED at a time, 5 rounds */
    uint8_t r = (wakeup_reason == ESP_SLEEP_WAKEUP_GPIO) ? 0 : 255;
    uint8_t g = (wakeup_reason == ESP_SLEEP_WAKEUP_GPIO) ? 255 : 0;
    uint8_t b = 0;
    int num_leds = CONFIG_EXAMPLE_STRIP_LED_NUMBER;
    int pattern_len = (num_leds > 1) ? (2 * num_leds - 2) : 1;

    for (int round = 0; round < 20; round++) {
        for (int p = 0; p < pattern_len; p++) {
            int led = (p < num_leds) ? p : (2 * (num_leds - 1) - p);
            light_driver_set_rgb(0, 0, 0);
            light_driver_set_pixel(led, r, g, b);
            vTaskDelay(100 / portTICK_PERIOD_MS);
        }
    }

    /* Keep last LED active */
    light_driver_set_rgb(0, 0, 0);
    light_driver_set_pixel(0, r, g, b);

    /* Wait 10 seconds before deep sleep only for non-GPIO wakeup */
    if (wakeup_reason != ESP_SLEEP_WAKEUP_GPIO) {
        vTaskDelay(10000 / portTICK_PERIOD_MS);
    }

    /* Turn LED off before deep sleep */
    light_driver_set_power(LIGHT_DEFAULT_OFF);

    /* Enter deep sleep */
    esp_deep_sleep_start();
}
