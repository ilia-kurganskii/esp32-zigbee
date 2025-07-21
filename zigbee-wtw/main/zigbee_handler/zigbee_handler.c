
#include "zigbee_handler.h"

static const char *TAG = "ZIGBEE_HANDLER";

// Basic device info
const uint8_t manufacturer[] = {6, 'E', 'S', 'P', '-', '3', '2'};
const uint8_t model[] = {3, 'W', 'T', 'W'};

// Current output state (0=night, 1=day, 2=shower)
static uint16_t current_output_state = 1; // Default to day mode

static const char *get_cluster_name(uint16_t cluster_id) {
    switch (cluster_id) {
        case ESP_ZB_ZCL_CLUSTER_ID_BASIC: return "Basic";
        case ESP_ZB_ZCL_CLUSTER_ID_IDENTIFY: return "Identify";
        case ESP_ZB_ZCL_CLUSTER_ID_MULTI_VALUE: return "Multistate Value";
        case OTA_CLUSTER_ID: return "OTA";
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

// Zigbee signal handler
void esp_zb_app_signal_handler(esp_zb_app_signal_t *signal_struct)
{
    uint32_t *p_sg_p = signal_struct->p_app_signal;
    esp_zb_app_signal_type_t sig_type = *p_sg_p;
    esp_err_t err_status = signal_struct->esp_err_status;

    switch (sig_type) {
        case ESP_ZB_ZDO_SIGNAL_SKIP_STARTUP:
            app_log(LOG_LEVEL_INFO, TAG, "Initialize Zigbee stack");
            esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_INITIALIZATION);
            break;

        case ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START:
        case ESP_ZB_BDB_SIGNAL_DEVICE_REBOOT:
            if (err_status == ESP_OK) {
                app_log(LOG_LEVEL_INFO, TAG, "Device started up in %s factory-reset mode",
                        esp_zb_bdb_is_factory_new() ? "" : "non ");
                if (esp_zb_bdb_is_factory_new()) {
                    app_log(LOG_LEVEL_INFO, TAG, "Start network steering");
                    esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_STEERING);
                } else {
                    app_log(LOG_LEVEL_INFO, TAG, "Device rebooted");
                }
            } else {
                app_log(LOG_LEVEL_WARN, TAG, "Failed to initialize Zigbee stack (status: %s)",
                        esp_err_to_name(err_status));
            }
            break;

        case ESP_ZB_BDB_SIGNAL_STEERING:
            if (err_status == ESP_OK) {
                esp_zb_ieee_addr_t extended_pan_id;
                esp_zb_get_extended_pan_id(extended_pan_id);
                app_log(LOG_LEVEL_INFO, TAG, "Joined network successfully (Extended PAN ID: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x, PAN ID: 0x%04x, Channel:%d, Short Address: 0x%04x)",
                        extended_pan_id[7], extended_pan_id[6], extended_pan_id[5], extended_pan_id[4],
                        extended_pan_id[3], extended_pan_id[2], extended_pan_id[1], extended_pan_id[0],
                        esp_zb_get_pan_id(), esp_zb_get_current_channel(), esp_zb_get_short_address());
            } else {
                app_log(LOG_LEVEL_INFO, TAG, "Network steering was not successful (status: %s)",
                        esp_err_to_name(err_status));
                esp_zb_scheduler_alarm((esp_zb_callback_t)esp_zb_bdb_start_top_level_commissioning,
                                       ESP_ZB_BDB_MODE_NETWORK_STEERING, 1000);
            }
            break;

        default:
            app_log(LOG_LEVEL_INFO, TAG, "Signal: %s (0x%x), status: %s", 
                     esp_zb_zdo_signal_to_string(sig_type), sig_type, esp_err_to_name(err_status));
            break;
    }
}

// Attribute handler for receiving commands
static esp_err_t zb_attribute_handler(const esp_zb_zcl_set_attr_value_message_t *message)
{
    metrics_increment(METRIC_ZIGBEE_CMD_RECEIVED);
    app_log(LOG_LEVEL_INFO, TAG, "Attribute handler called");

    if (!message || message->info.status != ESP_ZB_ZCL_STATUS_SUCCESS) {
        app_log(LOG_LEVEL_ERROR, TAG, "Invalid attribute message or status not successful");
        return ESP_ERR_INVALID_ARG;
    }

    uint16_t cluster = message->info.cluster;
    uint16_t attr_id = message->attribute.id;

    app_log(LOG_LEVEL_INFO, TAG, "Zigbee2MQTT Command Received: Cluster = 0x%04x (%s), Attribute = 0x%04x (%s), Endpoint = 0x%02x",
             cluster, get_cluster_name(cluster), attr_id,
             cluster == ESP_ZB_ZCL_CLUSTER_ID_MULTI_VALUE ? get_multistate_attr_name(attr_id) : "Unknown",
             message->info.dst_endpoint);

    if (cluster == ESP_ZB_ZCL_CLUSTER_ID_MULTI_VALUE) {
        if (attr_id == ESP_ZB_ZCL_ATTR_MULTI_VALUE_PRESENT_VALUE_ID) {
            uint16_t new_value = *(uint16_t*)message->attribute.data.value;
            app_log(LOG_LEVEL_INFO, TAG, "-> Set Present Value to %d", new_value);
            
            if (new_value >= 0 && new_value <= 2) {
                set_relay_outputs((output_state_t)new_value);
            } else {
                app_log(LOG_LEVEL_WARN, TAG, "Invalid Present Value received: %d (valid range: 0-2)", new_value);
            }
        } else if (attr_id == ESP_ZB_ZCL_ATTR_MULTI_VALUE_OUT_OF_SERVICE_ID) {
            bool out_of_service = *(bool*)message->attribute.data.value;
            app_log(LOG_LEVEL_INFO, TAG, "-> Set Out Of Service to %s", out_of_service ? "true" : "false");
        } else if (attr_id == ESP_ZB_ZCL_ATTR_MULTI_VALUE_STATUS_FLAGS_ID) {
            uint8_t flags = *(uint8_t*)message->attribute.data.value;
            app_log(LOG_LEVEL_INFO, TAG, "-> Set Status Flags to 0x%02x", flags);
        } else {
            app_log(LOG_LEVEL_WARN, TAG, "Unhandled multistate attribute ID: 0x%04x", attr_id);
        }
    } else if (cluster == OTA_CLUSTER_ID) {
        if (attr_id == OTA_ATTR_ID) {
            xTaskCreate(&ota_update_task, "ota_update_task", 8192, NULL, 5, NULL);
        }
    } else {
        app_log(LOG_LEVEL_WARN, TAG, "Unhandled cluster ID: 0x%04x", cluster);
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
    
    // Multistate Value cluster for receiving commands
    esp_zb_multistate_value_cluster_cfg_t multistate_cfg = {
        .number_of_states = 3,
        .out_of_service = false,
        .present_value = 1, // Default to day mode
        .status_flags = 0,
    };
    esp_zb_attribute_list_t *multistate_cluster = esp_zb_multistate_value_cluster_create(&multistate_cfg);

    // OTA cluster
    esp_zb_attribute_list_t *ota_cluster = esp_zb_zcl_attr_list_create(OTA_CLUSTER_ID);
    uint8_t ota_attr_value = 0;
    esp_zb_cluster_add_attr(ota_cluster, OTA_CLUSTER_ID, OTA_ATTR_ID, ESP_ZB_ZCL_ATTR_TYPE_U8, ESP_ZB_ZCL_ATTR_ACCESS_READ_WRITE, &ota_attr_value);

    // Create cluster list
    esp_zb_cluster_list_t *cluster_list = esp_zb_zcl_cluster_list_create();
    esp_zb_cluster_list_add_basic_cluster(cluster_list, basic_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_identify_cluster(cluster_list, identify_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_multistate_value_cluster(cluster_list, multistate_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_custom_cluster(cluster_list, ota_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    
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
    
    app_log(LOG_LEVEL_INFO, TAG, "WTW 2-relay controller device created");
}

// Main Zigbee task
void zigbee_main_task(void *pvParameters)
{
    // Initialize Zigbee stack
    esp_zb_cfg_t zb_nwk_cfg = {
        .esp_zb_role = ESP_ZB_DEVICE_TYPE_ED,
        .install_code_policy = false,
        .nwk_cfg.zed_cfg = {
            .ed_timeout = ESP_ZB_ED_AGING_TIMEOUT_64MIN,
            .keep_alive = 3000,
        },
    };
    esp_zb_init(&zb_nwk_cfg);
    
    // Initialize relay outputs
    init_relay_outputs();
    
    // Create device
    create_zigbee_device();
    
    // Start Zigbee stack
    ESP_ERROR_CHECK(esp_zb_start(false));
    
    // Main task loop
    esp_zb_main_loop_iteration();
}