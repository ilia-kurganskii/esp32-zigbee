#pragma once
#ifndef ZIGBEE_HANDLER_H
#define ZIGBEE_HANDLER_H

#include "esp_zigbee_core.h"
#include "esp_zigbee_endpoint.h" 
#include "zcl/esp_zigbee_zcl_command.h"
#include "ha/esp_zigbee_ha_standard.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "gpio_control/gpio_control.h"
#include "ota_updater/ota_updater.h"

#include "logger/logger.h"
#include "metrics/metrics.h"

// External declarations for device info constants
extern const uint8_t manufacturer[];
extern const uint8_t model[];

// Function declarations
void zigbee_main_task(void *pvParameters);

#endif // ZIGBEE_HANDLER_H