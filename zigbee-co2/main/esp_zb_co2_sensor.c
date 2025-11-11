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
#include "driver/i2c_master.h"

static const char *TAG = "SCD40_CO2";

#define I2C_MASTER_NUM              I2C_NUM_0  
#define I2C_MASTER_SCL_IO           6       /*!< GPIO number used for I2C master clock */
#define I2C_MASTER_SDA_IO           7       /*!< GPIO number used for I2C master data  */
#define I2C_MASTER_FREQ_HZ          40000 /*!< I2C master clock frequency */
#define I2C_MASTER_TIMEOUT_MS       10000

#define SCD40_SENSOR_ADDR           0x62                        /*!< I2C address of SCD40 sensor */

/* SCD40 Commands */
#define SCD40_CMD_START_PERIODIC_MEASUREMENT    0x21B1
#define SCD40_CMD_STOP_PERIODIC_MEASUREMENT     0x3f86
#define SCD40_CMD_READ_MEASUREMENT              0xEC05
#define SCD40_CMD_GET_SERIAL_NUMBER             0x3682
#define SDC40_CMD_REINIT                        0x3646

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
    return i2c_master_transmit(dev_handle, cmd_buf, sizeof(cmd_buf), I2C_MASTER_TIMEOUT_MS);
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

    esp_err_t ret = i2c_master_receive(dev_handle, buf, buf_len, I2C_MASTER_TIMEOUT_MS);
    if (ret != ESP_OK) {
        return ret;
    }

    // Validate CRC for each word
    for (size_t i = 0; i < num_words; i++) {
        uint8_t *word_data = &buf[i * 3];
        uint8_t crc = scd40_calculate_crc(word_data, 2);

        if (crc != word_data[2]) {
            ESP_LOGE(TAG, "CRC error for word %d: expected 0x%02X, got 0x%02X", i, crc, word_data[2]);
            return ESP_ERR_INVALID_CRC;
        }

        values[i] = (word_data[0] << 8) | word_data[1];
    }

    return ESP_OK;
}

/**
 * @brief Get SCD40 serial number (48-bit unique ID)
 */
static esp_err_t scd40_get_serial_number(i2c_master_dev_handle_t dev_handle, uint64_t *serial)
{
    esp_err_t ret;
    uint16_t serial_words[3];

    // Send get serial number command
    ret = scd40_send_command(dev_handle, SCD40_CMD_GET_SERIAL_NUMBER);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send get serial number command: %s", esp_err_to_name(ret));
        return ret;
    }

    // Read 3 words (48 bits) with CRC
    ret = scd40_read_data_with_crc(dev_handle, serial_words, 3);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read serial number: %s", esp_err_to_name(ret));
        return ret;
    }

    // Combine 3 words into 48-bit serial number
    *serial = ((uint64_t)serial_words[0] << 32) | ((uint64_t)serial_words[1] << 16) | serial_words[2];

    return ESP_OK;
}

/**
 * @brief Start SCD40 periodic measurement mode
 */
static esp_err_t scd40_start_periodic_measurement(i2c_master_dev_handle_t dev_handle)
{
    return scd40_send_command(dev_handle, SCD40_CMD_START_PERIODIC_MEASUREMENT);
}

static esp_err_t scd40_reinit(i2c_master_dev_handle_t dev_handle){
    return scd40_send_command(dev_handle, SDC40_CMD_REINIT);
}

static esp_err_t scd40_stop_periodic_measurement(i2c_master_dev_handle_t dev_handle)
{
    return scd40_send_command(dev_handle, SCD40_CMD_STOP_PERIODIC_MEASUREMENT);
}

/**
 * @brief Read CO2, temperature, and humidity from SCD40
 */
static esp_err_t scd40_read_measurement(i2c_master_dev_handle_t dev_handle,
                                       uint16_t *co2, float *temperature, float *humidity)
{
    esp_err_t ret;
    uint16_t data[3];

    // Send read measurement command
    ret = scd40_send_command(dev_handle, SCD40_CMD_READ_MEASUREMENT);
    if (ret != ESP_OK) {
        return ret;
    }

    // Wait for sensor response (1ms)
    vTaskDelay(pdMS_TO_TICKS(1));

    // Read 3 words: CO2, Temperature, Humidity
    ret = scd40_read_data_with_crc(dev_handle, data, 3);
    if (ret != ESP_OK) {
        return ret;
    }

    // Convert raw values according to SCD40 datasheet
    *co2 = data[0];                                         // CO2 in ppm
    *temperature = -45.0f + 175.0f * data[1] / 65536.0f;   // Temperature in °C
    *humidity = 100.0f * data[2] / 65536.0f;               // Humidity in %

    return ESP_OK;
}

/**
 * @brief Initialize I2C master bus and SCD40 device
 */
static esp_err_t i2c_master_init(i2c_master_bus_handle_t *bus_handle, i2c_master_dev_handle_t *dev_handle)
{
    i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_MASTER_NUM,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = false,
        };

    esp_err_t ret = i2c_new_master_bus(&bus_config, bus_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create I2C bus: %s", esp_err_to_name(ret));
        return ret;
    }

    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = SCD40_SENSOR_ADDR,
        .scl_speed_hz = I2C_MASTER_FREQ_HZ,
    };

    ret = i2c_master_bus_add_device(*bus_handle, &dev_config, dev_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add I2C device: %s", esp_err_to_name(ret));
        return ret;
    }

    return ESP_OK;
}



void app_main(void)
{
    i2c_master_bus_handle_t bus_handle;
    i2c_master_dev_handle_t dev_handle;

    ESP_LOGI(TAG, "Initializing SCD40...");

    // Initialize I2C
    esp_err_t ret = i2c_master_init(&bus_handle, &dev_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C init failed");
        return;
    }

    // Get and display serial number
    uint64_t serial_number = 0;
    ret = scd40_get_serial_number(dev_handle, &serial_number);
    while (ret != ESP_OK){
        ESP_LOGE(TAG, "Failed to read serial");
        ret = scd40_stop_periodic_measurement(dev_handle);
        if (ret != ESP_OK){
            ESP_LOGE(TAG, "Stop failed");
        }
        ret = scd40_get_serial_number(dev_handle, &serial_number);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    ESP_LOGI(TAG, "Serial: %04X-%04X-%04X",
               (uint16_t)(serial_number >> 32),
               (uint16_t)(serial_number >> 16),
               (uint16_t)(serial_number & 0xFFFF));

    // Start periodic measurement
    ret = scd40_start_periodic_measurement(dev_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start measurement");
        goto cleanup;
    }
    ESP_LOGI(TAG, "Measurement started");

    // Wait for first measurement (5 seconds)
    ESP_LOGI(TAG, "Waiting for first reading...");
    vTaskDelay(pdMS_TO_TICKS(5000));

    // Read measurements in a loop
    while (1) {
        uint16_t co2;
        float temperature, humidity;

        ret = scd40_read_measurement(dev_handle, &co2, &temperature, &humidity);
        if (ret == ESP_OK && co2 > 0) {
            ESP_LOGI(TAG, "CO2: %d ppm, Temp: %.2f °C, Humidity: %.2f %%",
                     co2, temperature, humidity);
        } else if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Read failed: %s", esp_err_to_name(ret));
        }

        vTaskDelay(pdMS_TO_TICKS(5000));
    }

cleanup:
    i2c_master_bus_rm_device(dev_handle);
    i2c_del_master_bus(bus_handle);
}