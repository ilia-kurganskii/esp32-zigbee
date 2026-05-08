/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 *
 * Motion Light with Zigbee
 *
 * This example code is in the Public Domain (or CC0 licensed, at your option.)
 *
 * Unless required by applicable law or agreed to in writing, this
 * software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
 * CONDITIONS OF ANY KIND, either express or implied.
 */

#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "light_driver.h"
#include "motion_driver.h"
#include "zigbee_motion.h"
#include "link_status_led.h"

static const char *TAG = "MOTION_LIGHT";

/* Motion-strip animation uses LEDs 1..N-1; LED 0 is reserved for Zigbee status. */
#define STRIP_LED_FIRST_INDEX        1

/* Deep sleep periodic timer (occupancy heartbeat). */
#define DEEP_SLEEP_TIMER_INTERVAL_SEC   (3 * 60)

#define TIMER_WAKE_ZB_MARGIN_US      (1500000LL)

/* Absolute supervisor cap when occupancy intent pending (µs). */
#define ZB_HARD_CAP_EXTRA_PENDING_US   (60 * 1000000LL)

/* Motion tracking */
static int64_t s_last_motion_time_us = 0;

static bool s_anim_done = false;

static inline int64_t i64_max(int64_t a, int64_t b)
{
    return (a > b) ? a : b;
}

/* ───────────────────── LED Animation ───────────────────── */

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
    /* Do not call light_driver_set_rgb() — it overwrites pixel 0 (status LED). */
}

static void motion_strip_anim_task(void *pvParameters)
{
    (void)pvParameters;

    bool motion_detected = motion_driver_get_state();
    if (motion_detected) {
        zigbee_motion_send_occupancy_report(true);
    }

    do {
        led_active_animation();

        motion_detected = motion_driver_get_state();
        if (motion_detected) {
            s_last_motion_time_us = esp_timer_get_time();
            ESP_LOGI(TAG, "Motion still detected, looping animation");
        } else {
            ESP_LOGI(TAG, "No motion detected, starting phase out");
            zigbee_motion_send_occupancy_report(false);
            led_phase_out_animation();
            break;
        }
    } while (motion_detected);

    s_anim_done = true;
}

/* ───────────────────── Deep Sleep ───────────────────── */

static void enter_deep_sleep(void)
{
    link_status_led_off();
    light_driver_set_power(LIGHT_DEFAULT_OFF);

    motion_driver_configure_deep_sleep_wakeup();
    ESP_ERROR_CHECK(esp_sleep_enable_timer_wakeup(DEEP_SLEEP_TIMER_INTERVAL_SEC * 1000000ULL));

    ESP_LOGI(TAG, "Entering deep sleep...");
    esp_deep_sleep_start();
}

/* ───────────────────── Main ───────────────────── */

void app_main(void)
{
    ESP_LOGI(TAG, "Starting motion light with Zigbee");

    ESP_ERROR_CHECK(nvs_flash_init());

    light_driver_init();
    light_driver_set_rgb(0, 0, 0);
    link_status_led_init();
    motion_driver_init();

    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();

    ESP_LOGI(TAG, "Wakeup: %s", wakeup_str);

    BaseType_t zigbee_task_handle = zigbee_motion_init();
    ESP_ERROR_CHECK(zigbee_task_handle == pdPASS ? ESP_OK : ESP_FAIL);

    BaseType_t animate_task_handle = xTaskCreate(motion_strip_anim_task, "motion_strip_anim", 4096, NULL, 5, NULL, 0);
    ESP_ERROR_CHECK(animate_task_handle == pdPASS ? ESP_OK : ESP_FAIL);

    bool is_zigbee_task_finished = false;
    bool is_animate_task_finished = false;
    while (!is_zigbee_task_finished || !is_animate_task_finished) {
        vTaskDelay(pdMS_TO_TICKS(100));
        ESP_LOGI(TAG, "Waiting for tasks to finish...");
    }

    vTaskDelete(zigbee_task_handle);
    vTaskDelete(animate_task_handle);


    enter_deep_sleep();
}
