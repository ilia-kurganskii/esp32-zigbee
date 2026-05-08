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
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
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

/* Deep sleep periodic timer (occupancy heartbeat + drift toward periodic time resync). */
#define DEEP_SLEEP_TIMER_INTERVAL_SEC   (3 * 60)

/* Time sync re-sync interval after accumulated sleep duration (µs tally on timer wakes). */
#define TIME_RESYNC_INTERVAL_SEC     (6 * 3600)

#define GPIO_WAKE_NEED_ZB_MARGIN_US  (5000000LL)
#define TIMER_WAKE_ZB_MARGIN_US      (1500000LL)

/* Absolute supervisor cap when occupancy intent pending (µs). */
#define ZB_HARD_CAP_EXTRA_PENDING_US   (60 * 1000000LL)

#define WAKE_EG_ANIM_DONE_BIT          BIT0

/* RTC memory — cumulative deep-sleep timer duration since last Zigbee coordinator time apply */
static RTC_DATA_ATTR int64_t s_accumulated_sleep_us = 0;

/* Motion tracking */
static int64_t s_last_motion_time_us = 0;

static EventGroupHandle_t s_wake_event_group;

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

    xEventGroupSetBits(s_wake_event_group, WAKE_EG_ANIM_DONE_BIT);
    vTaskDelete(NULL);
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
        if (wakeup_reason == ESP_SLEEP_WAKEUP_TIMER) {
            s_accumulated_sleep_us += (int64_t)DEEP_SLEEP_TIMER_INTERVAL_SEC * 1000000LL;
        }
        if (s_accumulated_sleep_us > (int64_t)TIME_RESYNC_INTERVAL_SEC * 1000000LL) {
            ESP_LOGI(TAG, "Periodic time re-sync needed");
            need_time_sync = true;
        }
    }

    zigbee_motion_set_coordinator_time_read_enabled(need_time_sync);
    ESP_ERROR_CHECK(zigbee_motion_init());

    const int64_t wakeup_start_us = esp_timer_get_time();
    const int64_t loop_start_us = wakeup_start_us;

    const bool gpio_wake = (wakeup_reason == ESP_SLEEP_WAKEUP_GPIO);
    const bool timer_wake = (wakeup_reason == ESP_SLEEP_WAKEUP_TIMER);
    const bool poll_motion_edges = gpio_wake || timer_wake;

    bool motion_prev = false;
    if (gpio_wake) {
        motion_prev = motion_driver_get_state();
        zigbee_motion_send_occupancy_report(motion_prev);
    } else if (timer_wake) {
        motion_prev = motion_driver_get_state();
        ESP_ERROR_CHECK(zigbee_motion_publish_occupancy_refresh(motion_prev));
        ESP_LOGI(TAG, "Timer wakeup: occupancy refresh published");
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
        vTaskDelay(pdMS_TO_TICKS(200));
    }

    s_wake_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(s_wake_event_group ? ESP_OK : ESP_ERR_NO_MEM);

    if (should_animate) {
        BaseType_t ok = xTaskCreate(motion_strip_anim_task, "strip_anim", 4096, NULL, 3, NULL);
        ESP_ERROR_CHECK(ok == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
    } else {
        xEventGroupSetBits(s_wake_event_group, WAKE_EG_ANIM_DONE_BIT);
    }

    int64_t wait_deadline_us = loop_start_us;
    bool run_baseline_wait = false;

    if (need_time_sync) {
        int64_t time_since_motion = s_last_motion_time_us > 0
                                        ? (esp_timer_get_time() - s_last_motion_time_us)
                                        : 0;
        int64_t remaining_wait_us = 30000000LL - time_since_motion;
        if (remaining_wait_us > 0) {
            wait_deadline_us = loop_start_us + remaining_wait_us;
            run_baseline_wait = true;
        }
    } else {
        wait_deadline_us = loop_start_us + (gpio_wake ? GPIO_WAKE_NEED_ZB_MARGIN_US : TIMER_WAKE_ZB_MARGIN_US);
        run_baseline_wait = true;
    }

    bool zb_baseline_done = !run_baseline_wait;
    bool pending_cap_warn_logged = false;

    for (;;) {
        bool anim_done = (xEventGroupGetBits(s_wake_event_group) & WAKE_EG_ANIM_DONE_BIT) != 0;

        if (zigbee_motion_occupancy_intent_pending()) {
            wait_deadline_us = i64_max(wait_deadline_us, wakeup_start_us + ZB_HARD_CAP_EXTRA_PENDING_US);
        }

        int64_t now_us = esp_timer_get_time();
        bool within_baseline_deadline = run_baseline_wait && now_us < wait_deadline_us;

        if (!zb_baseline_done) {
            bool exit_baseline = false;

            if (need_time_sync) {
                if (zigbee_motion_is_time_synced()) {
                    ESP_LOGI(TAG, "Zigbee time sync completed successfully");
                    s_accumulated_sleep_us = 0;
                    exit_baseline = true;
                } else if (zigbee_motion_is_sync_failed()) {
                    ESP_LOGW(TAG, "Zigbee sync failed");
                    exit_baseline = true;
                } else if (!within_baseline_deadline) {
                    ESP_LOGW(TAG, "Zigbee sync timeout");
                    exit_baseline = true;
                }
            } else {
                /*
                 * time_schedule already synced: we still restart Zigbee and the stack sends a
                 * coordinator Time read every wake ("Device rebooted"). Do not mirror the legacy
                 * join+JOIN_GATE shortcut here — exiting too early sleeps before READ_ATTR RESP and
                 * looks like Zigbee sends nothing visible in logs / HA misses updates until next wake.
                 */
                if (!within_baseline_deadline) {
                    exit_baseline = true;
                }
            }

            if (exit_baseline) {
                zb_baseline_done = true;
            }
        }

        bool pending_now = zigbee_motion_occupancy_intent_pending();
        bool cap_elapsed = (now_us - wakeup_start_us) >= ZB_HARD_CAP_EXTRA_PENDING_US;

        if (zb_baseline_done && pending_now && cap_elapsed && !pending_cap_warn_logged) {
            ESP_LOGW(TAG, "Sleeping with pending occupancy; will retry next wake");
            pending_cap_warn_logged = true;
        }

        bool zigbee_track_complete = zb_baseline_done &&
                                     (!pending_now || pending_cap_warn_logged);

        if (anim_done && zigbee_track_complete) {
            break;
        }

        if (poll_motion_edges) {
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

    ESP_LOGI(TAG, "Wakeup: %s, synced: %d", wakeup_str, time_schedule_is_time_synced());

    enter_deep_sleep();
}
