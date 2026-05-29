/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 *
 * Light Animation Component
 *
 * Pixel 0: Zigbee link status. Pixel 1: motion status. Pixel 2+: main animation.
 */

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "light_animation.h"
#include "light_driver.h"
#include "motion_driver.h"
#include "motion_status_led.h"
#include "zigbee_motion.h"

static const char *TAG = "LIGHT_ANIMATION";

#define ANIM_LED_FIRST_INDEX         2

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

static void update_motion_status_led(void)
{
    motion_status_led_set(motion_driver_get_state());
}

static bool animation_time_remaining(int64_t end_us)
{
    return esp_timer_get_time() < end_us;
}

static bool led_active_animation(void)
{
    if (CONFIG_EXAMPLE_STRIP_LED_NUMBER <= ANIM_LED_FIRST_INDEX) {
        return false;
    }

    uint8_t r = 255;
    uint8_t g = 180;
    uint8_t b = 0;

    for (int led = ANIM_LED_FIRST_INDEX; led < CONFIG_EXAMPLE_STRIP_LED_NUMBER; led++) {
        light_driver_set_pixel(led, r, g, b);
        vTaskDelay(pdMS_TO_TICKS(50));
        light_driver_set_pixel(led, 0, 0, 0);
    }
    for (int led = CONFIG_EXAMPLE_STRIP_LED_NUMBER - 2; led >= ANIM_LED_FIRST_INDEX; led--) {
        light_driver_set_pixel(led, r, g, b);
        vTaskDelay(pdMS_TO_TICKS(50));
        light_driver_set_pixel(led, 0, 0, 0);
    }

    light_driver_set_pixel(ANIM_LED_FIRST_INDEX, r, g, b);
    return true;
}

static void led_phase_out_animation(void)
{
    if (CONFIG_EXAMPLE_STRIP_LED_NUMBER <= ANIM_LED_FIRST_INDEX) {
        return;
    }

    uint8_t r = 255;
    uint8_t g = 180;
    uint8_t b = 0;

    for (int led = ANIM_LED_FIRST_INDEX; led < CONFIG_EXAMPLE_STRIP_LED_NUMBER; led++) {
        light_driver_set_pixel(led, r, g, b);
        vTaskDelay(pdMS_TO_TICKS(100));
        light_driver_set_pixel(led, 0, 0, 0);
    }
}

static void animation_task(void *pvParameters)
{
    (void)pvParameters;

    ESP_LOGI(TAG, "Animation task started");

    motion_driver_wait_settled();
    update_motion_status_led();

    const bool motion = motion_driver_get_state_debounced();
    const bool light_enabled = zigbee_motion_light_enabled();
    const int64_t end_us = esp_timer_get_time() + (int64_t)LIGHT_ANIMATION_MAX_MS * 1000;

    if (!motion || !light_enabled) {
        ESP_LOGI(TAG, "Main animation skipped (motion=%d, light=%d)", motion, light_enabled);
    } else {
        ESP_LOGI(TAG, "Motion detected, running strip animation for %d ms", LIGHT_ANIMATION_MAX_MS);

        while (animation_time_remaining(end_us) && zigbee_motion_light_enabled()) {
            update_motion_status_led();
            led_active_animation();
        }

        for (int led = ANIM_LED_FIRST_INDEX; led < CONFIG_EXAMPLE_STRIP_LED_NUMBER; led++) {
            light_driver_set_pixel(led, 0, 0, 0);
        }

        if (!motion_driver_get_state()) {
            ESP_LOGI(TAG, "Motion cleared, phase out");
            led_phase_out_animation();
        }
    }

    animation_signal_done();

    /* Keep motion indicator in sync until deep sleep (PIR may clear after occupancy). */
    while (true) {
        update_motion_status_led();
        vTaskDelay(pdMS_TO_TICKS(200));
    }
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
