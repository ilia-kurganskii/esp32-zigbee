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
    ESP_LOGI(TAG, "Starting LED Color Test");
    
    /* Initialize light driver */
    light_driver_init();
    
    /* LED color cycling loop */
    uint8_t colors[][3] = {
        {255, 0, 0},    // Red
        {0, 255, 0},    // Green
        {0, 0, 255},    // Blue
        {255, 255, 0},  // Yellow
        {255, 0, 255},  // Magenta
        {0, 255, 255},  // Cyan
        {255, 255, 255} // White
    };
    int num_colors = sizeof(colors) / sizeof(colors[0]);
    int color_index = 0;
    
    while (1) {
        light_driver_set_rgb(colors[color_index][0], colors[color_index][1], colors[color_index][2]);
        light_driver_set_level(2); // 1% brightness (2 out of 255)
        vTaskDelay(pdMS_TO_TICKS(1000));
        
        color_index = (color_index + 1) % num_colors;
    }
}
