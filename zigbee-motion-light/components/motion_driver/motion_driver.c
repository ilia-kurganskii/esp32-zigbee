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
    
    // Configure motion sensor GPIO as input with interrupt
    gpio_config_t motion_io_conf = {
        .pin_bit_mask = (1ULL << MOTION_SENSOR_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_POSEDGE,
    };
    ESP_ERROR_CHECK(gpio_config(&motion_io_conf));
    
    // Install GPIO interrupt service
    ESP_ERROR_CHECK(gpio_install_isr_service(0));
    ESP_ERROR_CHECK(gpio_isr_handler_add(MOTION_SENSOR_GPIO, motion_isr_handler, NULL));
    
    ESP_LOGI(TAG, "Motion driver initialized - GPIO %d", MOTION_SENSOR_GPIO);
}

bool motion_driver_get_state(void)
{
    return gpio_get_level(MOTION_SENSOR_GPIO) == (MOTION_DETECTED_HIGH ? 1 : 0);
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
    
    /* Configure the motion sensor GPIO as wake-up source */
    gpio_config_t wakeup_io_conf = {
        .pin_bit_mask = (1ULL << MOTION_SENSOR_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_POSEDGE,
    };
    ESP_ERROR_CHECK(gpio_config(&wakeup_io_conf));
    
    /* Enable wake-up from GPIO */
    ESP_ERROR_CHECK(esp_sleep_enable_gpio_wakeup());
    
    /* Set the wake-up mask for the motion sensor GPIO */
    ESP_ERROR_CHECK(gpio_wakeup_enable(MOTION_SENSOR_GPIO, GPIO_INTR_POSEDGE));
    
    ESP_LOGI(TAG, "Deep sleep wake-up configured on GPIO %d", MOTION_SENSOR_GPIO);
}
