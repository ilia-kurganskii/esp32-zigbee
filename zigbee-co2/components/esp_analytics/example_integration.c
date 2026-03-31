/*
 * Example: Integrating ESP Analytics with Zigbee CO2 Sensor
 * 
 * This file shows how to integrate the esp_analytics component
 * into the existing zigbee-co2 project.
 */

#include "esp_analytics.h"
#include "esp_log.h"
#include "esp_system.h"
#include "scd40.h"
#include "esp_wifi.h"
#include "esp_zigbee_core.h"

static const char *TAG = "analytics_example";

/**
 * @brief Initialize analytics component with configuration
 */
esp_err_t analytics_init_example(void)
{
    ESP_LOGI(TAG, "Initializing analytics component");
    
    // Configuration can be loaded from Kconfig or set programmatically
    esp_analytics_config_t config = {
        .server_url = CONFIG_ESP_ANALYTICS_SERVER_URL,
        .api_key = CONFIG_ESP_ANALYTICS_API_KEY,
        .device_id = CONFIG_ESP_ANALYTICS_DEVICE_ID,
        .transmission_interval_sec = CONFIG_ESP_ANALYTICS_TRANSMISSION_INTERVAL_SEC,
        .enable_deep_sleep = CONFIG_ESP_ANALYTICS_ENABLE_DEEP_SLEEP,
        .buffer_size = CONFIG_ESP_ANALYTICS_BUFFER_SIZE
    };
    
    esp_err_t err = esp_analytics_init(&config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize analytics: %s", esp_err_to_name(err));
        return err;
    }
    
    // Log initialization event
    esp_analytics_log_info("system_init", "Analytics component initialized successfully");
    
    return ESP_OK;
}

/**
 * @brief Example of tracking sensor readings
 */
void analytics_track_sensor_reading(uint16_t co2_ppm, float temperature, float humidity)
{
    // Increment sensor reading counter
    esp_analytics_increase(ESP_ANALYTICS_METRIC_SENSOR_READINGS);
    
    // Log the sensor reading
    esp_analytics_log_info("sensor_reading", 
                          "CO2: %d ppm, Temp: %.1f°C, Humidity: %.1f%%", 
                          co2_ppm, temperature, humidity);
    
    // Update memory usage
    esp_analytics_set(ESP_ANALYTICS_METRIC_MEMORY_USAGE, esp_get_free_heap_size());
}

/**
 * @brief Example of tracking WiFi events
 */
void analytics_track_wifi_event(const char* event_type, bool success)
{
    esp_analytics_increase(ESP_ANALYTICS_METRIC_WIFI_CONNECTIONS);
    
    if (success) {
        esp_analytics_log_info("wifi_event", "WiFi %s successful", event_type);
    } else {
        esp_analytics_log_error("wifi_error", "WiFi %s failed", event_type);
        esp_analytics_increase(ESP_ANALYTICS_METRIC_ERROR_COUNT);
    }
}

/**
 * @brief Example of tracking Zigbee messages
 */
void analytics_track_zigbee_message(const char* message_type, uint16_t cluster_id)
{
    esp_analytics_increase(ESP_ANALYTICS_METRIC_ZIGBEE_MESSAGES);
    
    esp_analytics_log_info("zigbee_message", 
                          "Type: %s, Cluster: 0x%04X", 
                          message_type, cluster_id);
}

/**
 * @brief Example of tracking battery level (if battery monitoring is available)
 */
void analytics_track_battery_level(uint8_t battery_percent)
{
    esp_analytics_set(ESP_ANALYTICS_METRIC_BATTERY_LEVEL, (double)battery_percent);
    
    if (battery_percent < 20) {
        esp_analytics_log_error("battery_warning", "Low battery: %d%%", battery_percent);
    } else if (battery_percent < 50) {
        esp_analytics_log_info("battery_status", "Battery level: %d%%", battery_percent);
    }
}

/**
 * @brief Example of tracking system errors
 */
void analytics_track_system_error(const char* error_context, const char* error_details)
{
    esp_analytics_increase(ESP_ANALYTICS_METRIC_ERROR_COUNT);
    esp_analytics_log_error("system_error", "%s: %s", error_context, error_details);
}

/**
 * @brief Example of periodic analytics transmission
 */
void analytics_periodic_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Analytics periodic task started");
    
    while (1) {
        // Wait for transmission interval
        vTaskDelay(pdMS_TO_TICKS(CONFIG_ESP_ANALYTICS_TRANSMISSION_INTERVAL_SEC * 1000));
        
        // Check if analytics is initialized
        if (!esp_analytics_is_initialized()) {
            ESP_LOGW(TAG, "Analytics not initialized, skipping transmission");
            continue;
        }
        
        // Get buffer status
        size_t buffered_count, buffer_capacity;
        esp_err_t err = esp_analytics_get_buffer_status(&buffered_count, &buffer_capacity);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Analytics buffer: %zu/%zu metrics", buffered_count, buffer_capacity);
        }
        
        // Force transmission if we have data
        if (buffered_count > 0) {
            ESP_LOGI(TAG, "Transmitting analytics data");
            err = esp_analytics_flush();
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Failed to transmit analytics: %s", esp_err_to_name(err));
            }
        }
    }
}

/**
 * @brief Integration example for the main CO2 sensor loop
 */
void analytics_integration_example_main_loop(void)
{
    // Initialize analytics at startup
    analytics_init_example();
    
    // Start periodic analytics task
    xTaskCreate(analytics_periodic_task, "analytics_task", 2048, NULL, 5, NULL);
    
    // Example integration in sensor reading loop
    while (1) {
        uint16_t co2_ppm;
        float temperature, humidity;
        
        // Read sensor (existing code)
        esp_err_t err = scd40_read_measurement(&co2_ppm, &temperature, &humidity);
        
        if (err == ESP_OK) {
            // Track successful reading
            analytics_track_sensor_reading(co2_ppm, temperature, humidity);
            
            // Existing Zigbee reporting code...
            
        } else {
            // Track sensor error
            analytics_track_system_error("sensor_read", esp_err_to_name(err));
        }
        
        // Sleep between readings (existing deep sleep code)
        // ...
    }
}

/**
 * @brief WiFi event handler with analytics integration
 */
void analytics_wifi_event_handler(void* handler_args, esp_event_base_t base, 
                                 int32_t event_id, void* event_data)
{
    switch (event_id) {
        case WIFI_EVENT_STA_START:
            analytics_track_wifi_event("station_start", true);
            break;
            
        case WIFI_EVENT_STA_CONNECTED:
            analytics_track_wifi_event("station_connected", true);
            break;
            
        case WIFI_EVENT_STA_DISCONNECTED:
            analytics_track_wifi_event("station_disconnected", false);
            break;
            
        default:
            break;
    }
}

/**
 * @brief Zigbee signal handler with analytics integration
 */
void analytics_zigbee_signal_handler(esp_zb_app_signal_t signal)
{
    switch (signal.sig_type) {
        case ESP_ZB_ZDO_SIGNAL_DEVICE_START:
            analytics_track_zigbee_message("device_start", 0);
            break;
            
        case ESP_ZB_ZCL_SIGNAL_DEVICE_REPORT_ATTRIB:
            analytics_track_zigbee_message("attribute_report", signal.sig_data.zcl_signal.cluster_id);
            break;
            
        default:
            break;
    }
}

/**
 * @brief Cleanup analytics before deep sleep
 */
void analytics_cleanup_before_sleep(void)
{
    ESP_LOGI(TAG, "Cleaning up analytics before deep sleep");
    
    // Flush any pending analytics data
    esp_err_t err = esp_analytics_flush();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to flush analytics before sleep: %s", esp_err_to_name(err));
    }
    
    // Note: Don't deinitialize if you want to preserve metrics across sleep
    // esp_analytics_deinit();  // Only call this on final shutdown
}
