/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#ifndef ESP_ZB_MOTION_LIGHT_H
#define ESP_ZB_MOTION_LIGHT_H

#include "esp_zigbee_core.h"

/* Zigbee configuration */
#define INSTALLCODE_POLICY_ENABLE       false                                /* enable the install code policy for security */
#define ED_AGING_TIMEOUT                ESP_ZB_ED_AGING_TIMEOUT_64MIN        /* aging timeout of device */
#define ED_KEEP_ALIVE                   3000                                 /* 3000 millisecond */
#define HA_ESP_MOTION_LIGHT_ENDPOINT    10                                   /* esp motion light device endpoint */
#define ESP_ZB_PRIMARY_CHANNEL_MASK     11 /* Zigbee primary channel mask use in the example */

/* Basic manufacturer information */
#define ESP_MANUFACTURER_NAME "\x09""ESPRESSIF"      /* Customized manufacturer name */
#define ESP_MODEL_IDENTIFIER "\x0D""Motion-Light"     /* Customized model identifier */


/* Motion sensor cluster attributes */
#define ESP_ZB_ZCL_ATTR_OCCUPANCY_ID                      0x0000
#define ESP_ZB_ZCL_ATTR_OCCUPANCY_SENSOR_TYPE_ID         0x0001

/* On/Off cluster attributes */
#define ESP_ZB_ZCL_ATTR_ON_OFF_ID                        0x0000

/**
 * @brief Initialize Zigbee motion light.
 *
 * This function initializes the Zigbee stack and creates the necessary clusters
 * for motion sensor and LED control.
 */
void esp_zb_motion_light_init(void);

/**
 * @brief Start Zigbee commissioning.
 *
 * This function starts the Zigbee network commissioning process.
 */
void esp_zb_motion_light_start_commissioning(void);

/**
 * @brief Handle Zigbee application signals.
 *
 * @param signal_struct Pointer to the signal structure
 */
void esp_zb_app_signal_handler(esp_zb_app_signal_t *signal_struct);

/**
 * @brief Restore last saved data.
 *
 * This function restores the last saved device state from non-volatile storage.
 */
void restore_last_data(void);

/**
 * @brief Start top level commissioning callback.
 *
 * @param mode_mask Commissioning mode mask
 */
void bdb_start_top_level_commissioning_cb(uint8_t mode_mask);

#endif // ESP_ZB_MOTION_LIGHT_H
