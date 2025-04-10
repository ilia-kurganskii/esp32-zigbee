/*
 * SPDX-FileCopyrightText: 2021-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: LicenseRef-Included
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form, except as embedded into a Espressif Systems
 *    integrated circuit in a product or a software update for such product,
 *    must reproduce the above copyright notice, this list of conditions and
 *    the following disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 *    may be used to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * 4. Any software provided in binary form under this license must not be reverse
 *    engineered, decompiled, modified and/or disassembled.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "esp_log.h"
#include "led_strip.h"
#include "light_driver.h"

static led_strip_handle_t s_led_strip;
static uint8_t s_red = 255, s_green = 255, s_blue = 255;
static float s_level = 1;

#include "nvs_flash.h"

static const char *TAG = "LIGHT_DRIVER";

// Forward declarations
static void light_driver_refresh(void);
static void load_initial_values_from_storage(void);
static void save_current_values_to_storage(void);

static void load_initial_values_from_storage(void)
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("app_light", NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Error opening NVS handle: %s", esp_err_to_name(err));
        return;
    }

    // Load RGB values
    err = nvs_get_u8(nvs_handle, "red", &s_red);
    if (err != ESP_OK) {
        ESP_LOGD(TAG, "No saved red value found");
        s_red = 255;
    }

    err = nvs_get_u8(nvs_handle, "green", &s_green); 
    if (err != ESP_OK) {
        ESP_LOGD(TAG, "No saved green value found");
        s_green = 255;
    }

    err = nvs_get_u8(nvs_handle, "blue", &s_blue);
    if (err != ESP_OK) {
        ESP_LOGD(TAG, "No saved blue value found");
        s_blue = 255;
    }

    // Load level value
    uint8_t level;
    err = nvs_get_u8(nvs_handle, "level", &level);
    if (err != ESP_OK) {
        ESP_LOGD(TAG, "No saved level value found");
        s_level = 1.0f;
    } else {
        s_level = level / 255.0f;
    }

    ESP_LOGI(TAG, "Loaded init values from NVS - RGB: (%u,%u,%u), Level: %.2f", 
             s_red, s_green, s_blue, s_level);

    nvs_close(nvs_handle);
}


static void save_current_values_to_storage(void)
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("app_light", NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Error opening NVS handle: %s", esp_err_to_name(err));
        return;
    }

    // Save RGB values
    err = nvs_set_u8(nvs_handle, "red", s_red);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Error saving red value: %s", esp_err_to_name(err));
    }

    err = nvs_set_u8(nvs_handle, "green", s_green);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Error saving green value: %s", esp_err_to_name(err));
    }

    err = nvs_set_u8(nvs_handle, "blue", s_blue);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Error saving blue value: %s", esp_err_to_name(err));
    }

    // Save level value
    uint8_t level = (uint8_t)(s_level * 255);
    err = nvs_set_u8(nvs_handle, "level", level);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Error saving level value: %s", esp_err_to_name(err));
    }

    err = nvs_commit(nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Error committing values to NVS: %s", esp_err_to_name(err));
    }

    ESP_LOGI(TAG, "Successfully saved light values to NVS");

    nvs_close(nvs_handle);
}


void light_driver_set_power(bool power)
{
    s_level = power;
    save_current_values_to_storage();
    light_driver_refresh();
}

void light_driver_set_rgb(uint8_t red, uint8_t green, uint8_t blue)
{
    s_red = red;
    s_green = green;
    s_blue = blue;
    save_current_values_to_storage();
    light_driver_refresh();
}

void light_driver_set_level(uint8_t level)
{
    s_level = level / 255.0;
    save_current_values_to_storage();
    light_driver_refresh();
}

static void light_driver_refresh(void)
{
    // Apply current RGB and level values to all pixels
    for (int i = 0; i < CONFIG_EXAMPLE_STRIP_LED_NUMBER; i++) {
        ESP_ERROR_CHECK(led_strip_set_pixel(s_led_strip, i, 
            (uint8_t)(s_red * s_level),
            (uint8_t)(s_green * s_level), 
            (uint8_t)(s_blue * s_level)));
    }
    ESP_ERROR_CHECK(led_strip_refresh(s_led_strip));
}


void light_driver_init()
{
    led_strip_config_t led_strip_conf = {
        .max_leds = CONFIG_EXAMPLE_STRIP_LED_NUMBER,
        .strip_gpio_num = CONFIG_EXAMPLE_STRIP_LED_GPIO,
    };
    led_strip_rmt_config_t rmt_conf = {
        .resolution_hz = 10 * 1000 * 1000, // 10MHz
    };
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&led_strip_conf, &rmt_conf, &s_led_strip));
    load_initial_values_from_storage();
    light_driver_refresh();
}


