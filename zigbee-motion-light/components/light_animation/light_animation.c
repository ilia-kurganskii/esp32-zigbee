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
#include "freertos/event_groups.h"
#include "light_animation.h"
#include "light_driver.h"
#include "motion_driver.h"
#include "zigbee_motion.h"

static const char *TAG = "LIGHT_ANIMATION";

/* Motion-strip animation uses LEDs 1..N-1; LED 0 is reserved for Zigbee status. */
#define STRIP_LED_FIRST_INDEX        1

static TaskHandle_t s_animation_task_handle = NULL;
static EventGroupHandle_t s_wake_events = NULL;
static EventBits_t s_done_bit = 0;

static void animation_signal_done(void)
{
    if (s_wake_events != NULL) {
        xEventGroupSetBits(s_wake_events, s_done_bit);
        ESP_LOGI(TAG, "Animation track ready (bits 0x%lx)",
                 (unsigned long)xEventGroupGetBits(s_wake_events));
    } else {
        ESP_LOGE(TAG, "s_wake_events is NULL");
    }
}

/* Returns false if motion cleared during the sweep. */
static bool led_active_animation(void)
{
    uint8_t r = 255;
    uint8_t g = 180;
    uint8_t b = 0;

    for (int led = STRIP_LED_FIRST_INDEX; led < CONFIG_EXAMPLE_STRIP_LED_NUMBER; led++) {
        if (!motion_driver_get_state()) {
            return false;
        }
        light_driver_set_pixel(led, r, g, b);
        vTaskDelay(pdMS_TO_TICKS(50));
        light_driver_set_pixel(led, 0, 0, 0);
    }
    for (int led = CONFIG_EXAMPLE_STRIP_LED_NUMBER - 2; led > STRIP_LED_FIRST_INDEX; led--) {
        if (!motion_driver_get_state()) {
            return false;
        }
        light_driver_set_pixel(led, r, g, b);
        vTaskDelay(pdMS_TO_TICKS(50));
        light_driver_set_pixel(led, 0, 0, 0);
    }

    light_driver_set_pixel(STRIP_LED_FIRST_INDEX, r, g, b);
    return true;
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

static void animation_task_end(void)
{
    s_animation_task_handle = NULL;
    vTaskDelete(NULL);
}

static void animation_task(void *pvParameters)
{
    (void)pvParameters;

    ESP_LOGI(TAG, "Animation task started");

    /* PIR (AM312) needs a moment after GPIO wake before the level is stable. */
    vTaskDelay(pdMS_TO_TICKS(100));

    if (!motion_driver_get_state()) {
        ESP_LOGI(TAG, "No motion after settle, animation skipped");
        animation_signal_done();
        animation_task_end();
        return;
    }

    if (!zigbee_motion_light_enabled()) {
        ESP_LOGI(TAG, "Light disabled via On/Off, animation skipped");
        animation_signal_done();
        animation_task_end();
        return;
    }

    ESP_LOGI(TAG, "Motion detected, running strip animation");

    while (motion_driver_get_state() && zigbee_motion_light_enabled()) {
        if (!led_active_animation()) {
            break;
        }
    }

    /* Unblock sleep before cosmetic phase-out (strip may be cut off by deep sleep). */
    animation_signal_done();

    if (!motion_driver_get_state()) {
        ESP_LOGI(TAG, "Motion cleared, phase out");
        led_phase_out_animation();
    }

    ESP_LOGI(TAG, "Animation task finished");
    animation_task_end();
}

TaskHandle_t light_animation_init(EventGroupHandle_t wake_events, EventBits_t done_bit)
{
    ESP_LOGI(TAG, "Initializing light animation component");

    s_wake_events = wake_events;
    s_done_bit = done_bit;
    s_animation_task_handle = NULL;

    BaseType_t ret = xTaskCreate(animation_task, "light_anim", 4096, NULL, 3, &s_animation_task_handle);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create animation task");
        return NULL;
    }

    ESP_LOGI(TAG, "Light animation component initialized");
    return s_animation_task_handle;
}

void light_animation_deinit(void)
{
    TaskHandle_t task = s_animation_task_handle;
    s_animation_task_handle = NULL;

    if (task != NULL) {
        ESP_LOGI(TAG, "Stopping animation task");
        vTaskDelete(task);
    }

    s_wake_events = NULL;
    s_done_bit = 0;
}
