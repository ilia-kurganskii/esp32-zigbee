/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "motion_status_led.h"
#include "light_driver.h"

#define MOTION_STATUS_LED_INDEX  1

void motion_status_led_init(void)
{
    motion_status_led_set(false);
}

void motion_status_led_off(void)
{
    light_driver_set_pixel(MOTION_STATUS_LED_INDEX, 0, 0, 0);
}

void motion_status_led_set(bool motion_detected)
{
    if (motion_detected) {
        light_driver_set_pixel(MOTION_STATUS_LED_INDEX, 255, 0, 0);
    } else {
        light_driver_set_pixel(MOTION_STATUS_LED_INDEX, 0, 255, 0);
    }
}
