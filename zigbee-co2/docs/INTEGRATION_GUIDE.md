# ESP Analytics Component Integration Guide

This guide explains how to integrate the ESP Analytics component into your ESP32 projects, specifically for the ESP32-C6 with power optimization requirements.

## Overview

The ESP Analytics component provides a power-efficient way to collect and transmit analytics data from ESP32 devices to a telemetry service using HTTP endpoints that comply with OpenAPI specifications.

## Key Features

- ✅ **Power Efficient**: Deep sleep support, adaptive transmission intervals
- ✅ **OpenAPI Compliant**: Compatible with existing telemetry service
- ✅ **Configurable**: Server URL, API key, transmission settings
- ✅ **Simple API**: `init()`, `increase()`, `log()` functions as requested
- ✅ **Robust**: Error handling, retry logic, network failure recovery

## Quick Integration Steps

### 1. Add Component to Project

The component is already included in this project under `components/esp_analytics/`.

### 2. Configure Build System

The main `CMakeLists.txt` has been updated to include the component:

```cmake
PRIV_REQUIRES scd40 nvs_flash esp_timer driver led_signal esp_analytics
```

### 3. Configure Settings

Add to your `sdkconfig` or use `idf.py menuconfig`:

```
CONFIG_ESP_ANALYTICS_SERVER_URL="http://your-server.com/api/v1/telemetry"
CONFIG_ESP_ANALYTICS_API_KEY="your-api-key"
CONFIG_ESP_ANALYTICS_DEVICE_ID="esp32-co2-sensor-001"
CONFIG_ESP_ANALYTICS_TRANSMISSION_INTERVAL_SEC=300
CONFIG_ESP_ANALYTICS_ENABLE_DEEP_SLEEP=y
CONFIG_ESP_ANALYTICS_BUFFER_SIZE=100
```

### 4. Include Header

In your main application file:

```c
#include "esp_analytics.h"
```

### 5. Initialize Component

```c
void app_main(void)
{
    // Initialize analytics
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
        ESP_LOGE("main", "Failed to initialize analytics: %s", esp_err_to_name(err));
        return;
    }
    
    esp_analytics_log_info("system_init", "Analytics component initialized");
    
    // Your existing application code...
}
```

## Integration Examples

### Zigbee CO2 Sensor Integration

Here's how to integrate analytics into the existing zigbee-co2 project:

```c
#include "esp_analytics.h"
#include "scd40.h"

// In your sensor reading function
void read_and_report_co2(void)
{
    uint16_t co2_ppm;
    float temperature, humidity;
    
    esp_err_t err = scd40_read_measurement(&co2_ppm, &temperature, &humidity);
    
    if (err == ESP_OK) {
        // Track successful sensor reading
        esp_analytics_increase(ESP_ANALYTICS_METRIC_SENSOR_READINGS);
        esp_analytics_log_info("co2_reading", 
                              "CO2: %d ppm, Temp: %.1f°C, Humidity: %.1f%%", 
                              co2_ppm, temperature, humidity);
        
        // Update system metrics
        esp_analytics_set(ESP_ANALYTICS_METRIC_MEMORY_USAGE, esp_get_free_heap_size());
        
        // Your existing Zigbee reporting code...
        
    } else {
        // Track error
        esp_analytics_log_error("sensor_error", "Failed to read CO2 sensor");
        esp_analytics_increase(ESP_ANALYTICS_METRIC_ERROR_COUNT);
    }
}
```

### WiFi Event Tracking

```c
void wifi_event_handler(void* handler_args, esp_event_base_t base, 
                       int32_t event_id, void* event_data)
{
    switch (event_id) {
        case WIFI_EVENT_STA_CONNECTED:
            esp_analytics_increase(ESP_ANALYTICS_METRIC_WIFI_CONNECTIONS);
            esp_analytics_log_info("wifi_connected", "WiFi connection established");
            break;
            
        case WIFI_EVENT_STA_DISCONNECTED:
            esp_analytics_log_error("wifi_disconnected", "WiFi connection lost");
            esp_analytics_increase(ESP_ANALYTICS_METRIC_ERROR_COUNT);
            break;
    }
}
```

### Zigbee Message Tracking

```c
void zigbee_message_handler(uint16_t cluster_id, const char* message_type)
{
    esp_analytics_increase(ESP_ANALYTICS_METRIC_ZIGBEE_MESSAGES);
    esp_analytics_log_info("zigbee_msg", 
                          "Type: %s, Cluster: 0x%04X", 
                          message_type, cluster_id);
}
```

## Power Management Integration

### Deep Sleep Integration

```c
void enter_deep_sleep_with_analytics(void)
{
    // Flush analytics before sleep
    esp_analytics_flush();
    
    // Enter deep sleep (existing code)
    esp_deep_sleep_start();
}

void wakeup_from_deep_sleep(void)
{
    // Analytics will automatically track restart count
    esp_analytics_log_info("system_wakeup", "Woke up from deep sleep");
    
    // Resume normal operation...
}
```

### Periodic Transmission Task

```c
void analytics_transmission_task(void *pvParameters)
{
    while (1) {
        // Wait for transmission interval
        vTaskDelay(pdMS_TO_TICKS(CONFIG_ESP_ANALYTICS_TRANSMISSION_INTERVAL_SEC * 1000));
        
        // Check if we have data to send
        size_t buffered_count, buffer_capacity;
        if (esp_analytics_get_buffer_status(&buffered_count, &buffer_capacity) == ESP_OK) {
            if (buffered_count > 0) {
                ESP_LOGI("analytics", "Transmitting %zu metrics", buffered_count);
                esp_analytics_flush();
            }
        }
    }
}

// Start the task in app_main()
xTaskCreate(analytics_transmission_task, "analytics_tx", 2048, NULL, 5, NULL);
```

## Available Metrics

| Metric | Type | Usage Example |
|--------|------|---------------|
| `ESP_ANALYTICS_METRIC_UPTIME_SECONDS` | counter | Automatically tracked |
| `ESP_ANALYTICS_METRIC_RESTART_COUNT` | counter | Automatically tracked |
| `ESP_ANALYTICS_METRIC_WIFI_CONNECTIONS` | counter | `esp_analytics_increase()` |
| `ESP_ANALYTICS_METRIC_ZIGBEE_MESSAGES` | counter | `esp_analytics_increase()` |
| `ESP_ANALYTICS_METRIC_SENSOR_READINGS` | counter | `esp_analytics_increase()` |
| `ESP_ANALYTICS_METRIC_ERROR_COUNT` | counter | `esp_analytics_increase()` |
| `ESP_ANALYTICS_METRIC_BATTERY_LEVEL` | gauge | `esp_analytics_set()` |
| `ESP_ANALYTICS_METRIC_MEMORY_USAGE` | gauge | `esp_analytics_set()` |

## API Reference Summary

### Initialization
```c
esp_err_t esp_analytics_init(const esp_analytics_config_t* config);
esp_err_t esp_analytics_init_simple(const char* server_url, const char* api_key);
```

### Metrics
```c
esp_err_t esp_analytics_increase(esp_analytics_metric_t metric);
esp_err_t esp_analytics_increase_by(esp_analytics_metric_t metric, double value);
esp_err_t esp_analytics_set(esp_analytics_metric_t metric, double value);
```

### Logging
```c
esp_err_t esp_analytics_log(const char* severity, const char* event_type, const char* format, ...);
esp_err_t esp_analytics_log_info(const char* event_type, const char* message);
esp_err_t esp_analytics_log_error(const char* event_type, const char* message);
```

### Control
```c
esp_err_t esp_analytics_flush(void);
esp_err_t esp_analytics_deinit(void);
```

## Testing

### Basic Test

To verify functionality, you can add simple test code to your application:

```c
// In your app_main() for testing
esp_err_t err = esp_analytics_init_simple(
    "http://test-server.com/api/v1/telemetry",
    "test-api-key"
);

if (err == ESP_OK) {
    esp_analytics_increase(ESP_ANALYTICS_METRIC_SENSOR_READINGS);
    esp_analytics_log_info("test", "Integration test successful");
    esp_analytics_deinit();
}
```

### Integration Test

Add to your existing application:

```c
// After successful sensor reading
esp_analytics_increase(ESP_ANALYTICS_METRIC_SENSOR_READINGS);
esp_analytics_log_info("test", "Integration test successful");
```

## Troubleshooting

### Common Issues

1. **Compilation Errors**
   - Ensure `cjson` component is available
   - Check that all required components are in CMakeLists.txt

2. **Runtime Errors**
   - Check server URL is accessible
   - Verify API key authentication
   - Monitor logs for detailed error messages

3. **Power Issues**
   - Ensure deep sleep configuration is correct
   - Check transmission intervals are appropriate for power source

### Debug Logging

Enable debug logging:
```
CONFIG_ESP_ANALYTICS_LOG_LEVEL=4
```

Monitor for these log messages:
- `Analytics initialized successfully`
- `Event: [event_type] - [message]`
- `Sending analytics data: [json_payload]`
- `Analytics data sent successfully, status: 200`

## Data Format

The component sends JSON data in this format:

```json
{
  "deviceId": "esp32-co2-sensor-001",
  "timestamp": "2024-03-31T23:17:00Z",
  "metrics": [
    {
      "name": "sensor_readings_count",
      "value": 150.0,
      "type": "counter",
      "labels": {
        "device_type": "esp32c6",
        "device_id": "esp32-co2-sensor-001"
      }
    }
  ],
  "events": [
    {
      "type": "co2_reading",
      "message": "CO2: 450 ppm, Temp: 22.5°C, Humidity: 45.2%",
      "severity": "INFO",
      "timestamp": "2024-03-31T23:17:00Z"
    }
  ],
  "otel": []
}
```

## Performance Considerations

- **Memory Usage**: ~2KB RAM for buffers (configurable)
- **Flash Usage**: ~15KB flash storage
- **Power Consumption**: <1mA in deep sleep, ~80mA during transmission
- **Network Usage**: ~1-5KB per transmission

## Best Practices

1. **Initialize Early**: Call `esp_analytics_init()` early in `app_main()`
2. **Track Errors**: Use `esp_analytics_log_error()` for error conditions
3. **Monitor Memory**: Periodically update memory usage metric
4. **Flush Before Sleep**: Call `esp_analytics_flush()` before deep sleep
5. **Use Appropriate Intervals**: Configure transmission based on power source
6. **Handle Failures Gracefully**: Check return values and handle errors

## Support

For issues or questions:
1. Check the log output for error messages
2. Verify server connectivity and API key
3. Review configuration settings
4. Test with the provided test functions
