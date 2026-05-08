/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#pragma once

#include <stdbool.h>
#include "freertos/FreeRTOS.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize and start the light animation component.
 *
 * This function creates a task that monitors the motion driver state
 * and runs LED animations while motion is detected.
 *
 * @return Task handle on success, NULL on failure
 */
TaskHandle_t light_animation_init(void);

/**
 * @brief Check if the animation task has finished.
 *
 * @return true if animation task is finished, false otherwise
 */
bool light_animation_is_finished(void);

/**
 * @brief Stop and deinitialize the light animation component.
 */
void light_animation_deinit(void);

#ifdef __cplusplus
}
#endif
