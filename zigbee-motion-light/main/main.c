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

static const char *TAG = "LED_TEST";

void app_main(void)
{
    ESP_LOGI(TAG, "Starting LED Test");
    
    /* Initialize light driver */
    light_driver_init();
    
    /* LED blinking loop */
    while (1) {
        light_driver_set_power(true);
        vTaskDelay(pdMS_TO_TICKS(500));
        light_driver_set_power(false);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
