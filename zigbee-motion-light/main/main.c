/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 *
 * Zigbee HA Motion Light Example
 *
 * This example code is in the Public Domain (or CC0 licensed, at your option.)
 *
 * Unless required by applicable law or agreed to in writing, this
 * software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
 * CONDITIONS OF ANY KIND, either express or implied.
 */

#include "esp_zb_motion_light.h"
#include "esp_check.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ha/esp_zigbee_ha_standard.h"
#include "motion_driver.h"
#include "light_driver.h"

static const char *TAG = "ESP_ZB_MOTION_LIGHT";

static void motion_detected_callback(void)
{
    ESP_LOGI(TAG, "Motion detected via callback");
}

void bdb_start_top_level_commissioning_cb(uint8_t mode_mask)
{
    ESP_RETURN_ON_FALSE(esp_zb_bdb_start_top_level_commissioning(mode_mask) == ESP_OK, , TAG, "Failed to start Zigbee commissioning");
}

void restore_last_data(void)
{
    // Restore LED state from NVS if needed
    ESP_LOGI(TAG, "Restoring last saved data");
}

void esp_zb_app_signal_handler(esp_zb_app_signal_t *signal_struct)
{
    uint32_t *p_sg_p = signal_struct->p_app_signal;
    esp_err_t err_status = signal_struct->esp_err_status;
    esp_zb_app_signal_type_t sig_type = *p_sg_p;
    switch (sig_type) {
    case ESP_ZB_ZDO_SIGNAL_SKIP_STARTUP:
        ESP_LOGI(TAG, "Initialize Zigbee stack");
        esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_INITIALIZATION);
        break;
    case ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START:
    case ESP_ZB_BDB_SIGNAL_DEVICE_REBOOT:
        if (err_status == ESP_OK) {
            ESP_LOGI(TAG, "Device started up in%s factory-reset mode", esp_zb_bdb_is_factory_new() ? "" : " non");
            if (esp_zb_bdb_is_factory_new()) {
                ESP_LOGI(TAG, "Start network steering");
                esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_STEERING);
            } else {
                ESP_LOGI(TAG, "Device rebooted");
            }
        }
        break;
    case ESP_ZB_BDB_SIGNAL_FORMATION:
    case ESP_ZB_BDB_SIGNAL_STEERING:
        if (err_status == ESP_OK) {
            ESP_LOGI(TAG, "Network formed/joined successfully");
        } else {
            ESP_LOGE(TAG, "Network formation/join failed");
        }
        break;
    default:
        ESP_LOGI(TAG, "Unhandled signal: %d", sig_type);
        break;
    }
}

static esp_zb_cluster_list_t *custom_clusters_create(void)
{
    esp_zb_cluster_list_t *cluster_list = esp_zb_zcl_cluster_list_create();
    
    /* Add Basic Cluster (mandatory) */
    esp_zb_attribute_list_t *basic_cluster = esp_zb_basic_cluster_create(NULL);
    esp_zb_basic_cluster_add_attr(basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID, ESP_MANUFACTURER_NAME);
    esp_zb_basic_cluster_add_attr(basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID, ESP_MODEL_IDENTIFIER);
    esp_zb_cluster_list_add_basic_cluster(cluster_list, basic_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    
    /* Add Identify Cluster (mandatory) */
    esp_zb_cluster_list_add_identify_cluster(cluster_list, esp_zb_identify_cluster_create(NULL), ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    
    /* Add Occupancy (Motion) Sensor Cluster */
    esp_zb_attribute_list_t *occupancy_cluster = esp_zb_occupancy_sensing_cluster_create(NULL);
    esp_zb_cluster_list_add_occupancy_sensing_cluster(cluster_list, occupancy_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    
    /* Add On/Off Cluster for LED control */
    esp_zb_attribute_list_t *onoff_cluster = esp_zb_on_off_cluster_create(NULL);
    esp_zb_cluster_list_add_on_off_cluster(cluster_list, onoff_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    
    return cluster_list;
}

void esp_zb_motion_light_init(void)
{
    /* Initialize motion driver */
    motion_driver_init();
    motion_driver_set_callback(motion_detected_callback);
    
    /* Initialize light driver */
    light_driver_init();
    
    /* Create endpoint list */
    esp_zb_ep_list_t *ep_list = esp_zb_ep_list_create();
    esp_zb_endpoint_config_t endpoint_config = {
        .endpoint = HA_ESP_MOTION_LIGHT_ENDPOINT,
        .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID,
        .app_device_id = ESP_ZB_HA_SIMPLE_SENSOR_DEVICE_ID,
        .app_device_version = 0
    };
    
    /* Add endpoint with clusters */
    esp_zb_ep_list_add_ep(ep_list, custom_clusters_create(), endpoint_config);
    
    /* Register device */
    esp_zb_device_register(ep_list);
}

void esp_zb_motion_light_start_commissioning(void)
{
    ESP_LOGI(TAG, "Starting Zigbee commissioning");
    esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_STEERING);
}

void app_main(void)
{
    ESP_LOGI(TAG, "Starting Zigbee Motion Light");
    
    /* Initialize NVS */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    /* Initialize Zigbee */
    esp_zb_init(NULL);
    
    /* Initialize motion light */
    esp_zb_motion_light_init();
    
    /* Restore last data */
    restore_last_data();
    
    /* Start Zigbee */
    ESP_ERROR_CHECK(esp_zb_start(false));
    
    ESP_LOGI(TAG, "Zigbee Motion Light started");
    
    /* Main loop - monitor motion and handle Zigbee */
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        
        /* Check for motion detection */
        if (motion_driver_was_motion_detected()) {
            ESP_LOGI(TAG, "Motion detected - LED ON");
            light_driver_set_power(true);
            
            // Keep LED on for 10 seconds
            vTaskDelay(pdMS_TO_TICKS(10000));
            ESP_LOGI(TAG, "LED OFF");
            light_driver_set_power(false);
        }
        
        /* Process Zigbee events */
        esp_zb_stack_main_loop_iteration();
    }
}
