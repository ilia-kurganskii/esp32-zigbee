/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
/* Zigbee CO2 Sensor with SCD40
 *
 * This example demonstrates how to create a Zigbee CO2 sensor device
 * using the SCD40/SCD41 sensor connected over I2C.
 */
#include <stdbool.h>
#include <stdio.h>
#include "esp_err.h"
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_check.h"
#include "nvs_flash.h"
#include "esp_zigbee_core.h"
#include "ha/esp_zigbee_ha_standard.h"
#include "scd40.h"
#include "esp_sleep.h"
#include "esp_timer.h"
#include "driver/rtc_io.h"
#include <sys/time.h>
#include "led_signal.h"

static const char *TAG = "ZIGBEE_CO2_SENSOR";

/* Zigbee Configuration */
#define ESP_ZB_ZED_CONFIG()                               \
    {                                                     \
        .esp_zb_role = ESP_ZB_DEVICE_TYPE_ED,            \
        .install_code_policy = false,                     \
        .nwk_cfg.zed_cfg = {                             \
            .ed_timeout = ESP_ZB_ED_AGING_TIMEOUT_64MIN, \
            .keep_alive = 3000,                           \
        },                                                \
    }


#define HA_ESP_SENSOR_ENDPOINT      1
#define ESP_MANUFACTURER_NAME       "\x0B""Espressif"
#define ESP_MODEL_IDENTIFIER        "\x0A""CO2-Sensor"

/* I2C Configuration */
#define I2C_MASTER_NUM              I2C_NUM_0
#define I2C_MASTER_SCL_IO           23       /*!< GPIO number used for I2C master clock */
#define I2C_MASTER_SDA_IO           22       /*!< GPIO number used for I2C master data  */
#define I2C_MASTER_FREQ_HZ          40000   /*!< I2C master clock frequency */

/* Retry Configuration */
#define MAX_RETRIES                 3
#define RETRY_DELAY_MS              1000

#define WAKE_UP_TIME_SEC            60

/* Global sensor handle */
static scd40_handle_t g_sensor;

/* Deep sleep variables */
static RTC_DATA_ATTR struct timeval s_sleep_enter_time;
static esp_timer_handle_t s_oneshot_timer;

/********************* Deep Sleep Functions *********************/

/**
 * @brief One-shot timer callback to enter deep sleep
 */
static void s_oneshot_timer_callback(void *arg)
{
    ESP_LOGI(TAG, "Enter deep sleep");
    led_signal_set_state(LED_STATE_DEEP_SLEEP_PREPARE);
    vTaskDelay(pdMS_TO_TICKS(1000)); // Show LED pattern before sleep
    led_signal_stop();
    gettimeofday(&s_sleep_enter_time, NULL);
    esp_deep_sleep_start();
}

/**
 * @brief Initialize deep sleep configuration
 */
static void zb_deep_sleep_init(void)
{
    // Create one-shot timer for deep sleep entry
    const esp_timer_create_args_t s_oneshot_timer_args = {
        .callback = &s_oneshot_timer_callback,
        .name = "deep-sleep-timer"
    };
    ESP_ERROR_CHECK(esp_timer_create(&s_oneshot_timer_args, &s_oneshot_timer));

    // Print wake-up reason
    struct timeval now;
    gettimeofday(&now, NULL);
    int sleep_time_ms = (now.tv_sec - s_sleep_enter_time.tv_sec) * 1000 +
                        (now.tv_usec - s_sleep_enter_time.tv_usec) / 1000;

    esp_sleep_wakeup_cause_t wake_up_cause = esp_sleep_get_wakeup_cause();
    switch (wake_up_cause) {
    case ESP_SLEEP_WAKEUP_TIMER:
        ESP_LOGI(TAG, "Wake up from timer. Time spent in deep sleep and boot: %dms", sleep_time_ms);
        break;
    case ESP_SLEEP_WAKEUP_UNDEFINED:
    default:
        ESP_LOGI(TAG, "Not a deep sleep reset");
        break;
    }

    // Configure RTC timer wake-up for x seconds
    ESP_LOGI(TAG, "Enabling timer wakeup, %ds", WAKE_UP_TIME_SEC);
    ESP_ERROR_CHECK(esp_sleep_enable_timer_wakeup(WAKE_UP_TIME_SEC * 1000000));
}

/**
 * @brief Check if conditions are met for deep sleep and trigger if ready
 */
static void go_to_deep_sleep()
{
    const int before_deep_sleep_time_sec = 10;
    ESP_LOGI(TAG, "Starting one-shot timer for %ds before entering deep sleep", before_deep_sleep_time_sec);
    ESP_ERROR_CHECK(esp_timer_start_once(s_oneshot_timer, before_deep_sleep_time_sec * 1000000));
}



/********************* Sensor Functions *********************/

/**
 * @brief Initialize the SCD40 sensor and read serial number
 *
 * @return ESP_OK on success, error code otherwise
 */
static esp_err_t sensor_init(void)
{
    esp_err_t ret;

    ESP_LOGI(TAG, "Initializing SCD40 CO2 sensor...");

    // Configure SCD40 sensor
    scd40_config_t sensor_config = {
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .i2c_freq_hz = I2C_MASTER_FREQ_HZ,
        .i2c_port = I2C_MASTER_NUM,
    };

    // Initialize sensor with retries
    for (int attempt = 1; attempt <= MAX_RETRIES; attempt++) {
        ret = scd40_init(&sensor_config, &g_sensor);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "Sensor initialized successfully");
            break;
        }

        ESP_LOGW(TAG, "Initialization attempt %d/%d failed: %s",
                 attempt, MAX_RETRIES, esp_err_to_name(ret));

        if (attempt < MAX_RETRIES) {
            vTaskDelay(pdMS_TO_TICKS(RETRY_DELAY_MS));
        } else {
            ESP_LOGE(TAG, "Failed to initialize sensor after %d attempts", MAX_RETRIES);
            return ret;
        }
    }

    ret = scd40_wake_up(&g_sensor);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Wake up returned: %s", esp_err_to_name(ret));
    }

    // Stop any ongoing periodic measurements (non-critical)
    ret = scd40_stop_periodic_measurement(&g_sensor);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Stop periodic measurement returned: %s (may not have been running)",
                 esp_err_to_name(ret));
    }

    // Small delay to ensure sensor is ready
    vTaskDelay(pdMS_TO_TICKS(100));

    // Get and display serial number (non-critical)
    uint64_t serial_number = 0;
    ret = scd40_get_serial_number(&g_sensor, &serial_number);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "SCD40 Serial Number: %04X-%04X-%04X",
                 (uint16_t)(serial_number >> 32),
                 (uint16_t)(serial_number >> 16),
                 (uint16_t)(serial_number & 0xFFFF));
    } else {
        ESP_LOGW(TAG, "Failed to read serial number: %s (continuing anyway)",
                 esp_err_to_name(ret));
    }

    return ESP_OK;
}

static esp_err_t start_measure_single_shot(void)
{
    esp_err_t ret;

    // Start periodic measurement with retries
    for (int attempt = 1; attempt <= MAX_RETRIES; attempt++) {
        ret = scd40_measure_single_shot(&g_sensor);
        if (ret == ESP_OK) {
            break;
        }

        ESP_LOGW(TAG, "Start measurement single shot attempt %d/%d failed: %s",
                 attempt, MAX_RETRIES, esp_err_to_name(ret));
    }

    return ret;
}
/**
 * @brief Get CO2, temperature, and humidity data from sensor
 *
 * @param data Pointer to measurement structure to fill
 * @return ESP_OK on success, error code otherwise
 */
static esp_err_t sensor_get_data(scd40_measurement_t *data)
{
    esp_err_t ret;

    if (data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    bool data_ready = false;

    // Wait for data ready status with retries
    for (int attempt = 1; attempt <= MAX_RETRIES; attempt++) {
        ret = scd40_get_data_ready_status(&g_sensor, &data_ready);
        if (ret == ESP_OK && data_ready) {
            break;
        }
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Wait for data ready status attempt %d/%d failed: %s",
                     attempt, MAX_RETRIES, esp_err_to_name(ret));
        }
        if (!data_ready) {
            ESP_LOGW(TAG, "Data not ready %d/%d", attempt, MAX_RETRIES);
        }

        vTaskDelay(pdMS_TO_TICKS(RETRY_DELAY_MS*3));
    }

    // Read measurement with retries
    for (int attempt = 1; attempt <= MAX_RETRIES; attempt++) {
        ret = scd40_read_measurement(&g_sensor, data);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "CO2: %d ppm | Temperature: %.2f °C | Humidity: %.2f %%",
                     data->co2_ppm, data->temperature_c, data->humidity_rh);
            return ESP_OK;
        }

        ESP_LOGW(TAG, "Read measurement attempt %d/%d failed: %s",
                 attempt, MAX_RETRIES, esp_err_to_name(ret));

        if (attempt < MAX_RETRIES) {
            vTaskDelay(pdMS_TO_TICKS(RETRY_DELAY_MS));
        }
    }

    ESP_LOGE(TAG, "Failed to read measurement after %d attempts", MAX_RETRIES);
    return ret;
}

/**
 * @brief Cleanup sensor resources
 */
static void sensor_cleanup(void)
{
    ESP_LOGI(TAG, "Cleaning up sensor resources...");
    esp_err_t ret = scd40_power_down(&g_sensor);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to power down sensor: %s", esp_err_to_name(ret));
    }
    scd40_deinit(&g_sensor);
    ESP_LOGI(TAG, "Sensor cleanup complete");
}

/********************* Zigbee Functions *********************/

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
        led_signal_set_state(LED_STATE_INITIALIZING);
        esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_INITIALIZATION);
        break;

    case ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START:
    case ESP_ZB_BDB_SIGNAL_DEVICE_REBOOT:
        if (err_status == ESP_OK) {
            ESP_LOGI(TAG, "Device started up in%s factory-reset mode",
                     esp_zb_bdb_is_factory_new() ? "" : " non");
            if (esp_zb_bdb_is_factory_new()) {
                ESP_LOGI(TAG, "Start network steering");
                led_signal_set_state(LED_STATE_JOINING);
                esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_STEERING);
            } else {
                ESP_LOGI(TAG, "Device rebooted");
                led_signal_set_state(LED_STATE_CONNECTED);
                go_to_deep_sleep();
            }
        } else {
            ESP_LOGW(TAG, "Failed to initialize Zigbee stack (status: %s)",
                     esp_err_to_name(err_status));
            led_signal_set_state(LED_STATE_ERROR);
            esp_zb_scheduler_alarm((esp_zb_callback_t)bdb_start_top_level_commissioning_cb,
                                   ESP_ZB_BDB_MODE_INITIALIZATION, 1000);
        }
        break;
    case ESP_ZB_BDB_SIGNAL_STEERING:
        if (err_status == ESP_OK) {
            esp_zb_ieee_addr_t extended_pan_id;
            esp_zb_get_extended_pan_id(extended_pan_id);
            ESP_LOGI(TAG, "Joined network successfully (Extended PAN ID: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x, PAN ID: 0x%04hx, Channel:%d, Short Address: 0x%04hx)",
                     extended_pan_id[7], extended_pan_id[6], extended_pan_id[5], extended_pan_id[4],
                     extended_pan_id[3], extended_pan_id[2], extended_pan_id[1], extended_pan_id[0],
                     esp_zb_get_pan_id(), esp_zb_get_current_channel(), esp_zb_get_short_address());

            led_signal_set_state(LED_STATE_CONNECTED);
            go_to_deep_sleep();
        } else {
            ESP_LOGI(TAG, "Network steering was not successful (status: %s)",
                     esp_err_to_name(err_status));
            led_signal_set_state(LED_STATE_ERROR);
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
    esp_err_t ret = ESP_OK;
    switch (callback_id) {
    case ESP_ZB_CORE_SET_ATTR_VALUE_CB_ID:
        // Handle attribute writes if needed in the future
        ESP_LOGI(TAG, "Received attribute callback");
        break;
    default:
        ESP_LOGW(TAG, "Receive Zigbee action(0x%x) callback", callback_id);
        break;
    }
    return ret;
}

static void create_zigbee_sensor_device(const scd40_measurement_t *data)
{
    // Convert sensor measurements to Zigbee attribute format
    int16_t temp_value = (int16_t)(data->temperature_c * 100);      // Zigbee uses 0.01°C units
    uint16_t humidity_value = (uint16_t)(data->humidity_rh * 100);  // Zigbee uses 0.01% RH units
    float co2_value = (float)data->co2_ppm / 1000000.0f;            // Zigbee uses fraction (ppm/1000000)

    ESP_LOGI(TAG, "Creating Zigbee sensor device with:");
    ESP_LOGI(TAG, "  Temperature: %.2f°C (raw: %d)", data->temperature_c, temp_value);
    ESP_LOGI(TAG, "  Humidity: %.2f%% (raw: %u)", data->humidity_rh, humidity_value);
    ESP_LOGI(TAG, "  CO2: %d ppm (raw: %.6f)", data->co2_ppm, co2_value);

    // Create temperature measurement cluster with actual sensor data
    esp_zb_temperature_meas_cluster_cfg_t temp_cfg = {
        .measured_value = temp_value,
        .min_value = -5000,   // -50°C
        .max_value = 10000,   // 100°C
    };
    esp_zb_attribute_list_t *temp_cluster = esp_zb_temperature_meas_cluster_create(&temp_cfg);

    // Create humidity measurement cluster with actual sensor data
    esp_zb_humidity_meas_cluster_cfg_t humidity_cfg = {
        .measured_value = humidity_value,
        .min_value = 0,
        .max_value = 10000,   // 100% RH
    };
    esp_zb_attribute_list_t *humidity_cluster = esp_zb_humidity_meas_cluster_create(&humidity_cfg);

    // Create CO2 measurement cluster with actual sensor data
    esp_zb_carbon_dioxide_measurement_cluster_cfg_t co2_cfg = {
        .measured_value = co2_value,
        .min_measured_value = 0.0f,      // 0 ppm
        .max_measured_value = 0.01f,     // 10,000 ppm (1%)
    };
    esp_zb_attribute_list_t *co2_cluster = esp_zb_carbon_dioxide_measurement_cluster_create(&co2_cfg);

    // Create basic cluster with manufacturer info
    esp_zb_basic_cluster_cfg_t basic_cfg = {
        .zcl_version = ESP_ZB_ZCL_BASIC_ZCL_VERSION_DEFAULT_VALUE,
        .power_source = ESP_ZB_ZCL_BASIC_POWER_SOURCE_DEFAULT_VALUE,
    };
    esp_zb_attribute_list_t *basic_cluster = esp_zb_basic_cluster_create(&basic_cfg);
    esp_zb_basic_cluster_add_attr(basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID,
                                   (void *)ESP_MANUFACTURER_NAME);
    esp_zb_basic_cluster_add_attr(basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID,
                                   (void *)ESP_MODEL_IDENTIFIER);

    // Create identify cluster
    esp_zb_identify_cluster_cfg_t identify_cfg = {
        .identify_time = 0,
    };
    esp_zb_attribute_list_t *identify_cluster = esp_zb_identify_cluster_create(&identify_cfg);

    // Create cluster list and add all clusters
    esp_zb_cluster_list_t *cluster_list = esp_zb_zcl_cluster_list_create();
    esp_zb_cluster_list_add_basic_cluster(cluster_list, basic_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_identify_cluster(cluster_list, identify_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_temperature_meas_cluster(cluster_list, temp_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_humidity_meas_cluster(cluster_list, humidity_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_carbon_dioxide_measurement_cluster(cluster_list, co2_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);

    // Create endpoint and add cluster list
    esp_zb_ep_list_t *ep_list = esp_zb_ep_list_create();
    esp_zb_endpoint_config_t endpoint_config = {
        .endpoint = HA_ESP_SENSOR_ENDPOINT,
        .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID,
        .app_device_id = ESP_ZB_HA_TEMPERATURE_SENSOR_DEVICE_ID,
        .app_device_version = 0
    };
    esp_zb_ep_list_add_ep(ep_list, cluster_list, endpoint_config);

    // Register the device
    esp_zb_device_register(ep_list);
    esp_zb_core_action_handler_register(zb_action_handler);

    ESP_LOGI(TAG, "Zigbee multi-sensor device created with all 3 measurements");
}


/**
 * @brief Main sensor task that periodically takes measurements
 *
 * @param args Task arguments (unused)
 */
static void sensor_task(void *args)
{
    scd40_measurement_t measurement;
    esp_err_t ret;


    while (true) {
        led_signal_set_state(LED_STATE_SENSOR_READING);

        ret = start_measure_single_shot();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to start periodic measurement");
            led_signal_set_state(LED_STATE_ERROR);
            vTaskDelay(pdMS_TO_TICKS(5000));
            continue;
        } else {
            ESP_LOGI(TAG, "Periodic measurement started");
        }


        // Get sensor data
        ret = sensor_get_data(&measurement);
        if (ret == ESP_OK && measurement.co2_ppm > 0) {
            // Update Zigbee attributes with new sensor data
            led_signal_set_state(LED_STATE_COMMAND_RECEIVED);
            break;
        } else {
            ESP_LOGW(TAG, "Failed to get sensor data, continuing...");
            led_signal_set_state(LED_STATE_ERROR);
            vTaskDelay(pdMS_TO_TICKS(5000));
            continue;
        }
    }

    // Wait 5 seconds before next measurement
    ESP_LOGI(TAG, "Measurement completed");

    sensor_cleanup();
    // Initialize Zigbee stack
    esp_zb_cfg_t zb_nwk_cfg = ESP_ZB_ZED_CONFIG();
    esp_zb_init(&zb_nwk_cfg);

    // Create Zigbee device
    create_zigbee_sensor_device(&measurement);

    // Start Zigbee stack
    ESP_ERROR_CHECK(esp_zb_start(false));

    // Main Zigbee loop
    esp_zb_stack_main_loop();

}

/********************* Main Function *********************/

void app_main(void)
{
    // Initialize NVS
    ESP_ERROR_CHECK(nvs_flash_init());

    // Initialize LED signal
    led_signal_init();

    // Initialize deep sleep configuration
    zb_deep_sleep_init();

    esp_err_t ret = sensor_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Sensor initialization failed, terminating application");
        led_signal_set_state(LED_STATE_ERROR);
        return;
    }

    // Initialize Zigbee platform
    esp_zb_platform_config_t config = {
        .radio_config = {.radio_mode = ZB_RADIO_MODE_NATIVE},
        .host_config = {.host_connection_mode = ZB_HOST_CONNECTION_MODE_NONE},
    };
    ESP_ERROR_CHECK(esp_zb_platform_config(&config));

    // Create sensor task
    xTaskCreate(sensor_task, "sensor_task", 4096, NULL, 6, NULL);
}
