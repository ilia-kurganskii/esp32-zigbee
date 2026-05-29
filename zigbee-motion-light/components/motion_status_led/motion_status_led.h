/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Owns RGB pixel 1 only (motion: green = clear, red = detected). */
void motion_status_led_init(void);

void motion_status_led_off(void);

void motion_status_led_set(bool motion_detected);

#ifdef __cplusplus
}
#endif
