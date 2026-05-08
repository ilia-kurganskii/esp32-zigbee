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

/** Owns RGB pixel 0 only (Zigbee / occupancy status palette B). */
void link_status_led_init(void);

void link_status_led_off(void);

void link_status_led_set_steering(void);

void link_status_led_set_joined(void);

/** Call only after coordinator time read applied successfully. */
void link_status_led_set_time_synced_from_coordinator(void);

void link_status_led_notify_occupancy_issued(void);

#ifdef __cplusplus
}
#endif
