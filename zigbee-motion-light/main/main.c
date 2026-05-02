/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 *
 * Motion Light with Zigbee Time Schedule
 *
 * This example code is in the Public Domain (or CC0 licensed, at your option.)
 *
 * Unless required by applicable law or agreed to in writing, this
 * software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
 * CONDITIONS OF ANY KIND, either express or implied.
 */

#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_check.h"
#include "esp_task_wdt.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "nvs_flash.h"
#include "light_driver.h"
#include "motion_driver.h"
#include "time_schedule.h"
#include "esp_zigbee_core.h"
#include "ha/esp_zigbee_ha_standard.h"

static const char *TAG = "MOTION_LIGHT";

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

#define MOTION_LIGHT_ENDPOINT   10
#define ESP_MANUFACTURER_NAME   "\x09""ESPRESSIF"
#define ESP_MODEL_IDENTIFIER    "\x0D""Motion-Light"

/* Time sync re-sync interval: every 6 hours of deep sleep wakeups */
#define TIME_RESYNC_INTERVAL_SEC  (6 * 3600)

/* RTC memory - track total sleep time for periodic resync */
static RTC_DATA_ATTR int64_t s_accumulated_sleep_us = 0;
static RTC_DATA_ATTR int64_t s_last_sync_time_us = 0;

/* Motion tracking */
static int64_t s_last_motion_time_us = 0;
static bool s_last_occupancy_state = false;
static esp_zb_attribute_list_t *s_occupancy_cluster = NULL;

/* Event group for Zigbee time sync coordination */
static EventGroupHandle_t s_zb_events;
#define ZB_TIME_SYNCED_BIT  BIT0
#define ZB_CONNECTED_BIT    BIT1
#define ZB_SYNC_FAILED_BIT  BIT2

/* ───────────────────── Zigbee Callbacks ───────────────────── */

static void send_occupancy_report(bool occupied)
{
    uint8_t occupancy_value = occupied ? 1 : 0;
    esp_zb_zcl_set_attribute_val(MOTION_LIGHT_ENDPOINT, ESP_ZB_ZCL_CLUSTER_ID_OCCUPANCY_SENSING, 
                                  ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_OCCUPANCY_SENSING_OCCUPANCY_ID, 
                                  &occupancy_value, false);
    ESP_LOGI(TAG, "Updated occupancy attribute: %s", occupied ? "OCCUPIED" : "UNOCCUPIED");
}

static void bdb_start_top_level_commissioning_cb(uint8_t mode_mask)
{
    ESP_RETURN_ON_FALSE(esp_zb_bdb_start_top_level_commissioning(mode_mask) == ESP_OK, ,
                        TAG, "Failed to start Zigbee commissioning");
}

static void request_time_from_coordinator(void)
{
    /* Read Time attribute (0x0000) from coordinator (addr 0x0000, endpoint 1) */
    uint16_t attr_ids[] = {0x0000}; /* Time attribute */
    esp_zb_zcl_read_attr_cmd_t read_cmd = {
        .zcl_basic_cmd = {
            .dst_addr_u.addr_short = 0x0000,
            .dst_endpoint = 1,
            .src_endpoint = MOTION_LIGHT_ENDPOINT,
        },
        .address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT,
        .clusterID = ESP_ZB_ZCL_CLUSTER_ID_TIME,
        .attr_number = 1,
        .attr_field = attr_ids,
    };

    esp_zb_lock_acquire(portMAX_DELAY);
    esp_zb_zcl_read_attr_cmd_req(&read_cmd);
    esp_zb_lock_release();

    ESP_LOGI(TAG, "Sent time read request to coordinator");
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
                esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_STEERING);
            } else {
                ESP_LOGI(TAG, "Device rebooted, requesting time");
                xEventGroupSetBits(s_zb_events, ZB_CONNECTED_BIT);
                request_time_from_coordinator();
            }
        } else {
            ESP_LOGW(TAG, "Failed to initialize Zigbee stack (status: %s)",
                     esp_err_to_name(err_status));
            xEventGroupSetBits(s_zb_events, ZB_SYNC_FAILED_BIT);
        }
        break;

    case ESP_ZB_BDB_SIGNAL_STEERING:
        if (err_status == ESP_OK) {
            esp_zb_ieee_addr_t extended_pan_id;
            esp_zb_get_extended_pan_id(extended_pan_id);
            ESP_LOGI(TAG, "Joined network (PAN: 0x%04hx, Ch:%d, Addr: 0x%04hx)",
                     esp_zb_get_pan_id(), esp_zb_get_current_channel(),
                     esp_zb_get_short_address());
            xEventGroupSetBits(s_zb_events, ZB_CONNECTED_BIT);
            request_time_from_coordinator();
        } else {
            ESP_LOGW(TAG, "Network steering failed: %s", esp_err_to_name(err_status));
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
    if (callback_id == ESP_ZB_CORE_CMD_READ_ATTR_RESP_CB_ID) {
        esp_zb_zcl_cmd_read_attr_resp_message_t *resp = (esp_zb_zcl_cmd_read_attr_resp_message_t *)message;

        if (resp->info.cluster == ESP_ZB_ZCL_CLUSTER_ID_TIME && resp->info.status == ESP_ZB_ZCL_STATUS_SUCCESS) {
            esp_zb_zcl_read_attr_resp_variable_t *var = resp->variables;
            while (var) {
                if (var->attribute.id == 0x0000 && var->attribute.data.size == 4) {
                    uint32_t zigbee_time = *(uint32_t *)var->attribute.data.value;
                    ESP_LOGI(TAG, "Received Zigbee time: %lu", (unsigned long)zigbee_time);

                    if (time_schedule_sync_time(zigbee_time) == ESP_OK) {
                        xEventGroupSetBits(s_zb_events, ZB_TIME_SYNCED_BIT);
                    } else {
                        xEventGroupSetBits(s_zb_events, ZB_SYNC_FAILED_BIT);
                    }
                }
                var = var->next;
            }
        } else {
            ESP_LOGW(TAG, "Time read failed, cluster=0x%04x status=0x%02x",
                     resp->info.cluster, resp->info.status);
            xEventGroupSetBits(s_zb_events, ZB_SYNC_FAILED_BIT);
        }
    }
    return ESP_OK;
}

/* ───────────────────── Zigbee Task ───────────────────── */

static void esp_zb_task(void *pvParameters)
{
    esp_zb_cfg_t zb_nwk_cfg = ESP_ZB_ZED_CONFIG();
    esp_zb_init(&zb_nwk_cfg);

    /* Create cluster list with Time Cluster client for reading from coordinator */
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

    /* Time cluster (client role) - to read time from coordinator */
    esp_zb_attribute_list_t *time_cluster = esp_zb_zcl_attr_list_create(ESP_ZB_ZCL_CLUSTER_ID_TIME);
    esp_zb_cluster_list_add_time_cluster(cluster_list, time_cluster, ESP_ZB_ZCL_CLUSTER_CLIENT_ROLE);

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
    esp_zb_set_primary_network_channel_set(ESP_ZB_TRANSCEIVER_ALL_CHANNELS_MASK);

    ESP_ERROR_CHECK(esp_zb_start(false));
    esp_zb_stack_main_loop();
}

/* ───────────────────── LED Animation ───────────────────── */

static void led_active_animation(void)
{
    uint8_t r = 255;
    uint8_t g = 180;
    uint8_t b = 0;

    /* Simple single-LED bounce pattern (1 round) */
    for (int led = 0; led < CONFIG_EXAMPLE_STRIP_LED_NUMBER; led++) {
        light_driver_set_pixel(led, r, g, b);
        vTaskDelay(50 / portTICK_PERIOD_MS);
        light_driver_set_pixel(led, 0, 0, 0);
    }
    for (int led = CONFIG_EXAMPLE_STRIP_LED_NUMBER - 2; led > 0; led--) {
        light_driver_set_pixel(led, r, g, b);
        vTaskDelay(50 / portTICK_PERIOD_MS);
        light_driver_set_pixel(led, 0, 0, 0);
    }

    /* Keep LED 0 active */
    light_driver_set_pixel(0, r, g, b);
}

static void led_phase_out_animation(void)
{
    uint8_t r = 255;
    uint8_t g = 180;
    uint8_t b = 0;

    /* Fade out pattern with slower timing (100ms per step) */
    for (int led = 0; led < CONFIG_EXAMPLE_STRIP_LED_NUMBER; led++) {
        light_driver_set_pixel(led, r, g, b);
        vTaskDelay(100 / portTICK_PERIOD_MS);
        light_driver_set_pixel(led, 0, 0, 0);
    }

    /* Turn off all LEDs */
    light_driver_set_rgb(0, 0, 0);
}

/* ───────────────────── Deep Sleep ───────────────────── */

static void enter_deep_sleep(void)
{
    /* Turn LED off before deep sleep */
    light_driver_set_power(LIGHT_DEFAULT_OFF);

    /* Configure wake-up sources */
    motion_driver_configure_deep_sleep_wakeup();
    ESP_ERROR_CHECK(esp_sleep_enable_timer_wakeup(10000000)); /* 10s timer */

    ESP_LOGI(TAG, "Entering deep sleep...");
    esp_deep_sleep_start();
}

/* ───────────────────── Main ───────────────────── */

void app_main(void)
{
    ESP_LOGI(TAG, "Starting motion light with time schedule");

    /* Initialize NVS (required for Zigbee and time schedule) */
    ESP_ERROR_CHECK(nvs_flash_init());

    /* Initialize LED */
    light_driver_init();
    light_driver_set_rgb(0, 0, 0);  /* Clear all LEDs */
    light_driver_set_pixel(0, 255, 255, 255);  /* Set only LED 0 white */

    /* Initialize time schedule */
    ESP_ERROR_CHECK(time_schedule_init());

    /* Check wake-up reason */
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();

    const char *wakeup_str = (wakeup_reason == ESP_SLEEP_WAKEUP_GPIO) ? "GPIO" :
                             (wakeup_reason == ESP_SLEEP_WAKEUP_TIMER) ? "TIMER" :
                             (wakeup_reason == ESP_SLEEP_WAKEUP_UNDEFINED) ? "UNDEFINED" : "OTHER";
    ESP_LOGI(TAG, "Wakeup: %s, synced: %d", wakeup_str, time_schedule_is_time_synced());

    /* Initialize motion driver */
    motion_driver_init();

    /* Check if we need Zigbee time sync */
    bool need_time_sync = !time_schedule_is_time_synced();

    /* Also re-sync periodically */
    if (!need_time_sync) {
        s_accumulated_sleep_us += 10000000; /* approximate: 10s per timer wakeup */
        int64_t since_last_sync = s_accumulated_sleep_us - s_last_sync_time_us;
        if (since_last_sync > (int64_t)TIME_RESYNC_INTERVAL_SEC * 1000000LL) {
            ESP_LOGI(TAG, "Periodic time re-sync needed");
            need_time_sync = true;
        }
    }

    /* ── Start Zigbee if needed ── */
    if (need_time_sync || wakeup_reason == ESP_SLEEP_WAKEUP_GPIO) {
        ESP_LOGI(TAG, "Starting Zigbee...");
        light_driver_set_pixel(0, 0, 0, 255); /* Blue = initializing */
        vTaskDelay(200 / portTICK_PERIOD_MS); /* Brief blue flash */

        s_zb_events = xEventGroupCreate();

        esp_zb_platform_config_t config = {
            .radio_config = {.radio_mode = ZB_RADIO_MODE_NATIVE},
            .host_config = {.host_connection_mode = ZB_HOST_CONNECTION_MODE_NONE},
        };
        ESP_ERROR_CHECK(esp_zb_platform_config(&config));
        xTaskCreate(esp_zb_task, "Zigbee_main", 4096, NULL, 5, NULL);

        ESP_LOGI(TAG, "Zigbee started");
    }

    /* ── Check if animation should run ── */
    bool should_animate = false;
    if (wakeup_reason == ESP_SLEEP_WAKEUP_GPIO) {
        if (!time_schedule_is_time_synced() || time_schedule_is_active()) {
            should_animate = true;
            ESP_LOGI(TAG, "Motion detected -> animation will run");
        } else {
            ESP_LOGI(TAG, "Motion detected outside schedule -> no animation");
        }
    } else if (wakeup_reason == ESP_SLEEP_WAKEUP_TIMER) {
        ESP_LOGI(TAG, "Timer wakeup, no animation");
    } else {
        /* First boot - show red briefly */
        ESP_LOGI(TAG, "First boot");
        light_driver_set_pixel(0, 255, 0, 0);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }

    /* ── Animation loop ── */
    if (should_animate) {
        /* Send initial occupancy report if motion detected */
        bool motion_detected = motion_driver_get_state();
        if (motion_detected && s_last_occupancy_state != true) {
            send_occupancy_report(true);
            s_last_occupancy_state = true;
        }

        /* Animation loop: run animation, check motion at end, loop if still motion */
        do {
            led_active_animation();

            /* Check motion at end of animation */
            motion_detected = motion_driver_get_state();
            if (motion_detected) {
                s_last_motion_time_us = esp_timer_get_time();
                ESP_LOGI(TAG, "Motion still detected, looping animation");
                /* Motion still detected, loop again */
            } else {
                /* No motion, send false and phase out */
                ESP_LOGI(TAG, "No motion detected, starting phase out");
                if (s_last_occupancy_state != false) {
                    send_occupancy_report(false);
                    s_last_occupancy_state = false;
                }
                led_phase_out_animation();
                break;
            }
        } while (motion_detected);
    }

    /* ── Wait for Zigbee sync (30 seconds from last motion) ── */
    if (need_time_sync) {
        int64_t time_since_motion = s_last_motion_time_us > 0 ? 
                                     (esp_timer_get_time() - s_last_motion_time_us) : 0;
        int64_t remaining_wait = 30000000 - time_since_motion; /* 30 seconds */

        if (remaining_wait > 0) {
            ESP_LOGI(TAG, "Waiting up to %lld ms for Zigbee sync", remaining_wait / 1000);

            /* Wait for sync or timeout */
            EventBits_t bits = xEventGroupWaitBits(s_zb_events,
                                                     ZB_TIME_SYNCED_BIT | ZB_SYNC_FAILED_BIT,
                                                     pdFALSE,
                                                     pdFALSE,
                                                     remaining_wait / portTICK_PERIOD_MS);

            if (bits & ZB_TIME_SYNCED_BIT) {
                ESP_LOGI(TAG, "Zigbee time sync completed successfully");
                s_last_sync_time_us = esp_timer_get_time();
                s_accumulated_sleep_us = 0;
            } else if (bits & ZB_SYNC_FAILED_BIT) {
                ESP_LOGW(TAG, "Zigbee sync failed");
            } else {
                ESP_LOGW(TAG, "Zigbee sync timeout");
            }
        }
    }

    ESP_LOGI(TAG, "Wakeup: %s, synced: %d", wakeup_str, time_schedule_is_time_synced());

    /* Enter deep sleep */
    enter_deep_sleep();
}
