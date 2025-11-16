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
#include <stdio.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "scd40.h"

static const char *TAG = "ZIGBEE_CO2_SENSOR";

/* I2C Configuration */
#define I2C_MASTER_NUM              I2C_NUM_0
#define I2C_MASTER_SCL_IO           6       /*!< GPIO number used for I2C master clock */
#define I2C_MASTER_SDA_IO           7       /*!< GPIO number used for I2C master data  */
#define I2C_MASTER_FREQ_HZ          40000   /*!< I2C master clock frequency */

/* Retry Configuration */
#define MAX_RETRIES                 3
#define RETRY_DELAY_MS              1000

/* Global sensor handle */
static scd40_handle_t g_sensor;

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

    // Stop any ongoing periodic measurements (non-critical)
    ret = scd40_stop_periodic_measurement(&g_sensor);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Stop periodic measurement returned: %s (may not have been running)", 
                 esp_err_to_name(ret));
        // Continue anyway - not critical
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
        // Continue anyway - not critical for operation
    }

    return ESP_OK;
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

    // Trigger single-shot measurement with retries
    for (int attempt = 1; attempt <= MAX_RETRIES; attempt++) {
        ret = scd40_measure_single_shot(&g_sensor);
        if (ret == ESP_OK) {
            break;
        }
        
        ESP_LOGW(TAG, "Trigger measurement attempt %d/%d failed: %s", 
                 attempt, MAX_RETRIES, esp_err_to_name(ret));
        
        if (attempt < MAX_RETRIES) {
            vTaskDelay(pdMS_TO_TICKS(RETRY_DELAY_MS));
        } else {
            ESP_LOGE(TAG, "Failed to trigger measurement after %d attempts", MAX_RETRIES);
            return ret;
        }
    }

    // Wait for measurement to complete (5 seconds as per datasheet)
    vTaskDelay(pdMS_TO_TICKS(5000));

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
    scd40_deinit(&g_sensor);
    ESP_LOGI(TAG, "Sensor cleanup complete");
}

/**
 * @brief Main sensor task that periodically takes measurements
 * 
 * @param args Task arguments (unused)
 */
static void sensor_task(void *args)
{
    scd40_measurement_t measurement;
    
    while (true) {
        // Get sensor data
        esp_err_t ret = sensor_get_data(&measurement);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to get sensor data, continuing...");
        }

        // Wait 5 seconds before next measurement
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

void app_main(void)
{
    // Initialize sensor
    esp_err_t ret = sensor_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Sensor initialization failed, terminating application");
        return;
    }

    // Create sensor task
    xTaskCreate(sensor_task, "sensor_task", 4096, NULL, 6, NULL);
}
