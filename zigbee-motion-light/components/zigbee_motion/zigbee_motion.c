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
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_zigbee_core.h"
#include "ha/esp_zigbee_ha_standard.h"
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

/* Occupancy cluster reference */
static esp_zb_attribute_list_t *s_occupancy_cluster = NULL;

/* Occupancy state tracking */
static bool s_last_occupancy_state = false;

static bool s_joined;
static bool s_pending_valid;
static bool s_pending_occupied;
static bool s_zigbee_finished = false;
static TaskHandle_t s_zigbee_task_handle = NULL;
static TaskHandle_t s_monitor_task_handle = NULL;
static bool s_motion_prev = false;
static EventGroupHandle_t s_wake_events = NULL;
static EventBits_t s_ready_bit = 0;

static esp_err_t send_occupancy_to_network(bool occupied, bool dedupe);
static void flush_pending_occupancy(void);
static void monitor_task(void *pvParameters);
static void try_signal_wake_ready(void);

static void try_signal_wake_ready(void)
{
    if (s_wake_events != NULL && s_joined && !s_pending_valid && s_zigbee_finished) {
        xEventGroupSetBits(s_wake_events, s_ready_bit);
        ESP_LOGI(TAG, "Zigbee track ready");
    }
}


static void zigbee_motion_mark_joined(void)
{
    s_joined = true;
    link_status_led_set_joined();
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
        try_signal_wake_ready();
        return;
    }
    ESP_LOGW(TAG, "flush pending occupancy failed, keeping intent for retry");
}

static void monitor_task(void *pvParameters)
{
    (void)pvParameters;

    ESP_LOGI(TAG, "Monitor task started");

    s_motion_prev = motion_driver_get_state();
    ESP_LOGI(TAG, "Initial motion state: %s", s_motion_prev ? "DETECTED" : "CLEARED");

    if (s_motion_prev) {
        send_occupancy_to_network(true, true);
    }

    /* If motion is already false, send report and finish immediately */
    if (!s_motion_prev) {
        ESP_LOGI(TAG, "Motion already cleared, sending report and finishing");
        send_occupancy_to_network(false, true);
        s_zigbee_finished = true;
        try_signal_wake_ready();
        ESP_LOGI(TAG, "Monitor task finished (motion never detected)");
        vTaskDelete(NULL);
        return;
    }

    for (;;) {
        bool motion_current = motion_driver_get_state();
        
        if (motion_current != s_motion_prev) {
            s_motion_prev = motion_current;
            ESP_LOGI(TAG, "Motion state changed: %s", motion_current ? "DETECTED" : "CLEARED");
            
            send_occupancy_to_network(motion_current, true);
            
            if (!motion_current) {
                ESP_LOGI(TAG, "Motion cleared, setting zigbee finished flag");
                s_zigbee_finished = true;
                try_signal_wake_ready();
                break;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }

    ESP_LOGI(TAG, "Monitor task finished");
    vTaskDelete(NULL);
}

static esp_err_t send_occupancy_to_network(bool occupied, bool dedupe)
{
    if (dedupe && s_last_occupancy_state == occupied) {
        return ESP_OK;
    }

    link_status_led_notify_occupancy_issued();

    uint8_t occupancy_value = occupied ? 1 : 0;
    esp_zb_lock_acquire(portMAX_DELAY);
    esp_zb_zcl_set_attribute_val(MOTION_LIGHT_ENDPOINT,
                                 ESP_ZB_ZCL_CLUSTER_ID_OCCUPANCY_SENSING,
                                 ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
                                 ESP_ZB_ZCL_ATTR_OCCUPANCY_SENSING_OCCUPANCY_ID,
                                 &occupancy_value, false);
    esp_zb_lock_release();

    s_last_occupancy_state = occupied;
    ESP_LOGI(TAG, "Updated occupancy attribute: %s", occupied ? "OCCUPIED" : "UNOCCUPIED");

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
            ESP_LOGI(TAG, "Device started up in%s factory-reset mode",
                     esp_zb_bdb_is_factory_new() ? "" : " non");
            if (esp_zb_bdb_is_factory_new()) {
                ESP_LOGI(TAG, "Start network steering");
                link_status_led_set_steering();
                esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_STEERING);
            } else {
                ESP_LOGI(TAG, "Rejoined from NVRAM");
                zigbee_motion_mark_joined();
            }
        } else {
            ESP_LOGW(TAG, "%s failed with status: %s, retrying",
                     esp_zb_zdo_signal_to_string(sig_type), esp_err_to_name(err_status));
            esp_zb_scheduler_alarm((esp_zb_callback_t)bdb_start_top_level_commissioning_cb,
                                   ESP_ZB_BDB_MODE_INITIALIZATION, 1000);
        }
        break;

    case ESP_ZB_BDB_SIGNAL_STEERING:
        if (err_status == ESP_OK) {
            esp_zb_ieee_addr_t extended_pan_id;
            esp_zb_get_extended_pan_id(extended_pan_id);
            ESP_LOGI(TAG, "Joined network (PAN: 0x%04hx, Ch:%d, Addr: 0x%04hx)",
                     esp_zb_get_pan_id(), esp_zb_get_current_channel(),
                     esp_zb_get_short_address());
            zigbee_motion_mark_joined();
        } else {
            ESP_LOGW(TAG, "Network steering failed: %s", esp_err_to_name(err_status));
            link_status_led_set_steering();
            esp_zb_scheduler_alarm((esp_zb_callback_t)bdb_start_top_level_commissioning_cb,
                                   ESP_ZB_BDB_MODE_NETWORK_STEERING, 1000);
        }
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
    (void)callback_id;
    (void)message;
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

    /* Identify cluster */
    esp_zb_identify_cluster_cfg_t identify_cfg = { .identify_time = 0 };
    esp_zb_attribute_list_t *identify_cluster = esp_zb_identify_cluster_create(&identify_cfg);
    esp_zb_cluster_list_add_identify_cluster(cluster_list, identify_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);

    /* On/Off cluster (server) - so the device appears as a light in the Zigbee network */
    esp_zb_on_off_cluster_cfg_t on_off_cfg = { .on_off = false };
    esp_zb_attribute_list_t *on_off_cluster = esp_zb_on_off_cluster_create(&on_off_cfg);
    esp_zb_cluster_list_add_on_off_cluster(cluster_list, on_off_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);


    /* Occupancy Sensing cluster (server role) - to report motion state */
    esp_zb_occupancy_sensing_cluster_cfg_t occupancy_cfg = {
        .occupancy = 0,
    };
    s_occupancy_cluster = esp_zb_occupancy_sensing_cluster_create(&occupancy_cfg);
    esp_zb_cluster_list_add_occupancy_sensing_cluster(cluster_list, s_occupancy_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);

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
    esp_zb_set_primary_network_channel_set(1 << 11);

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
    s_zigbee_finished = false;
    s_motion_prev = false;

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

bool zigbee_motion_occupancy_intent_pending(void)
{
    return s_pending_valid;
}

bool zigbee_motion_is_finished(void)
{
    return s_zigbee_finished;
}

