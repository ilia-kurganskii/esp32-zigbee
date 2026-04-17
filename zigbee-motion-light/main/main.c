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
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "light_driver.h"
#include "motion_driver.h"

static const char *TAG = "MOTION_LIGHT";

void app_main(void)
{
    ESP_LOGI(TAG, "Starting AM312 motion detection test");

    /* Initialize motion driver */
    motion_driver_init();

    ESP_LOGI(TAG, "Motion driver initialized - waiting for motion on GPIO %d", MOTION_SENSOR_GPIO);

    /* Motion detection loop */
    while (1) {
        if (motion_driver_was_motion_detected()) {
            ESP_LOGI(TAG, "Motion detected!");
        }
        vTaskDelay(pdMS_TO_TICKS(100)); // Check every 100ms
    }
}
