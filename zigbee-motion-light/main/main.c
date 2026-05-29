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
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "nvs_flash.h"
#include "light_driver.h"
#include "motion_driver.h"
#include "zigbee_motion.h"
#include "link_status_led.h"
#include "motion_status_led.h"
#include "light_animation.h"

static const char *TAG = "MOTION_LIGHT";

/* Deep sleep periodic timer (occupancy heartbeat). */
#define DEEP_SLEEP_TIMER_INTERVAL_SEC   (3 * 60)

#define WAKE_ANIM_DONE_BIT  BIT0
#define WAKE_ZB_READY_BIT   BIT1

/* ───────────────────── Deep Sleep ───────────────────── */

static void enter_deep_sleep(void)
{
    /* Animation uses the same RMT strip; stop it before touching LEDs for sleep. */
    light_animation_deinit();
    vTaskDelay(pdMS_TO_TICKS(20));

    link_status_led_off();
    motion_status_led_off();
    light_driver_set_power(LIGHT_DEFAULT_OFF);

    /* If PIR is still HIGH, GPIO wake fires immediately and looks like no sleep. */
    motion_driver_wait_until_clear(2500);
    motion_status_led_set(motion_driver_get_state());

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
    motion_status_led_init();
    motion_driver_init();

    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();

    const char *wakeup_str = (wakeup_reason == ESP_SLEEP_WAKEUP_GPIO) ? "GPIO" :
                             (wakeup_reason == ESP_SLEEP_WAKEUP_TIMER) ? "TIMER" :
                             (wakeup_reason == ESP_SLEEP_WAKEUP_UNDEFINED) ? "UNDEFINED" : "OTHER";
    ESP_LOGI(TAG, "Wakeup: %s", wakeup_str);

    EventGroupHandle_t wake_events = xEventGroupCreate();
    if (wake_events == NULL) {
        ESP_LOGE(TAG, "Failed to create wake event group");
        return;
    }

    TaskHandle_t zigbee_task_handle = zigbee_motion_init(wake_events, WAKE_ZB_READY_BIT);
    if (zigbee_task_handle == NULL) {
        ESP_LOGE(TAG, "Failed to initialize Zigbee");
        return;
    }

    TaskHandle_t animate_task_handle = light_animation_init(wake_events, WAKE_ANIM_DONE_BIT);
    if (animate_task_handle == NULL) {
        ESP_LOGE(TAG, "Failed to initialize animation");
        return;
    }

    ESP_LOGI(TAG, "Supervisor waiting for Zigbee report and animation");
    xEventGroupWaitBits(wake_events, WAKE_ZB_READY_BIT | WAKE_ANIM_DONE_BIT,
                        pdFALSE, pdTRUE, portMAX_DELAY);

    ESP_LOGI(TAG, "Wake cycle complete, entering deep sleep");
    enter_deep_sleep();
}
