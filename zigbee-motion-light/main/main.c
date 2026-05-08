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
#include "freertos/task.h"
#include "nvs_flash.h"
#include "light_driver.h"
#include "motion_driver.h"
#include "time_schedule.h"
#include "zigbee_motion.h"
#include "link_status_led.h"

static const char *TAG = "MOTION_LIGHT";

/* Motion-strip animation uses LEDs 1..N-1; LED 0 is reserved for Zigbee status. */
#define STRIP_LED_FIRST_INDEX        1

/* Time sync re-sync interval: every 6 hours of deep sleep wakeups */
#define TIME_RESYNC_INTERVAL_SEC     (6 * 3600)

/* After join window when time sync is not awaited (µs). */
#define ZB_JOIN_GATE_US              (800000LL)

#define GPIO_WAKE_NEED_ZB_MARGIN_US  (5000000LL)
#define TIMER_WAKE_ZB_MARGIN_US      (2000000LL)

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

/* ───────────────────── Deep Sleep ───────────────────── */

static void enter_deep_sleep(void)
{
    link_status_led_off();
    light_driver_set_power(LIGHT_DEFAULT_OFF);

    motion_driver_configure_deep_sleep_wakeup();
    ESP_ERROR_CHECK(esp_sleep_enable_timer_wakeup(10000000)); /* 10s timer */

    ESP_LOGI(TAG, "Entering deep sleep...");
    esp_deep_sleep_start();
}

/* ───────────────────── Main ───────────────────── */

void app_main(void)
{
    ESP_LOGI(TAG, "Starting motion light with time schedule");

    ESP_ERROR_CHECK(nvs_flash_init());

    light_driver_init();
    light_driver_set_rgb(0, 0, 0);
    link_status_led_init();

    ESP_ERROR_CHECK(time_schedule_init());

    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();

    const char *wakeup_str = (wakeup_reason == ESP_SLEEP_WAKEUP_GPIO) ? "GPIO" :
                             (wakeup_reason == ESP_SLEEP_WAKEUP_TIMER) ? "TIMER" :
                             (wakeup_reason == ESP_SLEEP_WAKEUP_UNDEFINED) ? "UNDEFINED" : "OTHER";
    ESP_LOGI(TAG, "Wakeup: %s, synced: %d", wakeup_str, time_schedule_is_time_synced());

    motion_driver_init();

    bool need_time_sync = !time_schedule_is_time_synced();

    if (!need_time_sync) {
        s_accumulated_sleep_us += 10000000; /* approximate: 10s per timer wakeup */
        int64_t since_last_sync = s_accumulated_sleep_us - s_last_sync_time_us;
        if (since_last_sync > (int64_t)TIME_RESYNC_INTERVAL_SEC * 1000000LL) {
            ESP_LOGI(TAG, "Periodic time re-sync needed");
            need_time_sync = true;
        }
    }

    const bool zigbee_started = need_time_sync || (wakeup_reason == ESP_SLEEP_WAKEUP_GPIO);

    if (zigbee_started) {
        link_status_led_set_steering();
        ESP_ERROR_CHECK(zigbee_motion_init());
    }

    const bool gpio_wake = (wakeup_reason == ESP_SLEEP_WAKEUP_GPIO);
    bool motion_prev = false;
    if (gpio_wake && zigbee_started) {
        motion_prev = motion_driver_get_state();
        zigbee_motion_send_occupancy_report(motion_prev);
    }

    bool should_animate = false;
    if (gpio_wake) {
        if (!time_schedule_is_time_synced() || time_schedule_is_active()) {
            should_animate = true;
            ESP_LOGI(TAG, "Motion detected -> animation will run");
        } else {
            ESP_LOGI(TAG, "Motion detected outside schedule -> no animation");
        }
    } else if (wakeup_reason == ESP_SLEEP_WAKEUP_TIMER) {
        ESP_LOGI(TAG, "Timer wakeup, no animation");
    } else {
        ESP_LOGI(TAG, "First boot");
        link_status_led_set_steering();
        vTaskDelay(pdMS_TO_TICKS(200));
    }

    if (should_animate) {
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
    }

    int64_t loop_start_us = esp_timer_get_time();
    int64_t wait_deadline_us = loop_start_us;
    bool run_wait_loop = false;

    if (need_time_sync) {
        int64_t time_since_motion = s_last_motion_time_us > 0
                                        ? (esp_timer_get_time() - s_last_motion_time_us)
                                        : 0;
        int64_t remaining_wait_us = 30000000LL - time_since_motion;
        if (remaining_wait_us > 0) {
            wait_deadline_us += remaining_wait_us;
            run_wait_loop = true;
        }
    } else if (zigbee_started) {
        wait_deadline_us += gpio_wake ? GPIO_WAKE_NEED_ZB_MARGIN_US : TIMER_WAKE_ZB_MARGIN_US;
        run_wait_loop = true;
    }

    if (run_wait_loop) {
        while (esp_timer_get_time() < wait_deadline_us) {
            bool exit_loop = false;

            if (need_time_sync) {
                if (zigbee_motion_is_time_synced()) {
                    ESP_LOGI(TAG, "Zigbee time sync completed successfully");
                    s_last_sync_time_us = esp_timer_get_time();
                    s_accumulated_sleep_us = 0;
                    exit_loop = true;
                } else if (zigbee_motion_is_sync_failed()) {
                    ESP_LOGW(TAG, "Zigbee sync failed");
                    exit_loop = true;
                }
            } else if (zigbee_motion_is_joined() &&
                       (esp_timer_get_time() - loop_start_us) >= ZB_JOIN_GATE_US) {
                exit_loop = true;
            }

            if (exit_loop) {
                break;
            }

            if (gpio_wake && zigbee_started) {
                bool m = motion_driver_get_state();
                if (m != motion_prev) {
                    motion_prev = m;
                    zigbee_motion_send_occupancy_report(m);
                    if (m) {
                        s_last_motion_time_us = esp_timer_get_time();
                    }
                }
            }

            vTaskDelay(pdMS_TO_TICKS(100));
        }

        if (need_time_sync && !zigbee_motion_is_time_synced() && !zigbee_motion_is_sync_failed()) {
            ESP_LOGW(TAG, "Zigbee sync timeout");
        }
    }

    ESP_LOGI(TAG, "Wakeup: %s, synced: %d", wakeup_str, time_schedule_is_time_synced());

    enter_deep_sleep();
}
