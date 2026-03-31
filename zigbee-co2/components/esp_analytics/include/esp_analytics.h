/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Predefined analytics metrics
 */
typedef enum {
    ESP_ANALYTICS_METRIC_UPTIME_SECONDS,      /**< Device uptime in seconds */
    ESP_ANALYTICS_METRIC_RESTART_COUNT,       /**< Number of device restarts */
    ESP_ANALYTICS_METRIC_WIFI_CONNECTIONS,    /**< WiFi connection attempts */
    ESP_ANALYTICS_METRIC_ZIGBEE_MESSAGES,     /**< Zigbee messages sent/received */
    ESP_ANALYTICS_METRIC_SENSOR_READINGS,     /**< Total sensor readings */
    ESP_ANALYTICS_METRIC_ERROR_COUNT,         /**< Error occurrences */
    ESP_ANALYTICS_METRIC_BATTERY_LEVEL,       /**< Battery percentage (if available) */
    ESP_ANALYTICS_METRIC_MEMORY_USAGE,        /**< Free heap memory in bytes */
    ESP_ANALYTICS_METRIC_COUNT                /**< Total number of predefined metrics */
} esp_analytics_metric_t;

/**
 * @brief Analytics configuration structure
 */
typedef struct {
    const char* server_url;       /**< Analytics server URL */
    const char* api_key;          /**< API key for authentication */
    const char* device_id;        /**< Device identifier */
    uint32_t transmission_interval_sec; /**< Transmission interval in seconds */
    bool enable_deep_sleep;       /**< Enable deep sleep between transmissions */
    uint16_t buffer_size;         /**< Metric buffer size */
} esp_analytics_config_t;

/**
 * @brief Initialize the analytics component
 * 
 * @param config Pointer to configuration structure
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t esp_analytics_init(const esp_analytics_config_t* config);

/**
 * @brief Initialize the analytics component with default configuration
 * 
 * @param server_url Analytics server URL
 * @param api_key API key for authentication
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t esp_analytics_init_simple(const char* server_url, const char* api_key);

/**
 * @brief Increment a predefined metric by 1
 * 
 * @param metric Metric to increment
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t esp_analytics_increase(esp_analytics_metric_t metric);

/**
 * @brief Increment a predefined metric by specified value
 * 
 * @param metric Metric to increment
 * @param value Value to add (must be positive)
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t esp_analytics_increase_by(esp_analytics_metric_t metric, double value);

/**
 * @brief Set a metric to a specific value
 * 
 * @param metric Metric to set
 * @param value Value to set
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t esp_analytics_set(esp_analytics_metric_t metric, double value);

/**
 * @brief Log an event with formatted message
 * 
 * @param severity Event severity (DEBUG, INFO, WARN, ERROR, FATAL)
 * @param event_type Event type string
 * @param format Printf-style format string
 * @param ... Variable arguments
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t esp_analytics_log(const char* severity, const char* event_type, const char* format, ...);

/**
 * @brief Log a simple info event
 * 
 * @param event_type Event type string
 * @param message Event message
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t esp_analytics_log_info(const char* event_type, const char* message);

/**
 * @brief Log an error event
 * 
 * @param event_type Event type string
 * @param message Error message
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t esp_analytics_log_error(const char* event_type, const char* message);

/**
 * @brief Force immediate transmission of buffered data
 * 
 * This bypasses power saving and sends data immediately.
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t esp_analytics_flush(void);

/**
 * @brief Check if analytics component is initialized
 * 
 * @return true if initialized, false otherwise
 */
bool esp_analytics_is_initialized(void);

/**
 * @brief Get current buffer status
 * 
 * @param buffered_count Output for number of buffered metrics
 * @param buffer_capacity Output for buffer capacity
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t esp_analytics_get_buffer_status(size_t* buffered_count, size_t* buffer_capacity);

/**
 * @brief Cleanup and release resources
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t esp_analytics_deinit(void);

#ifdef __cplusplus
}
#endif
