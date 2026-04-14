/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "light_driver.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "led_strip.h"
#include "esp_timer.h"

static const char *TAG = "LIGHT_DRIVER";

static bool current_power_state = false;
static uint8_t current_red = 0;
static uint8_t current_green = 0;
static uint8_t current_blue = 0;
static uint8_t current_level = 255;

// static led_strip_t *g_led_strip = NULL;  // Not used - GPIO control instead

void light_driver_init(void)
{
    ESP_LOGI(TAG, "Initializing light driver");
    
    // Configure LED GPIO as output for basic GPIO control
    gpio_config_t led_io_conf = {
        .pin_bit_mask = (1ULL << LIGHT_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&led_io_conf));
    
    // Set initial LED state to OFF
    ESP_ERROR_CHECK(gpio_set_level(LIGHT_GPIO, LED_ON_LOW ? 1 : 0));
    current_power_state = false;
    
    ESP_LOGI(TAG, "Light driver initialized - GPIO %d", LIGHT_GPIO);
}

void light_driver_set_power(bool power)
{
    current_power_state = power;
    ESP_ERROR_CHECK(gpio_set_level(LIGHT_GPIO, power == LED_ON_HIGH ? 1 : 0));
    ESP_LOGD(TAG, "Light power set to %s", power ? "ON" : "OFF");
}

bool light_driver_get_power(void)
{
    return current_power_state;
}

void light_driver_set_rgb(uint8_t red, uint8_t green, uint8_t blue)
{
    current_red = red;
    current_green = green;
    current_blue = blue;
    
    // For simple GPIO LED, use brightness based on RGB values
    uint8_t brightness = (red + green + blue) / 3;
    if (current_power_state && brightness > 0) {
        ESP_ERROR_CHECK(gpio_set_level(LIGHT_GPIO, LED_ON_HIGH ? 1 : 0));
    } else {
        ESP_ERROR_CHECK(gpio_set_level(LIGHT_GPIO, LED_ON_LOW ? 1 : 0));
    }
    
    ESP_LOGD(TAG, "Light RGB set to R:%d G:%d B:%d", red, green, blue);
}

void light_driver_set_level(uint8_t level)
{
    current_level = level;
    
    // Apply level to current RGB values
    uint8_t adjusted_red = (current_red * level) / 255;
    uint8_t adjusted_green = (current_green * level) / 255;
    uint8_t adjusted_blue = (current_blue * level) / 255;
    
    light_driver_set_rgb(adjusted_red, adjusted_green, adjusted_blue);
    
    ESP_LOGD(TAG, "Light level set to %d", level);
}

bool light_driver_toggle(void)
{
    bool new_state = !current_power_state;
    light_driver_set_power(new_state);
    return new_state;
}
