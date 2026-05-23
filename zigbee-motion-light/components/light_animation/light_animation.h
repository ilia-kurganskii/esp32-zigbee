/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize and start the light animation component.
 *
 * This function creates a task that monitors the motion driver state
 * and runs LED animations while motion is detected.
 *
 * @param wake_events Event group to signal on completion (may be NULL)
 * @param done_bit    Bit to set when the animation track is done
 * @return Task handle on success, NULL on failure
 */
TaskHandle_t light_animation_init(EventGroupHandle_t wake_events, EventBits_t done_bit);

/**
 * @brief Stop and deinitialize the light animation component.
 */
void light_animation_deinit(void);

#ifdef __cplusplus
}
#endif
