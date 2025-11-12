/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include "scd40_driver.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "SCD40_DRIVER";

/* SCD40 I2C Address */
#define SCD40_SENSOR_ADDR           0x62

/* SCD40 Commands */
#define SCD40_CMD_START_PERIODIC_MEASUREMENT    0x21B1
#define SCD40_CMD_STOP_PERIODIC_MEASUREMENT     0x3F86
#define SCD40_CMD_READ_MEASUREMENT              0xEC05
#define SCD40_CMD_GET_DATA_READY_STATUS         0xE4B8
#define SCD40_CMD_GET_SERIAL_NUMBER             0x3682
#define SCD40_CMD_REINIT                        0x3646

/* Timeout for I2C operations */
#define I2C_TIMEOUT_MS              10000

/**
 * @brief Calculate CRC8 checksum for SCD40 data validation
 *
 * Polynomial: 0x31 (x^8 + x^5 + x^4 + 1)
 * Initialization: 0xFF
 */
static uint8_t scd40_calculate_crc(const uint8_t *data, size_t len)
{
    uint8_t crc = 0xFF;

    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t bit = 0; bit < 8; bit++) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ 0x31;
            } else {
                crc = crc << 1;
            }
        }
    }

    return crc;
}

/**
 * @brief Send command to SCD40 sensor
 */
static esp_err_t scd40_send_command(i2c_master_dev_handle_t dev_handle, uint16_t cmd)
{
    uint8_t cmd_buf[2] = {(uint8_t)(cmd >> 8), (uint8_t)(cmd & 0xFF)};
    return i2c_master_transmit(dev_handle, cmd_buf, sizeof(cmd_buf), I2C_TIMEOUT_MS);
}

/**
 * @brief Read data from SCD40 with CRC validation
 *
 * SCD40 sends data in groups of 2 bytes + 1 CRC byte
 */
static esp_err_t scd40_read_data_with_crc(i2c_master_dev_handle_t dev_handle, uint16_t *values, size_t num_words)
{
    uint8_t buf[9]; // Maximum 3 words (6 bytes) + 3 CRC bytes
    size_t buf_len = num_words * 3; // Each word is 2 bytes + 1 CRC

    if (buf_len > sizeof(buf)) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = i2c_master_receive(dev_handle, buf, buf_len, I2C_TIMEOUT_MS);
    if (ret != ESP_OK) {
        return ret;
    }

    // Validate CRC for each word
    for (size_t i = 0; i < num_words; i++) {
        uint8_t *word_data = &buf[i * 3];
        uint8_t crc = scd40_calculate_crc(word_data, 2);

        if (crc != word_data[2]) {
            ESP_LOGE(TAG, "CRC error for word %zu: expected 0x%02X, got 0x%02X", i, crc, word_data[2]);
            return ESP_ERR_INVALID_CRC;
        }

        values[i] = (word_data[0] << 8) | word_data[1];
    }

    return ESP_OK;
}

esp_err_t scd40_init(const scd40_config_t *config, scd40_handle_t *handle)
{
    if (config == NULL || handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret;

    // Configure I2C master bus
    i2c_master_bus_config_t bus_config = {
        .i2c_port = config->i2c_port,
        .sda_io_num = config->sda_io_num,
        .scl_io_num = config->scl_io_num,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = false,
    };

    ret = i2c_new_master_bus(&bus_config, &handle->bus_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create I2C bus: %s", esp_err_to_name(ret));
        return ret;
    }

    // Add SCD40 device to the bus
    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = SCD40_SENSOR_ADDR,
        .scl_speed_hz = config->i2c_freq_hz,
    };

    ret = i2c_master_bus_add_device(handle->bus_handle, &dev_config, &handle->dev_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add I2C device: %s", esp_err_to_name(ret));
        i2c_del_master_bus(handle->bus_handle);
        return ret;
    }

    ESP_LOGI(TAG, "SCD40 driver initialized successfully");
    return ESP_OK;
}

esp_err_t scd40_deinit(scd40_handle_t *handle)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = ESP_OK;

    if (handle->dev_handle != NULL) {
        ret = i2c_master_bus_rm_device(handle->dev_handle);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to remove I2C device: %s", esp_err_to_name(ret));
        }
    }

    if (handle->bus_handle != NULL) {
        ret = i2c_del_master_bus(handle->bus_handle);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to delete I2C bus: %s", esp_err_to_name(ret));
        }
    }

    memset(handle, 0, sizeof(scd40_handle_t));
    ESP_LOGI(TAG, "SCD40 driver deinitialized");
    return ret;
}

esp_err_t scd40_get_serial_number(scd40_handle_t *handle, uint64_t *serial)
{
    if (handle == NULL || serial == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret;
    uint16_t serial_words[3];

    // Send get serial number command
    ret = scd40_send_command(handle->dev_handle, SCD40_CMD_GET_SERIAL_NUMBER);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send get serial number command: %s", esp_err_to_name(ret));
        return ret;
    }

    // Wait for sensor to process command (1ms)
    vTaskDelay(pdMS_TO_TICKS(1));

    // Read 3 words (48 bits) with CRC
    ret = scd40_read_data_with_crc(handle->dev_handle, serial_words, 3);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read serial number: %s", esp_err_to_name(ret));
        return ret;
    }

    // Combine 3 words into 48-bit serial number
    *serial = ((uint64_t)serial_words[0] << 32) | ((uint64_t)serial_words[1] << 16) | serial_words[2];

    return ESP_OK;
}

esp_err_t scd40_start_periodic_measurement(scd40_handle_t *handle)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = scd40_send_command(handle->dev_handle, SCD40_CMD_START_PERIODIC_MEASUREMENT);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start periodic measurement: %s", esp_err_to_name(ret));
    }
    return ret;
}

esp_err_t scd40_stop_periodic_measurement(scd40_handle_t *handle)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = scd40_send_command(handle->dev_handle, SCD40_CMD_STOP_PERIODIC_MEASUREMENT);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to stop periodic measurement: %s", esp_err_to_name(ret));
        return ret;
    }

    // Wait for sensor to stop measurements (500ms as per datasheet)
    vTaskDelay(pdMS_TO_TICKS(500));
    return ESP_OK;
}

esp_err_t scd40_reinit(scd40_handle_t *handle)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = scd40_send_command(handle->dev_handle, SCD40_CMD_REINIT);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to reinitialize sensor: %s", esp_err_to_name(ret));
        return ret;
    }

    // Wait for sensor to reinitialize (20ms as per datasheet)
    vTaskDelay(pdMS_TO_TICKS(20));
    return ESP_OK;
}

esp_err_t scd40_get_data_ready_status(scd40_handle_t *handle, bool *data_ready)
{
    if (handle == NULL || data_ready == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret;
    uint16_t status;

    // Send get data ready status command
    ret = scd40_send_command(handle->dev_handle, SCD40_CMD_GET_DATA_READY_STATUS);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send get data ready status command: %s", esp_err_to_name(ret));
        return ret;
    }

    // Wait for sensor response (1ms)
    vTaskDelay(pdMS_TO_TICKS(1));

    // Read status word (2 bytes + 1 CRC)
    ret = scd40_read_data_with_crc(handle->dev_handle, &status, 1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read data ready status: %s", esp_err_to_name(ret));
        return ret;
    }

    // According to datasheet: bit 11 (0x0800) indicates data is ready
    // Lower 11 bits are 0 when no data is available
    *data_ready = ((status & 0x07FF) != 0);

    return ESP_OK;
}

esp_err_t scd40_read_measurement(scd40_handle_t *handle, scd40_measurement_t *measurement)
{
    if (handle == NULL || measurement == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret;
    uint16_t data[3];

    // Send read measurement command
    ret = scd40_send_command(handle->dev_handle, SCD40_CMD_READ_MEASUREMENT);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send read measurement command: %s", esp_err_to_name(ret));
        return ret;
    }

    // Wait for sensor response (1ms)
    vTaskDelay(pdMS_TO_TICKS(1));

    // Read 3 words: CO2, Temperature, Humidity
    ret = scd40_read_data_with_crc(handle->dev_handle, data, 3);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read measurement data: %s", esp_err_to_name(ret));
        return ret;
    }

    // Convert raw values according to SCD40 datasheet
    measurement->co2_ppm = data[0];                                         // CO2 in ppm
    measurement->temperature_c = -45.0f + 175.0f * data[1] / 65536.0f;     // Temperature in °C
    measurement->humidity_rh = 100.0f * data[2] / 65536.0f;                // Humidity in %

    return ESP_OK;
}
