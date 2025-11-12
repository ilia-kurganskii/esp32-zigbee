/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "driver/i2c_master.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief SCD40 sensor configuration structure
 */
typedef struct {
    int scl_io_num;         /*!< GPIO number for I2C SCL */
    int sda_io_num;         /*!< GPIO number for I2C SDA */
    uint32_t i2c_freq_hz;   /*!< I2C clock frequency in Hz */
    i2c_port_num_t i2c_port;/*!< I2C port number */
} scd40_config_t;

/**
 * @brief SCD40 sensor handle
 */
typedef struct {
    i2c_master_bus_handle_t bus_handle;
    i2c_master_dev_handle_t dev_handle;
} scd40_handle_t;

/**
 * @brief SCD40 measurement data structure
 */
typedef struct {
    uint16_t co2_ppm;       /*!< CO2 concentration in ppm */
    float temperature_c;    /*!< Temperature in degrees Celsius */
    float humidity_rh;      /*!< Relative humidity in percent */
} scd40_measurement_t;

/**
 * @brief Initialize SCD40 sensor
 *
 * @param config Pointer to configuration structure
 * @param handle Pointer to handle structure to be initialized
 * @return
 *     - ESP_OK: Success
 *     - ESP_ERR_INVALID_ARG: Invalid argument
 *     - ESP_FAIL: I2C initialization failed
 */
esp_err_t scd40_init(const scd40_config_t *config, scd40_handle_t *handle);

/**
 * @brief Deinitialize SCD40 sensor and free resources
 *
 * @param handle Pointer to sensor handle
 * @return
 *     - ESP_OK: Success
 *     - ESP_ERR_INVALID_ARG: Invalid argument
 */
esp_err_t scd40_deinit(scd40_handle_t *handle);

/**
 * @brief Get SCD40 sensor serial number (48-bit unique ID)
 *
 * @param handle Pointer to sensor handle
 * @param serial Pointer to store the 48-bit serial number
 * @return
 *     - ESP_OK: Success
 *     - ESP_ERR_INVALID_ARG: Invalid argument
 *     - ESP_ERR_INVALID_CRC: CRC validation failed
 *     - ESP_FAIL: Communication failed
 */
esp_err_t scd40_get_serial_number(scd40_handle_t *handle, uint64_t *serial);

/**
 * @brief Start periodic measurement mode
 *
 * Measurements are available approximately every 5 seconds
 *
 * @param handle Pointer to sensor handle
 * @return
 *     - ESP_OK: Success
 *     - ESP_ERR_INVALID_ARG: Invalid argument
 *     - ESP_FAIL: Communication failed
 */
esp_err_t scd40_start_periodic_measurement(scd40_handle_t *handle);

/**
 * @brief Stop periodic measurement mode
 *
 * @param handle Pointer to sensor handle
 * @return
 *     - ESP_OK: Success
 *     - ESP_ERR_INVALID_ARG: Invalid argument
 *     - ESP_FAIL: Communication failed
 */
esp_err_t scd40_stop_periodic_measurement(scd40_handle_t *handle);

/**
 * @brief Reinitialize the sensor
 *
 * Reinit restores factory settings and clears measurement mode
 *
 * @param handle Pointer to sensor handle
 * @return
 *     - ESP_OK: Success
 *     - ESP_ERR_INVALID_ARG: Invalid argument
 *     - ESP_FAIL: Communication failed
 */
esp_err_t scd40_reinit(scd40_handle_t *handle);

/**
 * @brief Get data ready status
 *
 * Check if new measurement data is available to read
 *
 * @param handle Pointer to sensor handle
 * @param data_ready Pointer to store data ready status (true if data is ready)
 * @return
 *     - ESP_OK: Success
 *     - ESP_ERR_INVALID_ARG: Invalid argument
 *     - ESP_ERR_INVALID_CRC: CRC validation failed
 *     - ESP_FAIL: Communication failed
 */
esp_err_t scd40_get_data_ready_status(scd40_handle_t *handle, bool *data_ready);

/**
 * @brief Read CO2, temperature, and humidity measurement
 *
 * @param handle Pointer to sensor handle
 * @param measurement Pointer to structure to store measurement data
 * @return
 *     - ESP_OK: Success
 *     - ESP_ERR_INVALID_ARG: Invalid argument
 *     - ESP_ERR_INVALID_CRC: CRC validation failed
 *     - ESP_FAIL: Communication failed
 */
esp_err_t scd40_read_measurement(scd40_handle_t *handle, scd40_measurement_t *measurement);

#ifdef __cplusplus
}
#endif
