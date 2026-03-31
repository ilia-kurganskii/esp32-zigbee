/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_analytics.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include "esp_log.h"
#include "esp_http_client.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_timer.h"
#include "esp_sleep.h"
#include "esp_wifi.h"
#include "esp_system.h"
#include "cJSON.h"
#include "sdkconfig.h"

static const char* TAG = "esp_analytics";

// Metric names corresponding to esp_analytics_metric_t
static const char* METRIC_NAMES[] = {
    "device_uptime_seconds",
    "device_restart_count",
    "wifi_connection_attempts",
    "zigbee_messages_count",
    "sensor_readings_count",
    "error_count",
    "battery_level_percent",
    "free_heap_bytes"
};

// Metric types
static const char* METRIC_TYPES[] = {
    "counter",
    "counter",
    "counter", 
    "counter",
    "counter",
    "counter",
    "gauge",
    "gauge"
};

/**
 * @brief Metric data structure
 */
typedef struct {
    esp_analytics_metric_t metric;
    double value;
    int64_t timestamp;
} esp_analytics_metric_data_t;

/**
 * @brief Event data structure
 */
typedef struct {
    char* event_type;
    char* message;
    char* severity;
    int64_t timestamp;
} esp_analytics_event_data_t;

/**
 * @brief Analytics context structure
 */
typedef struct {
    bool initialized;
    esp_analytics_config_t config;
    
    // Metric storage
    esp_analytics_metric_data_t* metrics;
    size_t metrics_count;
    size_t metrics_capacity;
    
    // Event storage
    esp_analytics_event_data_t* events;
    size_t events_count;
    size_t events_capacity;
    
    // Synchronization
    SemaphoreHandle_t mutex;
    
    // Timing
    int64_t start_time;
    int64_t last_transmission;
    
    // HTTP client
    esp_http_client_handle_t http_client;
} esp_analytics_context_t;

static esp_analytics_context_t g_ctx = {0};

// Helper functions
static esp_err_t ensure_wifi_connected(void);
static esp_err_t create_json_payload(cJSON** root);
static esp_err_t send_http_request(const char* json_payload);
static esp_err_t load_config_from_nvs(esp_analytics_config_t* config);
static esp_err_t save_config_to_nvs(const esp_analytics_config_t* config);
static void cleanup_events(void);
static void cleanup_metrics(void);
static int64_t get_current_timestamp_ms(void);

static int64_t get_current_timestamp_ms(void) {
    return esp_timer_get_time() / 1000;
}

static esp_err_t load_config_from_nvs(esp_analytics_config_t* config) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("analytics", NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        return err;
    }
    
    size_t required_size = 0;
    
    // Load server URL
    err = nvs_get_str(nvs_handle, "server_url", NULL, &required_size);
    if (err == ESP_OK) {
        char* server_url = malloc(required_size);
        nvs_get_str(nvs_handle, "server_url", server_url, &required_size);
        config->server_url = server_url;
    }
    
    // Load API key
    err = nvs_get_str(nvs_handle, "api_key", NULL, &required_size);
    if (err == ESP_OK) {
        char* api_key = malloc(required_size);
        nvs_get_str(nvs_handle, "api_key", api_key, &required_size);
        config->api_key = api_key;
    }
    
    // Load device ID
    err = nvs_get_str(nvs_handle, "device_id", NULL, &required_size);
    if (err == ESP_OK) {
        char* device_id = malloc(required_size);
        nvs_get_str(nvs_handle, "device_id", device_id, &required_size);
        config->device_id = device_id;
    }
    
    // Load numeric values
    nvs_get_u32(nvs_handle, "tx_interval", &config->transmission_interval_sec);
    nvs_get_u8(nvs_handle, "deep_sleep", (uint8_t*)&config->enable_deep_sleep);
    nvs_get_u16(nvs_handle, "buffer_size", &config->buffer_size);
    
    nvs_close(nvs_handle);
    return ESP_OK;
}

static esp_err_t save_config_to_nvs(const esp_analytics_config_t* config) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("analytics", NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        return err;
    }
    
    // Save configuration values
    if (config->server_url) {
        nvs_set_str(nvs_handle, "server_url", config->server_url);
    }
    if (config->api_key) {
        nvs_set_str(nvs_handle, "api_key", config->api_key);
    }
    if (config->device_id) {
        nvs_set_str(nvs_handle, "device_id", config->device_id);
    }
    
    nvs_set_u32(nvs_handle, "tx_interval", config->transmission_interval_sec);
    nvs_set_u8(nvs_handle, "deep_sleep", config->enable_deep_sleep);
    nvs_set_u16(nvs_handle, "buffer_size", config->buffer_size);
    
    nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    return ESP_OK;
}

static esp_err_t ensure_wifi_connected(void) {
    wifi_config_t wifi_config;
    esp_err_t err = esp_wifi_get_config(WIFI_IF_STA, &wifi_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get WiFi config");
        return err;
    }
    
    wifi_ap_record_t ap_info;
    err = esp_wifi_sta_get_ap_info(&ap_info);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "WiFi not connected, attempting connection");
        err = esp_wifi_connect();
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to connect WiFi");
            return err;
        }
        
        // Wait for connection
        int retry_count = 0;
        while (retry_count < 10) {
            vTaskDelay(pdMS_TO_TICKS(1000));
            err = esp_wifi_sta_get_ap_info(&ap_info);
            if (err == ESP_OK) {
                ESP_LOGI(TAG, "WiFi connected");
                break;
            }
            retry_count++;
        }
        
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "WiFi connection failed");
            return err;
        }
    }
    
    return ESP_OK;
}

static esp_err_t create_json_payload(cJSON** root) {
    *root = cJSON_CreateObject();
    if (*root == NULL) {
        return ESP_ERR_NO_MEM;
    }
    
    // Add device ID and timestamp
    cJSON_AddStringToObject(*root, "deviceId", g_ctx.config.device_id ? g_ctx.config.device_id : "esp32-device");
    
    // Create ISO 8601 timestamp
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", tm_info);
    cJSON_AddStringToObject(*root, "timestamp", timestamp);
    
    // Add metrics array
    cJSON* metrics_array = cJSON_CreateArray();
    if (metrics_array == NULL) {
        cJSON_Delete(*root);
        return ESP_ERR_NO_MEM;
    }
    cJSON_AddItemToObject(*root, "metrics", metrics_array);
    
    // Add all metrics
    for (size_t i = 0; i < g_ctx.metrics_count; i++) {
        const esp_analytics_metric_data_t* metric_data = &g_ctx.metrics[i];
        
        cJSON* metric_obj = cJSON_CreateObject();
        if (metric_obj == NULL) {
            continue;
        }
        
        cJSON_AddStringToObject(metric_obj, "name", METRIC_NAMES[metric_data->metric]);
        cJSON_AddNumberToObject(metric_obj, "value", metric_data->value);
        cJSON_AddStringToObject(metric_obj, "type", METRIC_TYPES[metric_data->metric]);
        
        // Add labels
        cJSON* labels = cJSON_CreateObject();
        if (labels != NULL) {
            cJSON_AddStringToObject(labels, "device_type", "esp32c6");
            if (g_ctx.config.device_id) {
                cJSON_AddStringToObject(labels, "device_id", g_ctx.config.device_id);
            }
            cJSON_AddItemToObject(metric_obj, "labels", labels);
        }
        
        cJSON_AddItemToArray(metrics_array, metric_obj);
    }
    
    // Add events array
    cJSON* events_array = cJSON_CreateArray();
    if (events_array == NULL) {
        cJSON_Delete(*root);
        return ESP_ERR_NO_MEM;
    }
    cJSON_AddItemToObject(*root, "events", events_array);
    
    // Add all events
    for (size_t i = 0; i < g_ctx.events_count; i++) {
        const esp_analytics_event_data_t* event_data = &g_ctx.events[i];
        
        cJSON* event_obj = cJSON_CreateObject();
        if (event_obj == NULL) {
            continue;
        }
        
        cJSON_AddStringToObject(event_obj, "type", event_data->event_type);
        cJSON_AddStringToObject(event_obj, "message", event_data->message);
        cJSON_AddStringToObject(event_obj, "severity", event_data->severity);
        
        // Create timestamp
        time_t event_time = event_data->timestamp / 1000;
        struct tm* event_tm = localtime(&event_time);
        char event_timestamp[32];
        strftime(event_timestamp, sizeof(event_timestamp), "%Y-%m-%dT%H:%M:%SZ", event_tm);
        cJSON_AddStringToObject(event_obj, "timestamp", event_timestamp);
        
        cJSON_AddItemToArray(events_array, event_obj);
    }
    
    // Add empty OTEL array as required by spec
    cJSON* otel_array = cJSON_CreateArray();
    cJSON_AddItemToObject(*root, "otel", otel_array);
    
    return ESP_OK;
}

static esp_err_t send_http_request(const char* json_payload) {
    if (g_ctx.http_client == NULL) {
        esp_http_client_config_t config = {
            .url = g_ctx.config.server_url,
            .method = HTTP_METHOD_POST,
            .timeout_ms = 10000,
            .disable_auto_redirect = true,
        };
        
        g_ctx.http_client = esp_http_client_init(&config);
        if (g_ctx.http_client == NULL) {
            ESP_LOGE(TAG, "Failed to initialize HTTP client");
            return ESP_FAIL;
        }
    }
    
    // Set headers
    char content_length[32];
    snprintf(content_length, sizeof(content_length), "%zu", strlen(json_payload));
    
    esp_http_client_set_header(g_ctx.http_client, "Content-Type", "application/json");
    esp_http_client_set_header(g_ctx.http_client, "Content-Length", content_length);
    
    if (g_ctx.config.api_key && strlen(g_ctx.config.api_key) > 0) {
        esp_http_client_set_header(g_ctx.http_client, "X-API-Key", g_ctx.config.api_key);
    }
    
    // Set POST data
    esp_http_client_set_post_field(g_ctx.http_client, json_payload, strlen(json_payload));
    
    // Perform request
    esp_err_t err = esp_http_client_perform(g_ctx.http_client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTP request failed: %s", esp_err_to_name(err));
        return err;
    }
    
    int status_code = esp_http_client_get_status_code(g_ctx.http_client);
    if (status_code >= 200 && status_code < 300) {
        ESP_LOGI(TAG, "Analytics data sent successfully, status: %d", status_code);
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "HTTP request failed with status: %d", status_code);
        return ESP_FAIL;
    }
}

static void cleanup_events(void) {
    for (size_t i = 0; i < g_ctx.events_count; i++) {
        if (g_ctx.events[i].event_type) {
            free(g_ctx.events[i].event_type);
        }
        if (g_ctx.events[i].message) {
            free(g_ctx.events[i].message);
        }
        if (g_ctx.events[i].severity) {
            free(g_ctx.events[i].severity);
        }
    }
    if (g_ctx.events) {
        free(g_ctx.events);
        g_ctx.events = NULL;
    }
    g_ctx.events_count = 0;
    g_ctx.events_capacity = 0;
}

static void cleanup_metrics(void) {
    if (g_ctx.metrics) {
        free(g_ctx.metrics);
        g_ctx.metrics = NULL;
    }
    g_ctx.metrics_count = 0;
    g_ctx.metrics_capacity = 0;
}

// Public API implementation
esp_err_t esp_analytics_init(const esp_analytics_config_t* config) {
    if (g_ctx.initialized) {
        ESP_LOGW(TAG, "Analytics already initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (config == NULL || config->server_url == NULL) {
        ESP_LOGE(TAG, "Invalid configuration");
        return ESP_ERR_INVALID_ARG;
    }
    
    memset(&g_ctx, 0, sizeof(g_ctx));
    
    // Copy configuration
    g_ctx.config = *config;
    
    // Duplicate strings to avoid dangling pointers
    if (config->server_url) {
        g_ctx.config.server_url = strdup(config->server_url);
    }
    if (config->api_key) {
        g_ctx.config.api_key = strdup(config->api_key);
    }
    if (config->device_id) {
        g_ctx.config.device_id = strdup(config->device_id);
    } else {
        g_ctx.config.device_id = strdup("esp32-device");
    }
    
    // Set defaults
    if (g_ctx.config.transmission_interval_sec == 0) {
        g_ctx.config.transmission_interval_sec = 300; // 5 minutes
    }
    if (g_ctx.config.buffer_size == 0) {
        g_ctx.config.buffer_size = 100;
    }
    
    // Create mutex
    g_ctx.mutex = xSemaphoreCreateMutex();
    if (g_ctx.mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return ESP_ERR_NO_MEM;
    }
    
    // Allocate buffers
    g_ctx.metrics = malloc(sizeof(esp_analytics_metric_data_t) * g_ctx.config.buffer_size);
    g_ctx.events = malloc(sizeof(esp_analytics_event_data_t) * 10); // Smaller buffer for events
    g_ctx.metrics_capacity = g_ctx.config.buffer_size;
    g_ctx.events_capacity = 10;
    
    if (g_ctx.metrics == NULL || g_ctx.events == NULL) {
        ESP_LOGE(TAG, "Failed to allocate buffers");
        if (g_ctx.metrics) free(g_ctx.metrics);
        if (g_ctx.events) free(g_ctx.events);
        vSemaphoreDelete(g_ctx.mutex);
        return ESP_ERR_NO_MEM;
    }
    
    // Initialize timing
    g_ctx.start_time = get_current_timestamp_ms();
    g_ctx.last_transmission = g_ctx.start_time;
    
    // Count restarts (simplified - in real implementation would use NVS counter)
    esp_analytics_increase(ESP_ANALYTICS_METRIC_RESTART_COUNT);
    
    g_ctx.initialized = true;
    ESP_LOGI(TAG, "Analytics initialized successfully");
    
    return ESP_OK;
}

esp_err_t esp_analytics_init_simple(const char* server_url, const char* api_key) {
    esp_analytics_config_t config = {
        .server_url = server_url,
        .api_key = api_key,
        .device_id = NULL,
        .transmission_interval_sec = 300,
        .enable_deep_sleep = true,
        .buffer_size = 100
    };
    
    return esp_analytics_init(&config);
}

esp_err_t esp_analytics_increase(esp_analytics_metric_t metric) {
    return esp_analytics_increase_by(metric, 1.0);
}

esp_err_t esp_analytics_increase_by(esp_analytics_metric_t metric, double value) {
    if (!g_ctx.initialized || metric >= ESP_ANALYTICS_METRIC_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (xSemaphoreTake(g_ctx.mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    // Find existing metric or add new one
    for (size_t i = 0; i < g_ctx.metrics_count; i++) {
        if (g_ctx.metrics[i].metric == metric) {
            g_ctx.metrics[i].value += value;
            g_ctx.metrics[i].timestamp = get_current_timestamp_ms();
            xSemaphoreGive(g_ctx.mutex);
            return ESP_OK;
        }
    }
    
    // Add new metric if space available
    if (g_ctx.metrics_count < g_ctx.metrics_capacity) {
        g_ctx.metrics[g_ctx.metrics_count].metric = metric;
        g_ctx.metrics[g_ctx.metrics_count].value = value;
        g_ctx.metrics[g_ctx.metrics_count].timestamp = get_current_timestamp_ms();
        g_ctx.metrics_count++;
    } else {
        // Replace oldest metric (circular buffer)
        g_ctx.metrics[0].metric = metric;
        g_ctx.metrics[0].value = value;
        g_ctx.metrics[0].timestamp = get_current_timestamp_ms();
    }
    
    xSemaphoreGive(g_ctx.mutex);
    
    // Update uptime metric
    if (metric != ESP_ANALYTICS_METRIC_UPTIME_SECONDS) {
        int64_t uptime_ms = get_current_timestamp_ms() - g_ctx.start_time;
        esp_analytics_set(ESP_ANALYTICS_METRIC_UPTIME_SECONDS, uptime_ms / 1000.0);
    }
    
    return ESP_OK;
}

esp_err_t esp_analytics_set(esp_analytics_metric_t metric, double value) {
    if (!g_ctx.initialized || metric >= ESP_ANALYTICS_METRIC_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (xSemaphoreTake(g_ctx.mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    // Find existing metric or add new one
    for (size_t i = 0; i < g_ctx.metrics_count; i++) {
        if (g_ctx.metrics[i].metric == metric) {
            g_ctx.metrics[i].value = value;
            g_ctx.metrics[i].timestamp = get_current_timestamp_ms();
            xSemaphoreGive(g_ctx.mutex);
            return ESP_OK;
        }
    }
    
    // Add new metric if space available
    if (g_ctx.metrics_count < g_ctx.metrics_capacity) {
        g_ctx.metrics[g_ctx.metrics_count].metric = metric;
        g_ctx.metrics[g_ctx.metrics_count].value = value;
        g_ctx.metrics[g_ctx.metrics_count].timestamp = get_current_timestamp_ms();
        g_ctx.metrics_count++;
    } else {
        // Replace oldest metric
        g_ctx.metrics[0].metric = metric;
        g_ctx.metrics[0].value = value;
        g_ctx.metrics[0].timestamp = get_current_timestamp_ms();
    }
    
    xSemaphoreGive(g_ctx.mutex);
    return ESP_OK;
}

esp_err_t esp_analytics_log(const char* severity, const char* event_type, const char* format, ...) {
    if (!g_ctx.initialized || severity == NULL || event_type == NULL || format == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Format message
    va_list args;
    va_start(args, format);
    char message[512];
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    
    if (xSemaphoreTake(g_ctx.mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    // Add new event if space available
    if (g_ctx.events_count < g_ctx.events_capacity) {
        esp_analytics_event_data_t* event = &g_ctx.events[g_ctx.events_count];
        event->event_type = strdup(event_type);
        event->message = strdup(message);
        event->severity = strdup(severity);
        event->timestamp = get_current_timestamp_ms();
        g_ctx.events_count++;
    } else {
        // Replace oldest event
        if (g_ctx.events[0].event_type) free(g_ctx.events[0].event_type);
        if (g_ctx.events[0].message) free(g_ctx.events[0].message);
        if (g_ctx.events[0].severity) free(g_ctx.events[0].severity);
        
        g_ctx.events[0].event_type = strdup(event_type);
        g_ctx.events[0].message = strdup(message);
        g_ctx.events[0].severity = strdup(severity);
        g_ctx.events[0].timestamp = get_current_timestamp_ms();
    }
    
    xSemaphoreGive(g_ctx.mutex);
    
    // Also log to ESP log
    ESP_LOGI(TAG, "Event: %s - %s", event_type, message);
    
    return ESP_OK;
}

esp_err_t esp_analytics_log_info(const char* event_type, const char* message) {
    return esp_analytics_log("INFO", event_type, "%s", message);
}

esp_err_t esp_analytics_log_error(const char* event_type, const char* message) {
    return esp_analytics_log("ERROR", event_type, "%s", message);
}

esp_err_t esp_analytics_flush(void) {
    if (!g_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (xSemaphoreTake(g_ctx.mutex, pdMS_TO_TICKS(5000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    esp_err_t result = ESP_OK;
    
    if (g_ctx.metrics_count > 0 || g_ctx.events_count > 0) {
        // Ensure WiFi is connected
        esp_err_t wifi_err = ensure_wifi_connected();
        if (wifi_err != ESP_OK) {
            ESP_LOGE(TAG, "WiFi connection failed, cannot flush");
            result = wifi_err;
            goto cleanup;
        }
        
        // Create JSON payload
        cJSON* root = NULL;
        esp_err_t json_err = create_json_payload(&root);
        if (json_err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to create JSON payload");
            result = json_err;
            goto cleanup;
        }
        
        // Send HTTP request
        char* json_string = cJSON_PrintUnformatted(root);
        if (json_string == NULL) {
            ESP_LOGE(TAG, "Failed to serialize JSON");
            cJSON_Delete(root);
            result = ESP_ERR_NO_MEM;
            goto cleanup;
        }
        
        ESP_LOGI(TAG, "Sending analytics data: %s", json_string);
        
        esp_err_t http_err = send_http_request(json_string);
        if (http_err == ESP_OK) {
            // Clear buffers on successful transmission
            cleanup_metrics();
            cleanup_events();
            g_ctx.last_transmission = get_current_timestamp_ms();
        }
        
        free(json_string);
        cJSON_Delete(root);
        result = http_err;
    }
    
cleanup:
    xSemaphoreGive(g_ctx.mutex);
    return result;
}

bool esp_analytics_is_initialized(void) {
    return g_ctx.initialized;
}

esp_err_t esp_analytics_get_buffer_status(size_t* buffered_count, size_t* buffer_capacity) {
    if (!g_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (buffered_count == NULL || buffer_capacity == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (xSemaphoreTake(g_ctx.mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    *buffered_count = g_ctx.metrics_count;
    *buffer_capacity = g_ctx.metrics_capacity;
    
    xSemaphoreGive(g_ctx.mutex);
    return ESP_OK;
}

esp_err_t esp_analytics_deinit(void) {
    if (!g_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    // Flush any remaining data
    esp_analytics_flush();
    
    // Cleanup resources
    if (g_ctx.mutex) {
        vSemaphoreDelete(g_ctx.mutex);
    }
    
    if (g_ctx.http_client) {
        esp_http_client_cleanup(g_ctx.http_client);
    }
    
    cleanup_metrics();
    cleanup_events();
    
    if (g_ctx.config.server_url) {
        free((char*)g_ctx.config.server_url);
    }
    if (g_ctx.config.api_key) {
        free((char*)g_ctx.config.api_key);
    }
    if (g_ctx.config.device_id) {
        free((char*)g_ctx.config.device_id);
    }
    
    memset(&g_ctx, 0, sizeof(g_ctx));
    
    ESP_LOGI(TAG, "Analytics deinitialized");
    return ESP_OK;
}
