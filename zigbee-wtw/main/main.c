/**
 * Super Simple Multistate Value Zigbee Device
 * Based on ESP-IDF HA_on_off_light example
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_zigbee_core.h"
#include "zcl/esp_zigbee_zcl_command.h"
#include "ha/esp_zigbee_ha_standard.h"
#include "main.h"


static const char *get_cluster_name(uint16_t cluster_id) {
    switch (cluster_id) {
        case ESP_ZB_ZCL_CLUSTER_ID_BASIC: return "Basic";
        case ESP_ZB_ZCL_CLUSTER_ID_IDENTIFY: return "Identify";
        case ESP_ZB_ZCL_CLUSTER_ID_MULTI_VALUE: return "Multistate Value";
        default: return "Unknown";
    }
}

static const char *get_multistate_attr_name(uint16_t attr_id) {
    switch (attr_id) {
        case ESP_ZB_ZCL_ATTR_MULTI_VALUE_PRESENT_VALUE_ID: return "Present Value";
        case ESP_ZB_ZCL_ATTR_MULTI_VALUE_OUT_OF_SERVICE_ID: return "Out of Service";
        case ESP_ZB_ZCL_ATTR_MULTI_VALUE_STATUS_FLAGS_ID: return "Status Flags";
        default: return "Unknown Attribute";
    }
}


#define TAG "SIMPLE_MULTISTATE"

// Simple multistate value: 1=Off, 2=Low, 3=High
static uint16_t multistate_value = 1;

// Basic device info
static uint8_t manufacturer[] = {6, 'E', 'S', 'P', '-', '3', '2'};
static uint8_t model[] = {3, 'W', 'T', 'W'};

// Zigbee signal handler
void esp_zb_app_signal_handler(esp_zb_app_signal_t *signal_struct)
{
    uint32_t *p_sg_p = signal_struct->p_app_signal;
    esp_zb_app_signal_type_t sig_type = *p_sg_p;
    esp_err_t err_status = signal_struct->esp_err_status;

    switch (sig_type) {
        case ESP_ZB_ZDO_SIGNAL_SKIP_STARTUP:
            ESP_LOGI(TAG, "Zigbee stack initialized");
            esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_INITIALIZATION);
            break;

        case ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START:
            if (err_status == ESP_OK) {
                ESP_LOGI(TAG, "Device started, joining network...");
                esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_STEERING);
            }
            break;

        case ESP_ZB_BDB_SIGNAL_STEERING:
            if (err_status == ESP_OK) {
                ESP_LOGI(TAG, "Joined network successfully");
            } else {
                ESP_LOGI(TAG, "Network steering failed, retrying...");
                esp_zb_scheduler_alarm((esp_zb_callback_t)esp_zb_bdb_start_top_level_commissioning, 
                                       ESP_ZB_BDB_MODE_NETWORK_STEERING, 1000);
            }
            break;

        default:
            ESP_LOGI(TAG, "Signal: %s (0x%x), status: %s", 
                     esp_zb_zdo_signal_to_string(sig_type), sig_type, esp_err_to_name(err_status));
            break;
    }
}
static esp_err_t zb_attribute_handler(const esp_zb_zcl_set_attr_value_message_t *message)
{
    ESP_LOGI(TAG, "Attribute handler called");

    if (!message || message->info.status != ESP_ZB_ZCL_STATUS_SUCCESS) {
        ESP_LOGE(TAG, "Invalid attribute message or status not successful");
        return ESP_ERR_INVALID_ARG;
    }

    uint16_t cluster = message->info.cluster;
    uint16_t attr_id = message->attribute.id;

    ESP_LOGI(TAG, "Zigbee2MQTT Command Received: Cluster = 0x%04x (%s), Attribute = 0x%04x (%s), Endpoint = 0x%02x",
             cluster, get_cluster_name(cluster), attr_id,
             cluster == ESP_ZB_ZCL_CLUSTER_ID_MULTI_VALUE ? get_multistate_attr_name(attr_id) : "Unknown",
             message->info.dst_endpoint);

    if (cluster == ESP_ZB_ZCL_CLUSTER_ID_MULTI_VALUE) {
        if (attr_id == ESP_ZB_ZCL_ATTR_MULTI_VALUE_PRESENT_VALUE_ID) {
            uint16_t new_value = *(uint16_t*)message->attribute.data.value;
            ESP_LOGI(TAG, "-> Set Present Value to %d", new_value);
            
            if (new_value >= 1 && new_value <= 3) {
                multistate_value = new_value;
                const char* states[] = {"", "Off", "Low", "High"};
                ESP_LOGI(TAG, "State changed to: %s (%d)", states[new_value], new_value);
            } else {
                ESP_LOGW(TAG, "Invalid Present Value received: %d", new_value);
            }
        } else if (attr_id == ESP_ZB_ZCL_ATTR_MULTI_VALUE_OUT_OF_SERVICE_ID) {
            bool out_of_service = *(bool*)message->attribute.data.value;
            ESP_LOGI(TAG, "-> Set Out Of Service to %s", out_of_service ? "true" : "false");
        } else if (attr_id == ESP_ZB_ZCL_ATTR_MULTI_VALUE_STATUS_FLAGS_ID) {
            uint8_t flags = *(uint8_t*)message->attribute.data.value;
            ESP_LOGI(TAG, "-> Set Status Flags to 0x%02x", flags);
        } else {
            ESP_LOGW(TAG, "Unhandled multistate attribute ID: 0x%04x", attr_id);
        }
    } else {
        ESP_LOGW(TAG, "Unhandled cluster ID: 0x%04x", cluster);
    }

    return ESP_OK;
}


// Action handler
static esp_err_t zb_action_handler(esp_zb_core_action_callback_id_t callback_id, const void *message)
{
    if (callback_id == ESP_ZB_CORE_SET_ATTR_VALUE_CB_ID) {
        return zb_attribute_handler((esp_zb_zcl_set_attr_value_message_t *)message);
    }
    return ESP_OK;
}

// Create Zigbee device
static void create_zigbee_device(void)
{
    // Create endpoint list
    esp_zb_ep_list_t *ep_list = esp_zb_ep_list_create();
    
    // Basic cluster (required)
    esp_zb_basic_cluster_cfg_t basic_cfg = {
        .zcl_version = ESP_ZB_ZCL_BASIC_ZCL_VERSION_DEFAULT_VALUE,
        .power_source = ESP_ZB_ZCL_BASIC_POWER_SOURCE_UNKNOWN,
    };
    esp_zb_attribute_list_t *basic_cluster = esp_zb_basic_cluster_create(&basic_cfg);
    esp_zb_basic_cluster_add_attr(basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID, manufacturer);
    esp_zb_basic_cluster_add_attr(basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID, model);
    
    // Identify cluster (required)
    esp_zb_attribute_list_t *identify_cluster = esp_zb_zcl_attr_list_create(ESP_ZB_ZCL_CLUSTER_ID_IDENTIFY);
    esp_zb_identify_cluster_add_attr(identify_cluster, ESP_ZB_ZCL_ATTR_IDENTIFY_IDENTIFY_TIME_ID, &(uint16_t){0});
    
    esp_zb_multistate_value_cluster_cfg_t switch_value_cfg = {
            .number_of_states = 3,
            .out_of_service = false,
            .present_value = 1,
            .status_flags = 0,
    };
    esp_zb_attribute_list_t *esp_zb_multistate_cluster = esp_zb_multistate_value_cluster_create(&switch_value_cfg);

    // Create cluster list
    esp_zb_cluster_list_t *cluster_list = esp_zb_zcl_cluster_list_create();
    esp_zb_cluster_list_add_basic_cluster(cluster_list, basic_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_identify_cluster(cluster_list, identify_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_multistate_value_cluster(cluster_list, esp_zb_multistate_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    
    // Create endpoint
    esp_zb_endpoint_config_t endpoint_config = {
        .endpoint = 1,
        .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID,
        .app_device_id = ESP_ZB_HA_SIMPLE_SENSOR_DEVICE_ID,
        .app_device_version = 1
    };
    
    esp_zb_ep_list_add_ep(ep_list, cluster_list, endpoint_config);
    esp_zb_device_register(ep_list);
    esp_zb_core_action_handler_register(zb_action_handler);
    
    ESP_LOGI(TAG, "Simple multistate device created");
}

// Main Zigbee task
static void zigbee_main_task(void *pvParameters)
{
    // Initialize Zigbee stack
    esp_zb_cfg_t zb_nwk_cfg = ESP_ZB_ZED_CONFIG();
    esp_zb_init(&zb_nwk_cfg);
    
    // Create device
    create_zigbee_device();
    
    // Start Zigbee stack
    ESP_ERROR_CHECK(esp_zb_start(false));
    esp_zb_main_loop_iteration();
}

void app_main(void)
{
    // Initialize NVS
    ESP_ERROR_CHECK(nvs_flash_erase());
   ESP_ERROR_CHECK(nvs_flash_init());

    // Initialize Zigbee platform
    esp_zb_platform_config_t config = {
        .radio_config = {.radio_mode = ZB_RADIO_MODE_NATIVE},
        .host_config = {.host_connection_mode = ZB_HOST_CONNECTION_MODE_NONE},
    };
    ESP_ERROR_CHECK(esp_zb_platform_config(&config));
    
    ESP_LOGI(TAG, "Starting simple multistate Zigbee device");
    
    // Start Zigbee task
    xTaskCreate(zigbee_main_task, "Zigbee_main", 4096, NULL, 5, NULL);
}