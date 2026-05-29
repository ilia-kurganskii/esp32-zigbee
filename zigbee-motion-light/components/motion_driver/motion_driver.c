/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "motion_driver.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_sleep.h"

static const char *TAG = "MOTION_DRIVER";

static volatile bool motion_detected_flag = false;
static motion_callback_t motion_callback = NULL;

static void IRAM_ATTR motion_isr_handler(void *arg)
{
    motion_detected_flag = true;
    if (motion_callback) {
        motion_callback();
    }
}

void motion_driver_init(void)
{
    ESP_LOGI(TAG, "Initializing motion driver");

    // Configure motion sensor GPIO as input
    gpio_config_t motion_io_conf = {
        .pin_bit_mask = (1ULL << MOTION_SENSOR_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&motion_io_conf));

    ESP_LOGI(TAG, "Motion driver initialized - GPIO %d", MOTION_SENSOR_GPIO);
}

bool motion_driver_get_state(void)
{
    return gpio_get_level(MOTION_SENSOR_GPIO) == (MOTION_DETECTED_HIGH ? 1 : 0);
}

bool motion_driver_get_state_debounced(void)
{
    int active_samples = 0;

    for (int i = 0; i < 5; i++) {
        if (motion_driver_get_state()) {
            active_samples++;
        }
        if (i + 1 < 5) {
            vTaskDelay(pdMS_TO_TICKS(40));
        }
    }

    return active_samples >= 3;
}

void motion_driver_wait_settled(void)
{
    vTaskDelay(pdMS_TO_TICKS(MOTION_PIR_SETTLE_MS));
}

bool motion_driver_was_motion_detected(void)
{
    bool detected = motion_detected_flag;
    motion_detected_flag = false; // Clear the flag
    return detected;
}

void motion_driver_set_callback(motion_callback_t callback)
{
    motion_callback = callback;
}

void motion_driver_configure_deep_sleep_wakeup(void)
{
    ESP_LOGI(TAG, "Configuring GPIO %d for deep sleep wake-up", MOTION_SENSOR_GPIO);

    /* Enable deep sleep GPIO wakeup - wake on HIGH level (motion detected) */
    ESP_ERROR_CHECK(esp_deep_sleep_enable_gpio_wakeup(1ULL << MOTION_SENSOR_GPIO, ESP_GPIO_WAKEUP_GPIO_HIGH));

    ESP_LOGI(TAG, "Deep sleep wake-up configured on GPIO %d", MOTION_SENSOR_GPIO);
}

void motion_driver_wait_until_clear(uint32_t timeout_ms)
{
    if (!motion_driver_get_state()) {
        return;
    }

    ESP_LOGI(TAG, "Waiting for PIR to clear (timeout %lu ms)", (unsigned long)timeout_ms);
    uint32_t elapsed = 0;
    while (motion_driver_get_state() && elapsed < timeout_ms) {
        vTaskDelay(pdMS_TO_TICKS(50));
        elapsed += 50;
    }

    if (motion_driver_get_state()) {
        ESP_LOGW(TAG, "PIR still active after %lu ms", (unsigned long)timeout_ms);
    }
}
