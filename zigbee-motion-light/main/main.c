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

static void bdb_start_top_level_commissioning_cb(uint8_t mode_mask)
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
    case ESP_ZB_BDB_SIGNAL_NETWORK_FORMED:
    case ESP_ZB_BDB_SIGNAL_NETWORK_JOINED:
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
    
    /* Add Occupancy (Motion) Sensor Cluster */
    esp_zb_attribute_list_t *occupancy_cluster = esp_zb_zcl_attr_list_create(ESP_ZB_ZCL_CLUSTER_ID_OCCUPANCY_SENSING);
    
    /* Add occupancy attributes */
    bool occupancy_value = motion_driver_get_state();
    esp_zb_attribute_t *occupancy_attr = esp_zb_zcl_attr_create(ESP_ZB_ZCL_ATTR_OCCUPANCY_ID, ESP_ZB_ZCL_ATTR_TYPE_BOOL, ESP_ZB_ZCL_ATTR_READ_ONLY, &occupancy_value);
    esp_zb_zcl_attr_list_add_attr(occupancy_cluster, occupancy_attr);
    
    uint8_t sensor_type = 0x06; // PIR sensor
    esp_zb_attribute_t *sensor_type_attr = esp_zb_zcl_attr_create(ESP_ZB_ZCL_ATTR_OCCUPANCY_SENSOR_TYPE_ID, ESP_ZB_ZCL_ATTR_TYPE_U8, ESP_ZB_ZCL_ATTR_READ_ONLY, &sensor_type);
    esp_zb_zcl_attr_list_add_attr(occupancy_cluster, sensor_type_attr);
    
    esp_zb_cluster_list_add_cluster(cluster_list, occupancy_cluster);
    
    /* Add On/Off Cluster for LED control */
    esp_zb_attribute_list_t *onoff_cluster = esp_zb_zcl_attr_list_create(ESP_ZB_ZCL_CLUSTER_ID_ON_OFF);
    
    bool onoff_value = light_driver_get_power();
    esp_zb_attribute_t *onoff_attr = esp_zb_zcl_attr_create(ESP_ZB_ZCL_ATTR_ON_OFF_ID, ESP_ZB_ZCL_ATTR_TYPE_BOOL, ESP_ZB_ZCL_ATTR_READ_WRITE, &onoff_value);
    esp_zb_zcl_attr_list_add_attr(onoff_cluster, onoff_attr);
    
    esp_zb_cluster_list_add_cluster(cluster_list, onoff_cluster);
    
    return cluster_list;
}

void esp_zb_motion_light_init(void)
{
    /* Initialize motion driver */
    motion_driver_init();
    motion_driver_set_callback(motion_detected_callback);
    
    /* Initialize light driver */
    light_driver_init();
    
    /* Create custom clusters */
    esp_zb_ep_list_t *ep_list = esp_zb_ep_list_create();
    esp_zb_endpoint_config_t endpoint_config = {
        .endpoint = HA_ESP_MOTION_LIGHT_ENDPOINT,
        .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID,
        .app_device_id = ESP_ZB_HA_DEVICE_ID_OCCUPANCY_SENSOR,
        .app_device_version = 0
    };
    
    esp_zb_endpoint_add(ep_list, &endpoint_config, custom_clusters_create());
    
    /* Set device info */
    esp_zb_device_add(ep_list, NULL, NULL, NULL, NULL);
    
    /* Set signal handler */
    esp_zb_core_signal_register(esp_zb_app_signal_handler);
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
    ESP_ERROR_CHECK(esp_zb_init(ESP_ZB_ZED_CONFIG()));
    ESP_ERROR_CHECK(esp_zb_set_radio_config(ESP_ZB_DEFAULT_RADIO_CONFIG()));
    ESP_ERROR_CHECK(esp_zb_set_host_config(ESP_ZB_DEFAULT_HOST_CONFIG()));
    
    /* Initialize motion light */
    esp_zb_motion_light_init();
    
    /* Restore last data */
    restore_last_data();
    
    /* Start Zigbee */
    ESP_ERROR_CHECK(esp_zb_start(false));
    esp_zb_main_loop_iteration();
    
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
        esp_zb_main_loop_iteration();
    }
}
