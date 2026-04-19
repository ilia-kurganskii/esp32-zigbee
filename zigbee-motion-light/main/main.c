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

    ESP_LOGI(TAG, "Entering deep sleep. Will wake on motion detection...");

    /* Debug loop: run 10 iterations without deep sleep */
    for (int i = 0; i < 10; i++) {
        const char *wakeup_str = (wakeup_reason == ESP_SLEEP_WAKEUP_GPIO) ? "GPIO" :
                                 (wakeup_reason == ESP_SLEEP_WAKEUP_TIMER) ? "TIMER" :
                                 (wakeup_reason == ESP_SLEEP_WAKEUP_UNDEFINED) ? "UNDEFINED" : "OTHER";
        ESP_LOGI(TAG, "Debug iteration %d - GPIO %d level: %d - Wakeup: %s", i, MOTION_SENSOR_GPIO, gpio_get_level(MOTION_SENSOR_GPIO), wakeup_str);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

    /* Turn LED off before deep sleep */
    light_driver_set_power(LIGHT_DEFAULT_OFF);

    /* Enter deep sleep */
    esp_deep_sleep_start();
}
