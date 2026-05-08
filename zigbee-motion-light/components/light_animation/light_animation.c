/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 *
 * Light Animation Component
 *
 * This component monitors the motion driver state and runs LED animations
 * while motion is detected.
 */

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "light_animation.h"
#include "light_driver.h"
#include "motion_driver.h"

static const char *TAG = "LIGHT_ANIMATION";

/* Motion-strip animation uses LEDs 1..N-1; LED 0 is reserved for Zigbee status. */
#define STRIP_LED_FIRST_INDEX        1

static bool s_animation_finished = false;
static TaskHandle_t s_animation_task_handle = NULL;

static void led_active_animation(void)
{
    uint8_t r = 255;
    uint8_t g = 180;
    uint8_t b = 0;

    for (int led = STRIP_LED_FIRST_INDEX; led < CONFIG_EXAMPLE_STRIP_LED_NUMBER; led++) {
        light_driver_set_pixel(led, r, g, b);
        vTaskDelay(pdMS_TO_TICKS(50));
        light_driver_set_pixel(led, 0, 0, 0);
    }
    for (int led = CONFIG_EXAMPLE_STRIP_LED_NUMBER - 2; led > STRIP_LED_FIRST_INDEX; led--) {
        light_driver_set_pixel(led, r, g, b);
        vTaskDelay(pdMS_TO_TICKS(50));
        light_driver_set_pixel(led, 0, 0, 0);
    }

    light_driver_set_pixel(STRIP_LED_FIRST_INDEX, r, g, b);
}

static void led_phase_out_animation(void)
{
    uint8_t r = 255;
    uint8_t g = 180;
    uint8_t b = 0;

    for (int led = STRIP_LED_FIRST_INDEX; led < CONFIG_EXAMPLE_STRIP_LED_NUMBER; led++) {
        light_driver_set_pixel(led, r, g, b);
        vTaskDelay(pdMS_TO_TICKS(100));
        light_driver_set_pixel(led, 0, 0, 0);
    }
}

static void animation_task(void *pvParameters)
{
    (void)pvParameters;

    ESP_LOGI(TAG, "Animation task started");

    bool motion_detected = motion_driver_get_state();
    ESP_LOGI(TAG, "Initial motion state: %s", motion_detected ? "DETECTED" : "CLEARED");

    /* If motion is already false, finish immediately */
    if (!motion_detected) {
        ESP_LOGI(TAG, "Motion already cleared, finishing animation task");
        s_animation_finished = true;
        ESP_LOGI(TAG, "Animation task finished (motion never detected)");
        vTaskDelete(NULL);
        return;
    }

    do {
        led_active_animation();

        motion_detected = motion_driver_get_state();
        if (motion_detected) {
            ESP_LOGI(TAG, "Motion still detected, looping animation");
        } else {
            ESP_LOGI(TAG, "No motion detected, starting phase out");
            led_phase_out_animation();
            break;
        }
    } while (motion_detected);

    s_animation_finished = true;
    ESP_LOGI(TAG, "Animation task finished");
    vTaskDelete(NULL);
}

TaskHandle_t light_animation_init(void)
{
    ESP_LOGI(TAG, "Initializing light animation component");

    s_animation_finished = false;
    s_animation_task_handle = NULL;

    BaseType_t ret = xTaskCreate(animation_task, "light_anim", 4096, NULL, 5, &s_animation_task_handle);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create animation task");
        return NULL;
    }

    ESP_LOGI(TAG, "Light animation component initialized");
    return s_animation_task_handle;
}

bool light_animation_is_finished(void)
{
    return s_animation_finished;
}

void light_animation_deinit(void)
{
    if (s_animation_task_handle) {
        ESP_LOGI(TAG, "Stopping animation task");
        vTaskDelete(s_animation_task_handle);
        s_animation_task_handle = NULL;
    }
    s_animation_finished = false;
}
