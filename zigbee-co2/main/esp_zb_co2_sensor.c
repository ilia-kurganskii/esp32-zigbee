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
#include "scd40_driver.h"

static const char *TAG = "ZIGBEE_CO2_SENSOR";

/* I2C Configuration */
#define I2C_MASTER_NUM              I2C_NUM_0
#define I2C_MASTER_SCL_IO           6       /*!< GPIO number used for I2C master clock */
#define I2C_MASTER_SDA_IO           7       /*!< GPIO number used for I2C master data  */
#define I2C_MASTER_FREQ_HZ          40000   /*!< I2C master clock frequency */



void app_main(void)
{
    scd40_handle_t sensor;
    scd40_measurement_t measurement;
    esp_err_t ret;

    ESP_LOGI(TAG, "Initializing SCD40 CO2 sensor...");

    // Configure SCD40 sensor
    scd40_config_t sensor_config = {
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .i2c_freq_hz = I2C_MASTER_FREQ_HZ,
        .i2c_port = I2C_MASTER_NUM,
    };

    // Initialize sensor
    ret = scd40_init(&sensor_config, &sensor);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SCD40: %s", esp_err_to_name(ret));
        return;
    }

    // Stop any ongoing measurements to ensure we can read serial
    ESP_LOGI(TAG, "Stopping any ongoing measurements...");
    ret = scd40_stop_periodic_measurement(&sensor);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Stop measurement returned: %s (may not have been running)", esp_err_to_name(ret));
    }

    // Wait to ensure sensor is ready for serial read
    ESP_LOGI(TAG, "Checking sensor readiness...");
    bool data_ready = false;
    int ready_attempts = 0;
    while (!data_ready && ready_attempts < 10) {
        ret = scd40_get_data_ready_status(&sensor, &data_ready);
        if (ret == ESP_OK && !data_ready) {
            ESP_LOGD(TAG, "Sensor initializing, attempt %d/10...", ready_attempts + 1);
            vTaskDelay(pdMS_TO_TICKS(500));
            ready_attempts++;
        } else if (ret != ESP_OK) {
            ESP_LOGD(TAG, "Data ready check returned: %s (expected during init)", esp_err_to_name(ret));
            break; // Exit loop, sensor is likely ready
        } else {
            break; // Data is ready
        }
    }

    // Get and display serial number
    ESP_LOGI(TAG, "Reading sensor serial number...");
    uint64_t serial_number = 0;
    ret = scd40_get_serial_number(&sensor, &serial_number);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read serial number: %s", esp_err_to_name(ret));
        goto cleanup;
    }

    ESP_LOGI(TAG, "SCD40 Serial Number: %04X-%04X-%04X",
             (uint16_t)(serial_number >> 32),
             (uint16_t)(serial_number >> 16),
             (uint16_t)(serial_number & 0xFFFF));

    // Start periodic measurement
    ret = scd40_start_periodic_measurement(&sensor);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start periodic measurement: %s", esp_err_to_name(ret));
        goto cleanup;
    }
    ESP_LOGI(TAG, "Periodic measurement started");

    // Wait for first measurement to be available
    ESP_LOGI(TAG, "Waiting for first measurement...");
    data_ready = false;
    while (!data_ready) {
        ret = scd40_get_data_ready_status(&sensor, &data_ready);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to check data ready status: %s", esp_err_to_name(ret));
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        if (!data_ready) {
            ESP_LOGD(TAG, "Data not ready yet, waiting...");
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
    ESP_LOGI(TAG, "First measurement ready!");

    // Continuous measurement reading loop
    while (1) {
        data_ready = false;

        // Check if new data is available
        ret = scd40_get_data_ready_status(&sensor, &data_ready);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to check data ready status: %s", esp_err_to_name(ret));
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        // Only read measurement if data is ready
        if (data_ready) {
            ret = scd40_read_measurement(&sensor, &measurement);

            if (ret == ESP_OK) {
                ESP_LOGI(TAG, "CO2: %d ppm | Temperature: %.2f °C | Humidity: %.2f %%",
                         measurement.co2_ppm,
                         measurement.temperature_c,
                         measurement.humidity_rh);
            } else {
                ESP_LOGE(TAG, "Failed to read measurement: %s", esp_err_to_name(ret));
            }
        } else {
            ESP_LOGD(TAG, "No new data available, waiting...");
        }

        // Check for new data every second
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

cleanup:
    scd40_deinit(&sensor);
    ESP_LOGI(TAG, "Application terminated");
}