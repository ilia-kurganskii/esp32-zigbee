/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * SCD40 CO2 Sensor Driver Implementation
 */

#include "scd40.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "scd40";

/* SCD40 Commands */
#define SCD40_CMD_START_PERIODIC_MEASUREMENT        0x21B1
#define SCD40_CMD_READ_MEASUREMENT                  0xEC05
#define SCD40_CMD_STOP_PERIODIC_MEASUREMENT         0x3F86
#define SCD40_CMD_SET_TEMPERATURE_OFFSET            0x241D
#define SCD40_CMD_GET_TEMPERATURE_OFFSET            0x2318
#define SCD40_CMD_SET_SENSOR_ALTITUDE               0x2427
#define SCD40_CMD_GET_SENSOR_ALTITUDE               0x2322
#define SCD40_CMD_SET_AMBIENT_PRESSURE              0xE000
#define SCD40_CMD_PERFORM_FORCED_RECALIBRATION      0x362F
#define SCD40_CMD_SET_AUTOMATIC_SELF_CALIBRATION    0x2416
#define SCD40_CMD_GET_AUTOMATIC_SELF_CALIBRATION    0x2313
#define SCD40_CMD_START_LOW_POWER_PERIODIC_MEAS     0x21AC
#define SCD40_CMD_GET_DATA_READY_STATUS             0xE4B8
#define SCD40_CMD_PERSIST_SETTINGS                  0x3615
#define SCD40_CMD_GET_SERIAL_NUMBER                 0x3682
#define SCD40_CMD_PERFORM_SELF_TEST                 0x3639
#define SCD40_CMD_PERFORM_FACTORY_RESET             0x3632
#define SCD40_CMD_REINIT                            0x3646
#define SCD40_CMD_MEASURE_SINGLE_SHOT               0x219D
#define SCD40_CMD_MEASURE_SINGLE_SHOT_RHT_ONLY      0x2196
#define SCD40_CMD_POWER_DOWN                        0x36E0
#define SCD40_CMD_WAKE_UP                           0x36F6

/* Timing constants (milliseconds) */
#define SCD40_I2C_TIMEOUT_MS                        10000
#define SCD40_SELF_TEST_DELAY_MS                    10000
#define SCD40_FACTORY_RESET_DELAY_MS                1200
#define SCD40_REINIT_DELAY_MS                       20
#define SCD40_STOP_MEASUREMENT_DELAY_MS             500
#define SCD40_WAKE_UP_DELAY_MS                      20
#define SCD40_FRC_DELAY_MS                          400

/* CRC-8 calculation for data integrity */
#define SCD40_CRC8_POLYNOMIAL                       0x31
#define SCD40_CRC8_INIT                             0xFF

/**
 * @brief Calculate CRC-8 checksum for SCD40 data validation
 *
 * Polynomial: 0x31 (x^8 + x^5 + x^4 + 1)
 * Initialization: 0xFF
 */
static uint8_t scd40_calculate_crc(const uint8_t *data, size_t len)
{
    uint8_t crc = SCD40_CRC8_INIT;

    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t bit = 0; bit < 8; bit++) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ SCD40_CRC8_POLYNOMIAL;
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
    esp_err_t ret = i2c_master_transmit(dev_handle, cmd_buf, sizeof(cmd_buf), SCD40_I2C_TIMEOUT_MS);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send command 0x%04X: %s", cmd, esp_err_to_name(ret));
    }

    return ret;
}

/**
 * @brief Send command with data to SCD40 sensor
 */
static esp_err_t scd40_send_command_with_data(i2c_master_dev_handle_t dev_handle, uint16_t cmd, uint16_t data)
{
    uint8_t buf[5];
    buf[0] = (cmd >> 8) & 0xFF;
    buf[1] = cmd & 0xFF;
    buf[2] = (data >> 8) & 0xFF;
    buf[3] = data & 0xFF;
    buf[4] = scd40_calculate_crc(&buf[2], 2);

    esp_err_t ret = i2c_master_transmit(dev_handle, buf, sizeof(buf), SCD40_I2C_TIMEOUT_MS);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send command 0x%04X with data: %s", cmd, esp_err_to_name(ret));
    }

    return ret;
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
        ESP_LOGE(TAG, "Invalid read length: %zu words (max 3)", num_words);
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = i2c_master_receive(dev_handle, buf, buf_len, SCD40_I2C_TIMEOUT_MS);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to receive data: %s", esp_err_to_name(ret));
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
        ESP_LOGE(TAG, "Invalid arguments");
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
        .device_address = SCD40_I2C_ADDR,
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
        return ret;
    }

    // Wait for sensor to process command (1ms)
    vTaskDelay(pdMS_TO_TICKS(1));

    // Read 3 words (48 bits) with CRC
    ret = scd40_read_data_with_crc(handle->dev_handle, serial_words, 3);
    if (ret != ESP_OK) {
        return ret;
    }

    // Combine 3 words into 48-bit serial number
    *serial = ((uint64_t)serial_words[0] << 32) | ((uint64_t)serial_words[1] << 16) | serial_words[2];

    ESP_LOGI(TAG, "Serial number: 0x%04X%04X%04X", serial_words[0], serial_words[1], serial_words[2]);

    return ESP_OK;
}

esp_err_t scd40_start_periodic_measurement(scd40_handle_t *handle)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = scd40_send_command(handle->dev_handle, SCD40_CMD_START_PERIODIC_MEASUREMENT);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Started periodic measurement");
    }
    return ret;
}

esp_err_t scd40_stop_periodic_measurement(scd40_handle_t *handle)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = scd40_send_command(handle->dev_handle, SCD40_CMD_STOP_PERIODIC_MEASUREMENT);
    if (ret == ESP_OK) {
        // Wait for sensor to stop measurements (500ms as per datasheet)
        vTaskDelay(pdMS_TO_TICKS(SCD40_STOP_MEASUREMENT_DELAY_MS));
        ESP_LOGI(TAG, "Stopped periodic measurement");
    }
    return ret;
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
        return ret;
    }

    // Wait for sensor response (1ms)
    vTaskDelay(pdMS_TO_TICKS(1));

    // Read status word (2 bytes + 1 CRC)
    ret = scd40_read_data_with_crc(handle->dev_handle, &status, 1);
    if (ret != ESP_OK) {
        return ret;
    }

    // According to datasheet: lower 11 bits indicate data ready status
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
        return ret;
    }

    // Wait for sensor response (1ms)
    vTaskDelay(pdMS_TO_TICKS(1));

    // Read 3 words: CO2, Temperature, Humidity
    ret = scd40_read_data_with_crc(handle->dev_handle, data, 3);
    if (ret != ESP_OK) {
        return ret;
    }

    // Log raw values for debugging
    ESP_LOGI(TAG, "Raw values - CO2: 0x%04X (%u), Temp: 0x%04X (%u), Humidity: 0x%04X (%u)",
             data[0], data[0], data[1], data[1], data[2], data[2]);

    // Convert raw values according to SCD40 datasheet
    measurement->co2_ppm = data[0];
    measurement->temperature_c = -45.0f + 175.0f * data[1] / 65536.0f;
    
    // Check for invalid humidity reading (0xFFFF indicates sensor error or unavailable data)
    // This happens when using single-shot mode on SCD41 without RHT measurement
    if (data[2] == 0xFFFF) {
        ESP_LOGW(TAG, "Humidity data unavailable (0xFFFF) - sensor may need RHT single-shot measurement");
        measurement->humidity_rh = 0.0f; // Set to 0 to indicate invalid
    } else {
        measurement->humidity_rh = 100.0f * data[2] / 65536.0f;
    }

    ESP_LOGD(TAG, "CO2: %d ppm, Temp: %.2f °C, Humidity: %.2f %%",
             measurement->co2_ppm, measurement->temperature_c, measurement->humidity_rh);

    return ESP_OK;
}

esp_err_t scd40_perform_self_test(scd40_handle_t *handle, uint16_t *result)
{
    if (handle == NULL || result == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = scd40_send_command(handle->dev_handle, SCD40_CMD_PERFORM_SELF_TEST);
    if (ret != ESP_OK) {
        return ret;
    }

    // Wait for self-test to complete (10 seconds)
    vTaskDelay(pdMS_TO_TICKS(SCD40_SELF_TEST_DELAY_MS));

    ret = scd40_read_data_with_crc(handle->dev_handle, result, 1);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Self-test result: 0x%04X", *result);
    }

    return ret;
}

esp_err_t scd40_perform_factory_reset(scd40_handle_t *handle)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = scd40_send_command(handle->dev_handle, SCD40_CMD_PERFORM_FACTORY_RESET);
    if (ret == ESP_OK) {
        vTaskDelay(pdMS_TO_TICKS(SCD40_FACTORY_RESET_DELAY_MS));
        ESP_LOGI(TAG, "Factory reset completed");
    }
    return ret;
}

esp_err_t scd40_reinit(scd40_handle_t *handle)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = scd40_send_command(handle->dev_handle, SCD40_CMD_REINIT);
    if (ret == ESP_OK) {
        // Wait for sensor to reinitialize (20ms as per datasheet)
        vTaskDelay(pdMS_TO_TICKS(SCD40_REINIT_DELAY_MS));
        ESP_LOGI(TAG, "Sensor reinitialized");
    }
    return ret;
}

esp_err_t scd40_set_temperature_offset(scd40_handle_t *handle, uint16_t offset)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = scd40_send_command_with_data(handle->dev_handle, SCD40_CMD_SET_TEMPERATURE_OFFSET, offset);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Temperature offset set to %d", offset);
    }
    return ret;
}

esp_err_t scd40_get_temperature_offset(scd40_handle_t *handle, uint16_t *offset)
{
    if (handle == NULL || offset == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = scd40_send_command(handle->dev_handle, SCD40_CMD_GET_TEMPERATURE_OFFSET);
    if (ret != ESP_OK) {
        return ret;
    }

    vTaskDelay(pdMS_TO_TICKS(1));

    return scd40_read_data_with_crc(handle->dev_handle, offset, 1);
}

esp_err_t scd40_set_sensor_altitude(scd40_handle_t *handle, uint16_t altitude)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = scd40_send_command_with_data(handle->dev_handle, SCD40_CMD_SET_SENSOR_ALTITUDE, altitude);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Sensor altitude set to %d m", altitude);
    }
    return ret;
}

esp_err_t scd40_get_sensor_altitude(scd40_handle_t *handle, uint16_t *altitude)
{
    if (handle == NULL || altitude == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = scd40_send_command(handle->dev_handle, SCD40_CMD_GET_SENSOR_ALTITUDE);
    if (ret != ESP_OK) {
        return ret;
    }

    vTaskDelay(pdMS_TO_TICKS(1));

    return scd40_read_data_with_crc(handle->dev_handle, altitude, 1);
}

esp_err_t scd40_perform_forced_recalibration(scd40_handle_t *handle, uint16_t target_co2, uint16_t *frc_correction)
{
    if (handle == NULL || frc_correction == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = scd40_send_command_with_data(handle->dev_handle, SCD40_CMD_PERFORM_FORCED_RECALIBRATION, target_co2);
    if (ret != ESP_OK) {
        return ret;
    }

    vTaskDelay(pdMS_TO_TICKS(SCD40_FRC_DELAY_MS));

    ret = scd40_read_data_with_crc(handle->dev_handle, frc_correction, 1);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "FRC correction: 0x%04X (target: %d ppm)", *frc_correction, target_co2);
    }

    return ret;
}

esp_err_t scd40_set_automatic_self_calibration(scd40_handle_t *handle, bool enable)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    uint16_t value = enable ? 1 : 0;
    esp_err_t ret = scd40_send_command_with_data(handle->dev_handle, SCD40_CMD_SET_AUTOMATIC_SELF_CALIBRATION, value);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Automatic self-calibration %s", enable ? "enabled" : "disabled");
    }
    return ret;
}

esp_err_t scd40_get_automatic_self_calibration(scd40_handle_t *handle, bool *enabled)
{
    if (handle == NULL || enabled == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = scd40_send_command(handle->dev_handle, SCD40_CMD_GET_AUTOMATIC_SELF_CALIBRATION);
    if (ret != ESP_OK) {
        return ret;
    }

    vTaskDelay(pdMS_TO_TICKS(1));

    uint16_t value;
    ret = scd40_read_data_with_crc(handle->dev_handle, &value, 1);
    if (ret == ESP_OK) {
        *enabled = (value != 0);
    }

    return ret;
}

esp_err_t scd40_measure_single_shot(scd40_handle_t *handle)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = scd40_send_command(handle->dev_handle, SCD40_CMD_MEASURE_SINGLE_SHOT);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Single-shot measurement initiated");
    }
    return ret;
}

esp_err_t scd40_measure_single_shot_rht_only(scd40_handle_t *handle)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = scd40_send_command(handle->dev_handle, SCD40_CMD_MEASURE_SINGLE_SHOT_RHT_ONLY);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Single-shot RH/T measurement initiated");
    }
    return ret;
}

esp_err_t scd40_power_down(scd40_handle_t *handle)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = scd40_send_command(handle->dev_handle, SCD40_CMD_POWER_DOWN);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Sensor powered down");
    }
    return ret;
}

esp_err_t scd40_wake_up(scd40_handle_t *handle)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = scd40_send_command(handle->dev_handle, SCD40_CMD_WAKE_UP);
    if (ret == ESP_OK) {
        vTaskDelay(pdMS_TO_TICKS(SCD40_WAKE_UP_DELAY_MS));
        ESP_LOGI(TAG, "Sensor woken up");
    }
    return ret;
}
