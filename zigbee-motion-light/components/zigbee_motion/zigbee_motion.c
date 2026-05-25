/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 *
 * Zigbee Motion Light Component
 *
 * This component handles Zigbee stack initialization, cluster configuration,
 * and occupancy reporting for the motion light device.
 */

#include "esp_log.h"
#include "esp_check.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_zigbee_core.h"
#include "esp_zigbee_attribute.h"
#include "nwk/esp_zigbee_nwk.h"
#include "ha/esp_zigbee_ha_standard.h"
#include "zcl/esp_zigbee_zcl_command.h"
#include "zigbee_motion.h"
#include "link_status_led.h"
#include "motion_driver.h"

static const char *TAG = "ZIGBEE_MOTION";

/* Zigbee configuration */
#define ESP_ZB_ZED_CONFIG()                               \
    {                                                     \
        .esp_zb_role = ESP_ZB_DEVICE_TYPE_ED,            \
        .install_code_policy = false,                     \
        .nwk_cfg.zed_cfg = {                             \
            .ed_timeout = ESP_ZB_ED_AGING_TIMEOUT_64MIN, \
            .keep_alive = 3000,                           \
        },                                                \
    }

#define ESP_MANUFACTURER_NAME   "\x09""ESPRESSIF"
#define ESP_MODEL_IDENTIFIER    "\x0D""Motion-Light"

#define COORDINATOR_ADDR         0x0000
#define COORDINATOR_ENDPOINT     1

#define ON_OFF_NVS_NAMESPACE  "motion_lt"
#define ON_OFF_NVS_KEY        "on_off"

/* Occupancy cluster reference */
static esp_zb_attribute_list_t *s_occupancy_cluster = NULL;

/* Occupancy state tracking */
static bool s_last_occupancy_state = false;

static bool s_joined;
static bool s_pending_valid;
static bool s_pending_occupied;
static TaskHandle_t s_zigbee_task_handle = NULL;
static TaskHandle_t s_monitor_task_handle = NULL;
static EventGroupHandle_t s_wake_events = NULL;
static EventBits_t s_ready_bit = 0;
static bool s_report_payload_occupied;
static bool s_light_enabled = true;

static void zigbee_motion_mark_left(void);
static esp_err_t send_occupancy_to_network(bool occupied, bool dedupe);
static void flush_pending_occupancy(void);
static void monitor_task(void *pvParameters);
static void try_signal_wake_ready(void);
static bool zigbee_motion_network_is_valid(void);
static void zigbee_motion_log_nvram_snapshot(const char *context);
static void zigbee_motion_lock_channel_from_stack(void);
static void zigbee_motion_load_on_off_from_nvs(void);
static void zigbee_motion_save_on_off_to_nvs(bool enabled);
static void zigbee_motion_apply_on_off(bool enabled);
static void zigbee_motion_sync_on_off_from_stack(void);
static esp_err_t zigbee_motion_on_off_attr_handler(const esp_zb_zcl_set_attr_value_message_t *message);

static void occupancy_report_issue_cb(uint8_t param)
{
    (void)param;

    uint8_t occupancy_value = s_report_payload_occupied ? 1 : 0;
    esp_zb_zcl_status_t attr_status = esp_zb_zcl_set_attribute_val(
        MOTION_LIGHT_ENDPOINT,
        ESP_ZB_ZCL_CLUSTER_ID_OCCUPANCY_SENSING,
        ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
        ESP_ZB_ZCL_ATTR_OCCUPANCY_SENSING_OCCUPANCY_ID,
        &occupancy_value, false);
    if (attr_status != ESP_ZB_ZCL_STATUS_SUCCESS) {
        ESP_LOGW(TAG, "Set occupancy attribute failed: 0x%02x", attr_status);
        try_signal_wake_ready();
        return;
    }

    esp_zb_zcl_report_attr_cmd_t report_cmd = {
        .zcl_basic_cmd = {
            .dst_addr_u.addr_short = COORDINATOR_ADDR,
            .dst_endpoint = COORDINATOR_ENDPOINT,
            .src_endpoint = MOTION_LIGHT_ENDPOINT,
        },
        .address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT,
        .direction = ESP_ZB_ZCL_CMD_DIRECTION_TO_CLI,
        .clusterID = ESP_ZB_ZCL_CLUSTER_ID_OCCUPANCY_SENSING,
        .attributeID = ESP_ZB_ZCL_ATTR_OCCUPANCY_SENSING_OCCUPANCY_ID,
    };
    esp_err_t ret = esp_zb_zcl_report_attr_cmd_req(&report_cmd);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Occupancy report request failed: %s", esp_err_to_name(ret));
    }

    try_signal_wake_ready();
}

static bool zigbee_motion_network_is_valid(void)
{
    if (!esp_zb_bdb_dev_joined()) {
        return false;
    }

    uint16_t addr = esp_zb_get_short_address();
    uint16_t pan = esp_zb_get_pan_id();
    return addr != 0 && addr != 0xFFFE && addr != 0xFFFF &&
           pan != 0 && pan != 0xFFFF;
}

static void zigbee_motion_log_nvram_snapshot(const char *context)
{
    esp_zb_ieee_addr_t ext_pan;

    esp_zb_get_extended_pan_id(ext_pan);
    ESP_LOGI(TAG,
             "NVRAM snapshot [%s]: factory_new=%d dev_joined=%d valid=%d "
             "PAN=0x%04hx Ch=%u Addr=0x%04hx ExtPAN=%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x",
             context,
             esp_zb_bdb_is_factory_new(),
             esp_zb_bdb_dev_joined(),
             zigbee_motion_network_is_valid(),
             esp_zb_get_pan_id(),
             esp_zb_get_current_channel(),
             esp_zb_get_short_address(),
             ext_pan[7], ext_pan[6], ext_pan[5], ext_pan[4],
             ext_pan[3], ext_pan[2], ext_pan[1], ext_pan[0]);
}

static void zigbee_motion_lock_channel_from_stack(void)
{
    uint8_t ch = esp_zb_get_current_channel();

    if (ch >= 11 && ch <= 26) {
        esp_zb_set_primary_network_channel_set(1 << ch);
        esp_zb_set_secondary_network_channel_set(0);
    }
}

static void zigbee_motion_load_on_off_from_nvs(void)
{
    nvs_handle_t handle;
    if (nvs_open(ON_OFF_NVS_NAMESPACE, NVS_READONLY, &handle) != ESP_OK) {
        return;
    }

    uint8_t stored = 1;
    if (nvs_get_u8(handle, ON_OFF_NVS_KEY, &stored) == ESP_OK) {
        s_light_enabled = (stored != 0);
        ESP_LOGI(TAG, "On/Off state from NVS: %s", s_light_enabled ? "ON" : "OFF");
    }
    nvs_close(handle);
}

static void zigbee_motion_save_on_off_to_nvs(bool enabled)
{
    nvs_handle_t handle;
    if (nvs_open(ON_OFF_NVS_NAMESPACE, NVS_READWRITE, &handle) != ESP_OK) {
        ESP_LOGW(TAG, "Failed to open NVS for On/Off state");
        return;
    }

    esp_err_t err = nvs_set_u8(handle, ON_OFF_NVS_KEY, enabled ? 1 : 0);
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to persist On/Off state: %s", esp_err_to_name(err));
    }
    nvs_close(handle);
}

static void zigbee_motion_apply_on_off(bool enabled)
{
    if (s_light_enabled == enabled) {
        return;
    }

    s_light_enabled = enabled;
    zigbee_motion_save_on_off_to_nvs(enabled);
    ESP_LOGI(TAG, "Light %s via On/Off cluster", enabled ? "enabled" : "disabled");
}

static void zigbee_motion_sync_on_off_from_stack(void)
{
    esp_zb_zcl_attr_t *attr = esp_zb_zcl_get_attribute(
        MOTION_LIGHT_ENDPOINT,
        ESP_ZB_ZCL_CLUSTER_ID_ON_OFF,
        ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
        ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID);
    if (attr == NULL || attr->data_p == NULL) {
        return;
    }

    bool enabled = *(bool *)attr->data_p;
    s_light_enabled = enabled;
    zigbee_motion_save_on_off_to_nvs(enabled);
    ESP_LOGI(TAG, "On/Off state from stack: %s", enabled ? "ON" : "OFF");
}

static esp_err_t zigbee_motion_on_off_attr_handler(const esp_zb_zcl_set_attr_value_message_t *message)
{
    if (message->info.dst_endpoint != MOTION_LIGHT_ENDPOINT ||
        message->info.cluster != ESP_ZB_ZCL_CLUSTER_ID_ON_OFF ||
        message->info.status != ESP_ZB_ZCL_STATUS_SUCCESS) {
        return ESP_OK;
    }

    if (message->attribute.id == ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID &&
        message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_BOOL) {
        bool enabled = message->attribute.data.value ? *(bool *)message->attribute.data.value : false;
        ESP_LOGI(TAG, "Coordinator wrote On/Off: %s", enabled ? "ON" : "OFF");
        zigbee_motion_apply_on_off(enabled);
    }

    return ESP_OK;
}

static void zigbee_motion_mark_left(void)
{
    s_joined = false;
    s_pending_valid = false;

    if (s_monitor_task_handle != NULL) {
        vTaskDelete(s_monitor_task_handle);
        s_monitor_task_handle = NULL;
    }

    link_status_led_set_steering();

    if (s_wake_events != NULL) {
        xEventGroupSetBits(s_wake_events, s_ready_bit);
        ESP_LOGI(TAG, "Supervisor released after leave (bits 0x%lx)",
                 (unsigned long)xEventGroupGetBits(s_wake_events));
    }
}

static void try_signal_wake_ready(void)
{
    if (s_wake_events != NULL && s_joined && !s_pending_valid) {
        xEventGroupSetBits(s_wake_events, s_ready_bit);
        ESP_LOGI(TAG, "Zigbee track ready (bits 0x%lx)",
                 (unsigned long)xEventGroupGetBits(s_wake_events));
    }
}


static void zigbee_motion_mark_joined(void)
{
    if (s_joined) {
        return;
    }
    s_joined = true;
    link_status_led_set_joined();
    zigbee_motion_sync_on_off_from_stack();
    flush_pending_occupancy();

    /* Create monitor task after joining */
    BaseType_t ret = xTaskCreate(monitor_task, "zb_monitor", 4096, NULL, 4, &s_monitor_task_handle);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create monitor task");
    } else {
        ESP_LOGI(TAG, "Monitor task created after joining");
    }
}

static void flush_pending_occupancy(void)
{
    if (!s_joined || !s_pending_valid) {
        return;
    }
    bool occ = s_pending_occupied;
    esp_err_t r = send_occupancy_to_network(occ, true);
    if (r == ESP_OK) {
        s_pending_valid = false;
        return;
    }
    ESP_LOGW(TAG, "flush pending occupancy failed, keeping intent for retry");
}

static void monitor_task(void *pvParameters)
{
    (void)pvParameters;

    ESP_LOGI(TAG, "Monitor task started");

    vTaskDelay(pdMS_TO_TICKS(100));

    for (;;) {
        bool motion = motion_driver_get_state();
        ESP_LOGI(TAG, "Publishing occupancy (%s)", motion ? "OCCUPIED" : "UNOCCUPIED");

        if (send_occupancy_to_network(motion, false) == ESP_OK) {
            break;
        }

        ESP_LOGW(TAG, "Occupancy report failed, retrying in 1 s");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    ESP_LOGI(TAG, "Monitor task finished");
    vTaskDelete(NULL);
}

static esp_err_t send_occupancy_to_network(bool occupied, bool dedupe)
{
    if (dedupe && s_last_occupancy_state == occupied) {
        try_signal_wake_ready();
        return ESP_OK;
    }

    if (!s_joined) {
        return ESP_ERR_INVALID_STATE;
    }

    link_status_led_notify_occupancy_issued();
    s_report_payload_occupied = occupied;

    esp_zb_lock_acquire(portMAX_DELAY);
    esp_zb_scheduler_alarm((esp_zb_callback_t)occupancy_report_issue_cb, 0, 0);
    esp_zb_lock_release();

    ESP_LOGI(TAG, "Occupancy report queued (%s)",
             occupied ? "OCCUPIED" : "UNOCCUPIED");

    s_last_occupancy_state = occupied;
    return ESP_OK;
}

/* ───────────────────── Zigbee Callbacks ───────────────────── */

static void bdb_start_top_level_commissioning_cb(uint8_t mode_mask)
{
    ESP_RETURN_ON_FALSE(esp_zb_bdb_start_top_level_commissioning(mode_mask) == ESP_OK, ,
                        TAG, "Failed to start Zigbee commissioning");
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
            const char *boot_ctx = (sig_type == ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START)
                                       ? "DEVICE_FIRST_START"
                                       : "DEVICE_REBOOT";
            zigbee_motion_log_nvram_snapshot(boot_ctx);
            ESP_LOGI(TAG, "Device started up in%s factory-reset mode",
                     esp_zb_bdb_is_factory_new() ? "" : " non");
            if (esp_zb_bdb_is_factory_new()) {
                ESP_LOGI(TAG, "Start network steering");
                link_status_led_set_steering();
                esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_STEERING);
            } else if (zigbee_motion_network_is_valid()) {
                ESP_LOGI(TAG, "Rejoined from NVRAM");
                zigbee_motion_lock_channel_from_stack();
                zigbee_motion_mark_joined();
            } else {
                ESP_LOGW(TAG, "NVRAM data present but network invalid, falling back to steering");
                link_status_led_set_steering();
                esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_STEERING);
            }
        } else {
            zigbee_motion_log_nvram_snapshot(esp_zb_zdo_signal_to_string(sig_type));
            if (zigbee_motion_network_is_valid()) {
                ESP_LOGI(TAG, "Rejoined from NVRAM (%s reported %s)",
                         esp_zb_zdo_signal_to_string(sig_type), esp_err_to_name(err_status));
                zigbee_motion_lock_channel_from_stack();
                zigbee_motion_mark_joined();
            } else {
                ESP_LOGW(TAG, "%s failed with status: %s",
                         esp_zb_zdo_signal_to_string(sig_type), esp_err_to_name(err_status));
                if (sig_type == ESP_ZB_BDB_SIGNAL_DEVICE_REBOOT && !esp_zb_bdb_is_factory_new()) {
                    ESP_LOGW(TAG, "Rejoin in progress, retrying initialization");
                    esp_zb_scheduler_alarm((esp_zb_callback_t)bdb_start_top_level_commissioning_cb,
                                           ESP_ZB_BDB_MODE_INITIALIZATION, 1000);
                } else {
                    esp_zb_scheduler_alarm((esp_zb_callback_t)bdb_start_top_level_commissioning_cb,
                                           ESP_ZB_BDB_MODE_INITIALIZATION, 1000);
                }
            }
        }
        break;

    case ESP_ZB_ZDO_SIGNAL_LEAVE:
        if (err_status == ESP_OK && s_joined) {
            esp_zb_zdo_signal_leave_params_t *leave_params =
                (esp_zb_zdo_signal_leave_params_t *)esp_zb_app_signal_get_params(p_sg_p);
            if (leave_params != NULL) {
                ESP_LOGI(TAG, "ZDO Leave received (type=%u)", leave_params->leave_type);
            } else {
                ESP_LOGI(TAG, "ZDO Leave received");
            }
            zigbee_motion_mark_left();
            esp_zb_scheduler_alarm((esp_zb_callback_t)bdb_start_top_level_commissioning_cb,
                                   ESP_ZB_BDB_MODE_NETWORK_STEERING, 1000);
        } else if (err_status == ESP_OK) {
            ESP_LOGD(TAG, "ZDO Leave during stack cleanup (ignored)");
        } else {
            ESP_LOGW(TAG, "ZDO Leave failed: %s", esp_err_to_name(err_status));
        }
        break;

    case ESP_ZB_BDB_SIGNAL_STEERING:
        if (s_joined) {
            ESP_LOGD(TAG, "Ignoring steering signal, already joined");
            break;
        }
        if (err_status == ESP_OK && zigbee_motion_network_is_valid()) {
            zigbee_motion_log_nvram_snapshot("STEERING join");
            zigbee_motion_lock_channel_from_stack();
            ESP_LOGI(TAG, "Joined network (PAN: 0x%04hx, Ch:%d, Addr: 0x%04hx)",
                     esp_zb_get_pan_id(), esp_zb_get_current_channel(),
                     esp_zb_get_short_address());
            zigbee_motion_mark_joined();
        } else if (err_status == ESP_OK) {
            zigbee_motion_log_nvram_snapshot("STEERING invalid");
            ESP_LOGW(TAG, "Steering OK but not on network (PAN: 0x%04hx, Ch:%d, Addr: 0x%04hx, joined=%d)",
                     esp_zb_get_pan_id(), esp_zb_get_current_channel(),
                     esp_zb_get_short_address(), esp_zb_bdb_dev_joined());
            link_status_led_set_steering();
            esp_zb_scheduler_alarm((esp_zb_callback_t)bdb_start_top_level_commissioning_cb,
                                   ESP_ZB_BDB_MODE_NETWORK_STEERING, 1000);
        } else {
            ESP_LOGW(TAG, "Network steering failed: %s", esp_err_to_name(err_status));
            link_status_led_set_steering();
            esp_zb_scheduler_alarm((esp_zb_callback_t)bdb_start_top_level_commissioning_cb,
                                   ESP_ZB_BDB_MODE_NETWORK_STEERING, 1000);
        }
        break;

    case ESP_ZB_COMMON_SIGNAL_CAN_SLEEP:
        /* Deep sleep reboot replaces stack sleep; nothing to do. */
        break;

    default:
        ESP_LOGI(TAG, "ZDO signal: %s (0x%x), status: %s",
                 esp_zb_zdo_signal_to_string(sig_type), sig_type,
                 esp_err_to_name(err_status));
        break;
    }
}

static esp_err_t zb_action_handler(esp_zb_core_action_callback_id_t callback_id, const void *message)
{
    if (callback_id == ESP_ZB_CORE_SET_ATTR_VALUE_CB_ID && message != NULL) {
        zigbee_motion_on_off_attr_handler((const esp_zb_zcl_set_attr_value_message_t *)message);
    }

    return ESP_OK;
}

/* ───────────────────── Zigbee Task ───────────────────── */

static void esp_zb_task(void *pvParameters)
{
    esp_zb_cfg_t zb_nwk_cfg = ESP_ZB_ZED_CONFIG();
    esp_zb_init(&zb_nwk_cfg);

    /* Create cluster list */
    esp_zb_cluster_list_t *cluster_list = esp_zb_zcl_cluster_list_create();

    /* Basic cluster */
    esp_zb_basic_cluster_cfg_t basic_cfg = {
        .zcl_version = ESP_ZB_ZCL_BASIC_ZCL_VERSION_DEFAULT_VALUE,
        .power_source = ESP_ZB_ZCL_BASIC_POWER_SOURCE_DEFAULT_VALUE,
    };
    esp_zb_attribute_list_t *basic_cluster = esp_zb_basic_cluster_create(&basic_cfg);
    esp_zb_basic_cluster_add_attr(basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID,
                                   (void *)ESP_MANUFACTURER_NAME);
    esp_zb_basic_cluster_add_attr(basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID,
                                   (void *)ESP_MODEL_IDENTIFIER);
    esp_zb_cluster_list_add_basic_cluster(cluster_list, basic_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_basic_cluster(cluster_list, esp_zb_basic_cluster_create(NULL),
                                          ESP_ZB_ZCL_CLUSTER_CLIENT_ROLE);

    /* Identify cluster */
    esp_zb_identify_cluster_cfg_t identify_cfg = { .identify_time = 0 };
    esp_zb_attribute_list_t *identify_cluster = esp_zb_identify_cluster_create(&identify_cfg);
    esp_zb_cluster_list_add_identify_cluster(cluster_list, identify_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);

    /* On/Off cluster (server) — coordinator can disable motion LED strip */
    esp_zb_on_off_cluster_cfg_t on_off_cfg = {
        .on_off = s_light_enabled,
    };
    esp_zb_attribute_list_t *on_off_cluster = esp_zb_on_off_cluster_create(&on_off_cfg);
    esp_zb_cluster_list_add_on_off_cluster(cluster_list, on_off_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);

    /* Occupancy Sensing cluster (server role) - to report motion state */
    esp_zb_occupancy_sensing_cluster_cfg_t occupancy_cfg = {
        .occupancy = 0,
    };
    s_occupancy_cluster = esp_zb_occupancy_sensing_cluster_create(&occupancy_cfg);
    esp_zb_cluster_list_add_occupancy_sensing_cluster(cluster_list, s_occupancy_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_occupancy_sensing_cluster(
        cluster_list, esp_zb_occupancy_sensing_cluster_create(&occupancy_cfg), ESP_ZB_ZCL_CLUSTER_CLIENT_ROLE);

    /* Create endpoint */
    esp_zb_ep_list_t *ep_list = esp_zb_ep_list_create();
    esp_zb_endpoint_config_t ep_cfg = {
        .endpoint = MOTION_LIGHT_ENDPOINT,
        .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID,
        .app_device_id = ESP_ZB_HA_ON_OFF_LIGHT_DEVICE_ID,
        .app_device_version = 0,
    };
    esp_zb_ep_list_add_ep(ep_list, cluster_list, ep_cfg);

    esp_zb_device_register(ep_list);
    esp_zb_core_action_handler_register(zb_action_handler);
    esp_zb_set_rx_on_when_idle(false);
    esp_zb_sleep_enable(false);
    esp_zb_set_primary_network_channel_set(1 << ESP_ZB_PRIMARY_CHANNEL);

    ESP_ERROR_CHECK(esp_zb_start(false));
    esp_zb_stack_main_loop();
}

/* ───────────────────── Public API ───────────────────── */

TaskHandle_t zigbee_motion_init(EventGroupHandle_t wake_events, EventBits_t ready_bit)
{
    ESP_LOGI(TAG, "Initializing Zigbee motion light component");

    s_wake_events = wake_events;
    s_ready_bit = ready_bit;
    s_joined = false;
    s_pending_valid = false;
    s_pending_occupied = false;
    s_last_occupancy_state = false;
    s_light_enabled = true;
    zigbee_motion_load_on_off_from_nvs();

    /* Configure Zigbee platform */
    esp_zb_platform_config_t config = {
        .radio_config = {.radio_mode = ZB_RADIO_MODE_NATIVE},
        .host_config = {.host_connection_mode = ZB_HOST_CONNECTION_MODE_NONE},
    };
    ESP_ERROR_CHECK(esp_zb_platform_config(&config));

    /* Create Zigbee task */
    BaseType_t ret = xTaskCreate(esp_zb_task, "Zigbee_main", 4096, NULL, 5, &s_zigbee_task_handle);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create Zigbee task");
        return NULL;
    }

    ESP_LOGI(TAG, "Zigbee motion light component initialized");
    return s_zigbee_task_handle;
}

esp_err_t zigbee_motion_send_occupancy_report(bool occupied)
{

    if (!s_joined) {
        s_pending_occupied = occupied;
        s_pending_valid = true;
        ESP_LOGD(TAG, "Occupancy queued (not joined yet): %s", occupied ? "OCCUPIED" : "UNOCCUPIED");
        return ESP_OK;
    }

    return send_occupancy_to_network(occupied, true);
}

esp_err_t zigbee_motion_publish_occupancy_refresh(bool occupied)
{

    if (!s_joined) {
        s_pending_occupied = occupied;
        s_pending_valid = true;
        ESP_LOGD(TAG, "Occupancy refresh queued (not joined yet): %s", occupied ? "OCCUPIED" : "UNOCCUPIED");
        return ESP_OK;
    }

    return send_occupancy_to_network(occupied, false);
}

bool zigbee_motion_is_joined(void)
{
    return s_joined;
}

bool zigbee_motion_light_enabled(void)
{
    return s_light_enabled;
}

bool zigbee_motion_occupancy_intent_pending(void)
{
    return s_pending_valid;
}

