/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "link_status_led.h"
#include "light_driver.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/portmacro.h"

static const char *TAG = "LINK_STATUS_LED";

#define ORANGE_PULSE_US  280000

typedef enum {
    LS_BASE_OFF = 0,
    LS_BASE_BLUE,
    LS_BASE_GREEN,
    LS_BASE_PURPLE,
} ls_base_t;

static ls_base_t s_base = LS_BASE_OFF;
static bool s_orange_overlay;
static esp_timer_handle_t s_orange_timer;
static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;

static void redraw(void)
{
    bool orange;
    ls_base_t base;
    portENTER_CRITICAL(&s_mux);
    orange = s_orange_overlay;
    base = s_base;
    portEXIT_CRITICAL(&s_mux);

    if (orange) {
        light_driver_set_pixel(0, 255, 165, 0);
        return;
    }

    switch (base) {
    case LS_BASE_OFF:
        light_driver_set_pixel(0, 0, 0, 0);
        break;
    case LS_BASE_BLUE:
        light_driver_set_pixel(0, 0, 0, 255);
        break;
    case LS_BASE_GREEN:
        light_driver_set_pixel(0, 0, 255, 0);
        break;
    case LS_BASE_PURPLE:
        light_driver_set_pixel(0, 128, 0, 128);
        break;
    default:
        break;
    }
}

static void orange_timer_cb(void *arg)
{
    portENTER_CRITICAL(&s_mux);
    s_orange_overlay = false;
    portEXIT_CRITICAL(&s_mux);
    redraw();
}

void link_status_led_init(void)
{
    if (s_orange_timer == NULL) {
        const esp_timer_create_args_t targs = {
            .callback = &orange_timer_cb,
            .name = "ls_orange",
        };
        ESP_ERROR_CHECK(esp_timer_create(&targs, &s_orange_timer));
    }
    link_status_led_off();
}

void link_status_led_off(void)
{
    if (s_orange_timer) {
        esp_timer_stop(s_orange_timer);
    }
    portENTER_CRITICAL(&s_mux);
    s_orange_overlay = false;
    s_base = LS_BASE_OFF;
    portEXIT_CRITICAL(&s_mux);
    redraw();
}

void link_status_led_set_steering(void)
{
    portENTER_CRITICAL(&s_mux);
    s_base = LS_BASE_BLUE;
    portEXIT_CRITICAL(&s_mux);
    redraw();
}

void link_status_led_set_joined(void)
{
    portENTER_CRITICAL(&s_mux);
    s_base = LS_BASE_GREEN;
    portEXIT_CRITICAL(&s_mux);
    redraw();
}

void link_status_led_set_time_synced_from_coordinator(void)
{
    portENTER_CRITICAL(&s_mux);
    s_base = LS_BASE_PURPLE;
    portEXIT_CRITICAL(&s_mux);
    redraw();
}

void link_status_led_notify_occupancy_issued(void)
{
    if (s_orange_timer == NULL) {
        ESP_LOGW(TAG, "Timer not initialized");
        return;
    }
    esp_timer_stop(s_orange_timer);
    portENTER_CRITICAL(&s_mux);
    s_orange_overlay = true;
    portEXIT_CRITICAL(&s_mux);
    redraw();
    esp_err_t err = esp_timer_start_once(s_orange_timer, ORANGE_PULSE_US);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Orange timer start: %s", esp_err_to_name(err));
        portENTER_CRITICAL(&s_mux);
        s_orange_overlay = false;
        portEXIT_CRITICAL(&s_mux);
        redraw();
    }
}
