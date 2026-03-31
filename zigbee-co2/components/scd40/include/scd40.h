/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * SCD40 CO2 Sensor Driver
 * 
 * This driver provides an interface to the Sensirion SCD40 CO2 sensor
 * using I2C communication protocol with the new I2C master driver API.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "driver/i2c_master.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief SCD40 I2C address (fixed)
 */
#define SCD40_I2C_ADDR 0x62

/**
 * @brief SCD40 sensor configuration structure
 */
typedef struct {
    int scl_io_num;         /**< GPIO number for I2C SCL */
    int sda_io_num;         /**< GPIO number for I2C SDA */
    uint32_t i2c_freq_hz;   /**< I2C clock frequency in Hz */
    i2c_port_num_t i2c_port;/**< I2C port number */
} scd40_config_t;

/**
 * @brief SCD40 sensor handle
 */
typedef struct {
    i2c_master_bus_handle_t bus_handle;  /**< I2C master bus handle */
    i2c_master_dev_handle_t dev_handle;  /**< I2C device handle */
} scd40_handle_t;

/**
 * @brief SCD40 measurement data structure
 */
typedef struct {
    uint16_t co2_ppm;       /**< CO2 concentration in ppm */
    float temperature_c;    /**< Temperature in degrees Celsius */
    float humidity_rh;      /**< Relative humidity in percent */
} scd40_measurement_t;

/**
 * @brief Initialize SCD40 sensor
 * 
 * @param config Pointer to configuration structure
 * @param handle Pointer to handle structure to be initialized
 * @return
 *     - ESP_OK on success
 *     - ESP_ERR_INVALID_ARG if arguments are NULL
 *     - ESP_FAIL on I2C initialization failure
 */
esp_err_t scd40_init(const scd40_config_t *config, scd40_handle_t *handle);

/**
 * @brief Deinitialize SCD40 sensor and free resources
 * 
 * @param handle Pointer to sensor handle
 * @return
 *     - ESP_OK on success
 *     - ESP_ERR_INVALID_ARG if handle is NULL
 */
esp_err_t scd40_deinit(scd40_handle_t *handle);

/**
 * @brief Get SCD40 sensor serial number (48-bit unique ID)
 * 
 * @param handle Pointer to sensor handle
 * @param serial Pointer to store the 48-bit serial number
 * @return
 *     - ESP_OK on success
 *     - ESP_ERR_INVALID_ARG if arguments are NULL
 *     - ESP_ERR_INVALID_CRC on CRC validation failure
 *     - ESP_FAIL on communication error
 */
esp_err_t scd40_get_serial_number(scd40_handle_t *handle, uint64_t *serial);

/**
 * @brief Start periodic measurement mode
 * 
 * Measurements are available approximately every 5 seconds.
 * 
 * @param handle Pointer to sensor handle
 * @return
 *     - ESP_OK on success
 *     - ESP_ERR_INVALID_ARG if handle is NULL
 *     - ESP_FAIL on communication error
 */
esp_err_t scd40_start_periodic_measurement(scd40_handle_t *handle);

/**
 * @brief Stop periodic measurement mode
 * 
 * @param handle Pointer to sensor handle
 * @return
 *     - ESP_OK on success
 *     - ESP_ERR_INVALID_ARG if handle is NULL
 *     - ESP_FAIL on communication error
 */
esp_err_t scd40_stop_periodic_measurement(scd40_handle_t *handle);

/**
 * @brief Check if data is ready to be read
 * 
 * @param handle Pointer to sensor handle
 * @param data_ready Pointer to store data ready status (true if data is ready)
 * @return
 *     - ESP_OK on success
 *     - ESP_ERR_INVALID_ARG if arguments are NULL
 *     - ESP_ERR_INVALID_CRC on CRC validation failure
 *     - ESP_FAIL on communication error
 */
esp_err_t scd40_get_data_ready_status(scd40_handle_t *handle, bool *data_ready);

/**
 * @brief Read CO2, temperature, and humidity measurement
 * 
 * @param handle Pointer to sensor handle
 * @param measurement Pointer to structure to store measurement data
 * @return
 *     - ESP_OK on success
 *     - ESP_ERR_INVALID_ARG if arguments are NULL
 *     - ESP_ERR_INVALID_CRC on CRC validation failure
 *     - ESP_FAIL on communication error
 */
esp_err_t scd40_read_measurement(scd40_handle_t *handle, scd40_measurement_t *measurement);

/**
 * @brief Perform self-test
 * 
 * This command can take up to 10 seconds to complete.
 * 
 * @param handle Pointer to sensor handle
 * @param result Pointer to store test result (0 = no malfunction detected)
 * @return
 *     - ESP_OK on success
 *     - ESP_ERR_INVALID_ARG if arguments are NULL
 *     - ESP_ERR_INVALID_CRC on CRC validation failure
 *     - ESP_FAIL on communication error
 */
esp_err_t scd40_perform_self_test(scd40_handle_t *handle, uint16_t *result);

/**
 * @brief Perform factory reset
 * 
 * Resets all configuration settings in EEPROM to factory defaults.
 * 
 * @param handle Pointer to sensor handle
 * @return
 *     - ESP_OK on success
 *     - ESP_ERR_INVALID_ARG if handle is NULL
 *     - ESP_FAIL on communication error
 */
esp_err_t scd40_perform_factory_reset(scd40_handle_t *handle);

/**
 * @brief Reinitialize the sensor
 * 
 * Reloads configuration from EEPROM. Takes about 20ms.
 * 
 * @param handle Pointer to sensor handle
 * @return
 *     - ESP_OK on success
 *     - ESP_ERR_INVALID_ARG if handle is NULL
 *     - ESP_FAIL on communication error
 */
esp_err_t scd40_reinit(scd40_handle_t *handle);

/**
 * @brief Set temperature offset
 * 
 * Temperature offset value in degrees Celsius * 374.
 * 
 * @param handle Pointer to sensor handle
 * @param offset Temperature offset (raw value)
 * @return
 *     - ESP_OK on success
 *     - ESP_ERR_INVALID_ARG if handle is NULL
 *     - ESP_FAIL on communication error
 */
esp_err_t scd40_set_temperature_offset(scd40_handle_t *handle, uint16_t offset);

/**
 * @brief Get temperature offset
 * 
 * @param handle Pointer to sensor handle
 * @param offset Pointer to store temperature offset (raw value)
 * @return
 *     - ESP_OK on success
 *     - ESP_ERR_INVALID_ARG if arguments are NULL
 *     - ESP_ERR_INVALID_CRC on CRC validation failure
 *     - ESP_FAIL on communication error
 */
esp_err_t scd40_get_temperature_offset(scd40_handle_t *handle, uint16_t *offset);

/**
 * @brief Set sensor altitude
 * 
 * @param handle Pointer to sensor handle
 * @param altitude Altitude in meters above sea level (0-3000m)
 * @return
 *     - ESP_OK on success
 *     - ESP_ERR_INVALID_ARG if handle is NULL
 *     - ESP_FAIL on communication error
 */
esp_err_t scd40_set_sensor_altitude(scd40_handle_t *handle, uint16_t altitude);

/**
 * @brief Get sensor altitude
 * 
 * @param handle Pointer to sensor handle
 * @param altitude Pointer to store altitude in meters
 * @return
 *     - ESP_OK on success
 *     - ESP_ERR_INVALID_ARG if arguments are NULL
 *     - ESP_ERR_INVALID_CRC on CRC validation failure
 *     - ESP_FAIL on communication error
 */
esp_err_t scd40_get_sensor_altitude(scd40_handle_t *handle, uint16_t *altitude);

/**
 * @brief Perform forced recalibration
 * 
 * @param handle Pointer to sensor handle
 * @param target_co2 Target CO2 concentration in ppm (400-2000 ppm)
 * @param frc_correction Pointer to store FRC correction value
 * @return
 *     - ESP_OK on success
 *     - ESP_ERR_INVALID_ARG if arguments are NULL
 *     - ESP_ERR_INVALID_CRC on CRC validation failure
 *     - ESP_FAIL on communication error
 */
esp_err_t scd40_perform_forced_recalibration(scd40_handle_t *handle, uint16_t target_co2, uint16_t *frc_correction);

/**
 * @brief Enable/disable automatic self-calibration
 * 
 * @param handle Pointer to sensor handle
 * @param enable true to enable ASC, false to disable
 * @return
 *     - ESP_OK on success
 *     - ESP_ERR_INVALID_ARG if handle is NULL
 *     - ESP_FAIL on communication error
 */
esp_err_t scd40_set_automatic_self_calibration(scd40_handle_t *handle, bool enable);

/**
 * @brief Get automatic self-calibration status
 * 
 * @param handle Pointer to sensor handle
 * @param enabled Pointer to store ASC status
 * @return
 *     - ESP_OK on success
 *     - ESP_ERR_INVALID_ARG if arguments are NULL
 *     - ESP_ERR_INVALID_CRC on CRC validation failure
 *     - ESP_FAIL on communication error
 */
esp_err_t scd40_get_automatic_self_calibration(scd40_handle_t *handle, bool *enabled);

/**
 * @brief Put sensor into low power single-shot measurement mode
 * 
 * Measurement takes 5 seconds.
 * 
 * @param handle Pointer to sensor handle
 * @return
 *     - ESP_OK on success
 *     - ESP_ERR_INVALID_ARG if handle is NULL
 *     - ESP_FAIL on communication error
 */
esp_err_t scd40_measure_single_shot(scd40_handle_t *handle);

/**
 * @brief Put sensor into low power mode with RH/T only
 * 
 * @param handle Pointer to sensor handle
 * @return
 *     - ESP_OK on success
 *     - ESP_ERR_INVALID_ARG if handle is NULL
 *     - ESP_FAIL on communication error
 */
esp_err_t scd40_measure_single_shot_rht_only(scd40_handle_t *handle);

/**
 * @brief Power down sensor
 * 
 * Puts sensor into sleep mode (lowest power consumption).
 * 
 * @param handle Pointer to sensor handle
 * @return
 *     - ESP_OK on success
 *     - ESP_ERR_INVALID_ARG if handle is NULL
 *     - ESP_FAIL on communication error
 */
esp_err_t scd40_power_down(scd40_handle_t *handle);

/**
 * @brief Wake up sensor from sleep mode
 * 
 * @param handle Pointer to sensor handle
 * @return
 *     - ESP_OK on success
 *     - ESP_ERR_INVALID_ARG if handle is NULL
 *     - ESP_FAIL on communication error
 */
esp_err_t scd40_wake_up(scd40_handle_t *handle);

#ifdef __cplusplus
}
#endif
