/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 *
 * Motion Light with Zigbee Time Schedule
 *
 * This example code is in the Public Domain (or CC0 licensed, at your option.)
 *
 * Unless required by applicable law or agreed to in writing, this
 * software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
 * CONDITIONS OF ANY KIND, either express or implied.
 */

#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_check.h"
#include "esp_task_wdt.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "light_driver.h"
#include "motion_driver.h"
#include "time_schedule.h"
#include "zigbee_motion.h"

static const char *TAG = "MOTION_LIGHT";

/* Time sync re-sync interval: every 6 hours of deep sleep wakeups */
#define TIME_RESYNC_INTERVAL_SEC  (6 * 3600)

/* RTC memory - track total sleep time for periodic resync */
static RTC_DATA_ATTR int64_t s_accumulated_sleep_us = 0;
static RTC_DATA_ATTR int64_t s_last_sync_time_us = 0;

/* Motion tracking */
static int64_t s_last_motion_time_us = 0;

/* ───────────────────── LED Animation ───────────────────── */

static void led_active_animation(void)
{
    uint8_t r = 255;
    uint8_t g = 180;
    uint8_t b = 0;

    /* Simple single-LED bounce pattern (1 round) */
    for (int led = 0; led < CONFIG_EXAMPLE_STRIP_LED_NUMBER; led++) {
        light_driver_set_pixel(led, r, g, b);
        vTaskDelay(50 / portTICK_PERIOD_MS);
        light_driver_set_pixel(led, 0, 0, 0);
    }
    for (int led = CONFIG_EXAMPLE_STRIP_LED_NUMBER - 2; led > 0; led--) {
        light_driver_set_pixel(led, r, g, b);
        vTaskDelay(50 / portTICK_PERIOD_MS);
        light_driver_set_pixel(led, 0, 0, 0);
    }

    /* Keep LED 0 active */
    light_driver_set_pixel(0, r, g, b);
}

static void led_phase_out_animation(void)
{
    uint8_t r = 255;
    uint8_t g = 180;
    uint8_t b = 0;

    /* Fade out pattern with slower timing (100ms per step) */
    for (int led = 0; led < CONFIG_EXAMPLE_STRIP_LED_NUMBER; led++) {
        light_driver_set_pixel(led, r, g, b);
        vTaskDelay(100 / portTICK_PERIOD_MS);
        light_driver_set_pixel(led, 0, 0, 0);
    }

    /* Turn off all LEDs */
    light_driver_set_rgb(0, 0, 0);
}

/* ───────────────────── Deep Sleep ───────────────────── */

static void enter_deep_sleep(void)
{
    /* Turn LED off before deep sleep */
    light_driver_set_power(LIGHT_DEFAULT_OFF);

    /* Configure wake-up sources */
    motion_driver_configure_deep_sleep_wakeup();
    ESP_ERROR_CHECK(esp_sleep_enable_timer_wakeup(10000000)); /* 10s timer */

    ESP_LOGI(TAG, "Entering deep sleep...");
    esp_deep_sleep_start();
}

/* ───────────────────── Main ───────────────────── */

void app_main(void)
{
    ESP_LOGI(TAG, "Starting motion light with time schedule");

    /* Initialize NVS (required for Zigbee and time schedule) */
    ESP_ERROR_CHECK(nvs_flash_init());

    /* Initialize LED */
    light_driver_init();
    light_driver_set_rgb(0, 0, 0);  /* Clear all LEDs */
    light_driver_set_pixel(0, 255, 255, 255);  /* Set only LED 0 white */

    /* Initialize time schedule */
    ESP_ERROR_CHECK(time_schedule_init());

    /* Check wake-up reason */
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();

    const char *wakeup_str = (wakeup_reason == ESP_SLEEP_WAKEUP_GPIO) ? "GPIO" :
                             (wakeup_reason == ESP_SLEEP_WAKEUP_TIMER) ? "TIMER" :
                             (wakeup_reason == ESP_SLEEP_WAKEUP_UNDEFINED) ? "UNDEFINED" : "OTHER";
    ESP_LOGI(TAG, "Wakeup: %s, synced: %d", wakeup_str, time_schedule_is_time_synced());

    /* Initialize motion driver */
    motion_driver_init();

    /* Check if we need Zigbee time sync */
    bool need_time_sync = !time_schedule_is_time_synced();

    /* Also re-sync periodically */
    if (!need_time_sync) {
        s_accumulated_sleep_us += 10000000; /* approximate: 10s per timer wakeup */
        int64_t since_last_sync = s_accumulated_sleep_us - s_last_sync_time_us;
        if (since_last_sync > (int64_t)TIME_RESYNC_INTERVAL_SEC * 1000000LL) {
            ESP_LOGI(TAG, "Periodic time re-sync needed");
            need_time_sync = true;
        }
    }

    /* ── Start Zigbee if needed ── */
    if (need_time_sync || wakeup_reason == ESP_SLEEP_WAKEUP_GPIO) {
        ESP_LOGI(TAG, "Starting Zigbee...");
        light_driver_set_pixel(0, 0, 0, 255); /* Blue = initializing */
        vTaskDelay(200 / portTICK_PERIOD_MS); /* Brief blue flash */

        ESP_ERROR_CHECK(zigbee_motion_init());

        ESP_LOGI(TAG, "Zigbee started");
    }

    /* ── Check if animation should run ── */
    bool should_animate = false;
    if (wakeup_reason == ESP_SLEEP_WAKEUP_GPIO) {
        if (!time_schedule_is_time_synced() || time_schedule_is_active()) {
            should_animate = true;
            ESP_LOGI(TAG, "Motion detected -> animation will run");
        } else {
            ESP_LOGI(TAG, "Motion detected outside schedule -> no animation");
        }
    } else if (wakeup_reason == ESP_SLEEP_WAKEUP_TIMER) {
        ESP_LOGI(TAG, "Timer wakeup, no animation");
    } else {
        /* First boot - show red briefly */
        ESP_LOGI(TAG, "First boot");
        light_driver_set_pixel(0, 255, 0, 0);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }

    /* ── Animation loop ── */
    if (should_animate) {
        /* Send initial occupancy report if motion detected */
        bool motion_detected = motion_driver_get_state();
        if (motion_detected) {
            zigbee_motion_send_occupancy_report(true);
        }

        /* Animation loop: run animation, check motion at end, loop if still motion */
        do {
            led_active_animation();

            /* Check motion at end of animation */
            motion_detected = motion_driver_get_state();
            if (motion_detected) {
                s_last_motion_time_us = esp_timer_get_time();
                ESP_LOGI(TAG, "Motion still detected, looping animation");
                /* Motion still detected, loop again */
            } else {
                /* No motion, send false and phase out */
                ESP_LOGI(TAG, "No motion detected, starting phase out");
                zigbee_motion_send_occupancy_report(false);
                led_phase_out_animation();
                break;
            }
        } while (motion_detected);
    }

    /* ── Wait for Zigbee sync (30 seconds from last motion) ── */
    if (need_time_sync) {
        int64_t time_since_motion = s_last_motion_time_us > 0 ? 
                                     (esp_timer_get_time() - s_last_motion_time_us) : 0;
        int64_t remaining_wait = 30000000 - time_since_motion; /* 30 seconds */

        if (remaining_wait > 0) {
            ESP_LOGI(TAG, "Waiting up to %lld ms for Zigbee sync", remaining_wait / 1000);

            esp_err_t ret = zigbee_motion_wait_for_sync(remaining_wait / 1000);
            if (ret == ESP_OK) {
                ESP_LOGI(TAG, "Zigbee time sync completed successfully");
                s_last_sync_time_us = esp_timer_get_time();
                s_accumulated_sleep_us = 0;
            } else if (ret == ESP_FAIL) {
                ESP_LOGW(TAG, "Zigbee sync failed");
            } else {
                ESP_LOGW(TAG, "Zigbee sync timeout");
            }
        }
    }

    ESP_LOGI(TAG, "Wakeup: %s, synced: %d", wakeup_str, time_schedule_is_time_synced());

    /* Enter deep sleep */
    enter_deep_sleep();
}
