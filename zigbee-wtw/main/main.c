#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "esp_zigbee_core.h"
#include "zcl/esp_zigbee_zcl_command.h"
#include "ha/esp_zigbee_ha_standard.h"

#define TAG "ZIGBEE_BUTTONS"

// Define the GPIO pins for outputs (modify these according to your board)
#define OUTPUT_1_GPIO    GPIO_NUM_4
#define OUTPUT_2_GPIO    GPIO_NUM_5
#define OUTPUT_3_GPIO    GPIO_NUM_6

// Zigbee endpoint and cluster IDs
#define HA_DEVICE_ENDPOINT  1
#define HA_ON_OFF_CLUSTER_ID 0x0006  // On/Off cluster ID


// Device information
static char manufacturer_name[] = "ESP";
static char model_identifier[] = "ESP32C6_ZIGBEE_BUTTONS";
static char date_code[] = "20240625";

// Initialize GPIO outputs
static void init_gpio(void)
{
    gpio_config_t io_conf = {
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    // Configure all output pins
    io_conf.pin_bit_mask = (1ULL << OUTPUT_1_GPIO) | (1ULL << OUTPUT_2_GPIO) | (1ULL << OUTPUT_3_GPIO);
    gpio_config(&io_conf);
    
    // Initialize all outputs to OFF
    gpio_set_level(OUTPUT_1_GPIO, 0);
    gpio_set_level(OUTPUT_2_GPIO, 0);
    gpio_set_level(OUTPUT_3_GPIO, 0);
}

// Handle Zigbee app signals
void esp_zb_app_signal_handler(esp_zb_app_signal_t *signal_struct)
{
    uint32_t *p_sg_p = signal_struct->p_app_signal;
    esp_err_t err_status = signal_struct->esp_err_status;
    esp_zb_app_signal_type_t sig_type = *p_sg_p;

    switch (sig_type) {
        case ESP_ZB_ZDO_SIGNAL_SKIP_STARTUP:
            ESP_LOGI(TAG, "Zigbee stack initialized");
            esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_INITIALIZATION);
            break;

        case ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START:
            if (err_status == ESP_OK) {
                ESP_LOGI(TAG, "Device started up in factory-reset mode");
                if (esp_zb_bdb_is_factory_new()) {
                    ESP_LOGI(TAG, "Start network steering");
                    esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_STEERING);
                } else {
                    ESP_LOGI(TAG, "Device rebooted");
                }
            } else {
                ESP_LOGE(TAG, "Failed to initialize Zigbee stack (status: %s)", esp_err_to_name(err_status));
            }
            break;

        case ESP_ZB_BDB_SIGNAL_DEVICE_REBOOT:
            if (err_status == ESP_OK) {
                ESP_LOGI(TAG, "Device rebooted and rejoined network successfully");
            } else {
                ESP_LOGE(TAG, "Failed to rejoin network after reboot (status: %s)", esp_err_to_name(err_status));
                esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_STEERING);
            }
            break;

        case ESP_ZB_BDB_SIGNAL_STEERING:
            if (err_status == ESP_OK) {
                ESP_LOGI(TAG, "Network steering completed successfully");
            } else {
                ESP_LOGI(TAG, "Network steering was not successful (status: %s)", esp_err_to_name(err_status));
                esp_zb_scheduler_alarm((esp_zb_callback_t)esp_zb_bdb_start_top_level_commissioning, ESP_ZB_BDB_MODE_NETWORK_STEERING, 1000);
            }
            break;

        case ESP_ZB_ZDO_SIGNAL_LEAVE:
            ESP_LOGI(TAG, "Network left, restart commissioning");
            esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_STEERING);
            break;

        default:
            ESP_LOGI(TAG, "ZDO signal: %s (0x%x), status: %s", esp_zb_zdo_signal_to_string(sig_type), sig_type, esp_err_to_name(err_status));
            break;
    }
}

// Handle ZCL attribute changes
static esp_err_t zb_attribute_handler(const esp_zb_zcl_set_attr_value_message_t *message)
{
    esp_err_t ret = ESP_OK;
    bool light_state = 0;

    if (!message) {
        ESP_LOGE(TAG, "Empty message");
        return ESP_ERR_INVALID_ARG;
    }
    if (message->info.status != ESP_ZB_ZCL_STATUS_SUCCESS) {
        ESP_LOGE(TAG, "Received message: error status(%d)", message->info.status);
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Received message: endpoint(0x%x), cluster(0x%x), attribute(0x%x), data size(%d)", 
             message->info.dst_endpoint, message->info.cluster, 
             message->attribute.id, message->attribute.data.size);

    if (message->info.dst_endpoint <= 3 && message->info.cluster == ESP_ZB_ZCL_CLUSTER_ID_ON_OFF) {
        if (message->attribute.id == ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID && 
            message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_BOOL) {
            light_state = message->attribute.data.value ? *(bool *)message->attribute.data.value : light_state;
            ESP_LOGI(TAG, "Light state: %s on endpoint %d", light_state ? "On" : "Off", message->info.dst_endpoint);
            
            // Map endpoint to GPIO
            gpio_num_t gpio_pin;
            switch (message->info.dst_endpoint) {
                case 1: gpio_pin = OUTPUT_1_GPIO; break;
                case 2: gpio_pin = OUTPUT_2_GPIO; break;
                case 3: gpio_pin = OUTPUT_3_GPIO; break;
                default: 
                    ESP_LOGI(TAG, "Invalid endpoint: %d", message->info.dst_endpoint);
                    return ESP_ERR_INVALID_ARG;
            }
            
            esp_zb_lock_acquire(portMAX_DELAY);
            gpio_set_level(gpio_pin, light_state ? 1 : 0);
            esp_zb_lock_release();
        }
    }
    return ret;
}

// Handle ZCL action callbacks
static esp_err_t zb_action_handler(esp_zb_core_action_callback_id_t callback_id, const void *message)
{
    esp_err_t ret = ESP_OK;
    switch (callback_id) {
        case ESP_ZB_CORE_SET_ATTR_VALUE_CB_ID:
            ret = zb_attribute_handler((esp_zb_zcl_set_attr_value_message_t *)message);
            break;
        default:
            ESP_LOGW(TAG, "Receive Zigbee action(0x%x) callback", callback_id);
            break;
    }
    return ret;
}

static void esp_zb_task(void *pvParameters)
{
    ESP_LOGI(TAG, "DEBUG: Starting esp_zb_task");
    
    // Initialize Zigbee stack
    esp_zb_cfg_t zb_nwk_cfg = {
        .esp_zb_role = ESP_ZB_DEVICE_TYPE_ED,
        .install_code_policy = false,
        .nwk_cfg = {
            .zczr_cfg = {
                .max_children = 10,
            },
        },
    };
    ESP_LOGI(TAG, "DEBUG: About to call esp_zb_init");
    esp_zb_init(&zb_nwk_cfg);
    ESP_LOGI(TAG, "DEBUG: esp_zb_init completed");
    
    // Initialize GPIO
    ESP_LOGI(TAG, "DEBUG: About to init GPIO");
    init_gpio();
    ESP_LOGI(TAG, "DEBUG: GPIO init completed");
    
    // Create endpoint list
    ESP_LOGI(TAG, "DEBUG: About to create endpoint list");
    esp_zb_ep_list_t *ep_list = esp_zb_ep_list_create();
    ESP_LOGI(TAG, "DEBUG: Endpoint list created");
    
    // Create endpoints for each button
    for (int i = 1; i <= 3; i++) {
        ESP_LOGI(TAG, "DEBUG: Creating endpoint %d", i);
        
        // Create basic cluster list for this endpoint
        ESP_LOGI(TAG, "DEBUG: Endpoint %d - About to create basic cluster", i);
        esp_zb_attribute_list_t *esp_zb_basic_cluster = esp_zb_zcl_attr_list_create(ESP_ZB_ZCL_CLUSTER_ID_BASIC);
        ESP_LOGI(TAG, "DEBUG: Endpoint %d - Basic cluster created, adding attributes", i);
        esp_zb_basic_cluster_add_attr(esp_zb_basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_ZCL_VERSION_ID, &(uint8_t) {0x08});
        ESP_LOGI(TAG, "DEBUG: Endpoint %d - Basic cluster - ZCL version added", i);
        esp_zb_basic_cluster_add_attr(esp_zb_basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_POWER_SOURCE_ID, &(uint8_t) {0x03});
        ESP_LOGI(TAG, "DEBUG: Endpoint %d - Basic cluster - power source added", i);
        esp_zb_basic_cluster_add_attr(esp_zb_basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID, manufacturer_name);
        ESP_LOGI(TAG, "DEBUG: Endpoint %d - Basic cluster - manufacturer name added", i);
        esp_zb_basic_cluster_add_attr(esp_zb_basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID, model_identifier);
        ESP_LOGI(TAG, "DEBUG: Endpoint %d - Basic cluster - model identifier added", i);
        esp_zb_basic_cluster_add_attr(esp_zb_basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_DATE_CODE_ID, date_code);
        ESP_LOGI(TAG, "DEBUG: Endpoint %d - Basic cluster - date code added", i);
        
        // Create identify cluster list for this endpoint
        ESP_LOGI(TAG, "DEBUG: Endpoint %d - About to create identify cluster", i);
        esp_zb_attribute_list_t *esp_zb_identify_cluster = esp_zb_zcl_attr_list_create(ESP_ZB_ZCL_CLUSTER_ID_IDENTIFY);
        ESP_LOGI(TAG, "DEBUG: Endpoint %d - Identify cluster created, adding attributes", i);
        esp_zb_identify_cluster_add_attr(esp_zb_identify_cluster, ESP_ZB_ZCL_ATTR_IDENTIFY_IDENTIFY_TIME_ID, &(uint16_t) {0});
        ESP_LOGI(TAG, "DEBUG: Endpoint %d - Identify cluster - time attribute added", i);
        
        // Create on/off cluster for this endpoint
        ESP_LOGI(TAG, "DEBUG: Endpoint %d - About to create on/off cluster", i);
        esp_zb_attribute_list_t *esp_zb_on_off_cluster = esp_zb_zcl_attr_list_create(ESP_ZB_ZCL_CLUSTER_ID_ON_OFF);
        ESP_LOGI(TAG, "DEBUG: Endpoint %d - On/off cluster created", i);
        esp_zb_on_off_cluster_add_attr(esp_zb_on_off_cluster, ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID, &(bool) {false});
        ESP_LOGI(TAG, "DEBUG: Endpoint %d - On/off attribute added", i);
        
        // Create cluster list for server side
        ESP_LOGI(TAG, "DEBUG: Endpoint %d - About to create cluster list", i);
        esp_zb_cluster_list_t *esp_zb_cluster_list = esp_zb_zcl_cluster_list_create();
        ESP_LOGI(TAG, "DEBUG: Endpoint %d - Cluster list created", i);
        
        ESP_LOGI(TAG, "DEBUG: Endpoint %d - About to add basic cluster", i);
        esp_zb_cluster_list_add_basic_cluster(esp_zb_cluster_list, esp_zb_basic_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
        ESP_LOGI(TAG, "DEBUG: Endpoint %d - Basic cluster added", i);
        
        ESP_LOGI(TAG, "DEBUG: Endpoint %d - About to add identify cluster", i);
        esp_zb_cluster_list_add_identify_cluster(esp_zb_cluster_list, esp_zb_identify_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
        ESP_LOGI(TAG, "DEBUG: Endpoint %d - Identify cluster added", i);
        
        ESP_LOGI(TAG, "DEBUG: Endpoint %d - About to add on/off cluster", i);
        esp_zb_cluster_list_add_on_off_cluster(esp_zb_cluster_list, esp_zb_on_off_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
        ESP_LOGI(TAG, "DEBUG: Endpoint %d - On/off cluster added", i);
        
        // Create endpoint configuration
        ESP_LOGI(TAG, "DEBUG: Endpoint %d - About to create endpoint config", i);
        esp_zb_endpoint_config_t endpoint_config = {
            .endpoint = i,
            .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID,
            .app_device_id = ESP_ZB_HA_ON_OFF_OUTPUT_DEVICE_ID,
            .app_device_version = 0
        };
        ESP_LOGI(TAG, "DEBUG: Endpoint %d - Endpoint config created", i);
        
        // Add endpoint to list
        ESP_LOGI(TAG, "DEBUG: Endpoint %d - About to add endpoint to list", i);
        esp_zb_ep_list_add_ep(ep_list, esp_zb_cluster_list, endpoint_config);
        ESP_LOGI(TAG, "DEBUG: Endpoint %d - Endpoint added to list", i);
    }
    
    ESP_LOGI(TAG, "DEBUG: All endpoints created, about to register device");
    // Register the device
    esp_zb_device_register(ep_list);
    ESP_LOGI(TAG, "DEBUG: Device registered");
    
    esp_zb_core_action_handler_register(zb_action_handler);
    ESP_LOGI(TAG, "DEBUG: Action handler registered");
    
    esp_zb_set_primary_network_channel_set(ESP_ZB_TRANSCEIVER_ALL_CHANNELS_MASK);
    ESP_LOGI(TAG, "DEBUG: Network channel set");
    
    ESP_LOGI(TAG, "DEBUG: About to start Zigbee stack");
    ESP_ERROR_CHECK(esp_zb_start(false));
    ESP_LOGI(TAG, "DEBUG: Zigbee stack started, entering main loop");
    esp_zb_main_loop_iteration();
}

void app_main(void)
{
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize Zigbee platform  
    esp_zb_platform_config_t config = {
        .radio_config = {
            .radio_mode = ZB_RADIO_MODE_NATIVE,
        },
        .host_config = {
            .host_connection_mode = ZB_HOST_CONNECTION_MODE_NONE,
        },
    };
    ESP_ERROR_CHECK(esp_zb_platform_config(&config));
    
    // Initialize logging
    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set(TAG, ESP_LOG_INFO);
    
    ESP_LOGI(TAG, "Starting Zigbee 3-button controller...");

    // Start Zigbee stack
    xTaskCreate(esp_zb_task, "Zigbee_main", 4096, NULL, 5, NULL);
}
