# ESP Analytics Component

A power-efficient ESP-IDF component for collecting and transmitting analytics data from ESP32 devices to a telemetry service using OpenAPI-compliant HTTP endpoints.

## Features

- **Power Optimized**: Deep sleep support, adaptive transmission intervals, WiFi power saving
- **OpenAPI Compliant**: Compatible with telemetry service specification
- **Configurable**: Server URL, API key, transmission intervals via NVS
- **Predefined Metrics**: Device uptime, restarts, WiFi, Zigbee, sensors, errors, battery, memory
- **Simple API**: `init()`, `increase()`, `log()` functions as requested
- **Robust**: Retry logic, error handling, network failure recovery

## Quick Start

### Basic Usage

```c
#include "esp_analytics.h"

// Initialize with server URL and API key
esp_analytics_init_simple("http://your-server.com/api/v1/telemetry", "your-api-key");

// Increment metrics
esp_analytics_increase(ESP_ANALYTICS_METRIC_SENSOR_READINGS);
esp_analytics_increase_by(ESP_ANALYTICS_METRIC_WIFI_CONNECTIONS, 2.0);

// Log events
esp_analytics_log_info("sensor_read", "CO2 sensor reading completed");
esp_analytics_log_error("sensor_error", "Failed to read temperature sensor");

// Force immediate transmission (bypasses power saving)
esp_analytics_flush();

// Cleanup when done
esp_analytics_deinit();
```

### Advanced Configuration

```c
esp_analytics_config_t config = {
    .server_url = "https://telemetry.example.com/api/v1/telemetry",
    .api_key = "your-secure-api-key",
    .device_id = "esp32-co2-sensor-001",
    .transmission_interval_sec = 600,  // 10 minutes
    .enable_deep_sleep = true,
    .buffer_size = 200
};

esp_analytics_init(&config);
```

## API Reference

### Initialization

```c
esp_err_t esp_analytics_init(const esp_analytics_config_t* config);
esp_err_t esp_analytics_init_simple(const char* server_url, const char* api_key);
```

### Metrics

```c
// Increment by 1
esp_err_t esp_analytics_increase(esp_analytics_metric_t metric);

// Increment by specific value
esp_err_t esp_analytics_increase_by(esp_analytics_metric_t metric, double value);

// Set to specific value
esp_err_t esp_analytics_set(esp_analytics_metric_t metric, double value);
```

### Events

```c
// Formatted logging
esp_err_t esp_analytics_log(const char* severity, const char* event_type, const char* format, ...);

// Convenience methods
esp_err_t esp_analytics_log_info(const char* event_type, const char* message);
esp_err_t esp_analytics_log_error(const char* event_type, const char* message);
```

### Control

```c
// Force immediate transmission
esp_err_t esp_analytics_flush(void);

// Check buffer status
esp_err_t esp_analytics_get_buffer_status(size_t* buffered_count, size_t* buffer_capacity);

// Cleanup
esp_err_t esp_analytics_deinit(void);
```

## Predefined Metrics

| Metric | Type | Description |
|--------|------|-------------|
| `ESP_ANALYTICS_METRIC_UPTIME_SECONDS` | counter | Device uptime in seconds |
| `ESP_ANALYTICS_METRIC_RESTART_COUNT` | counter | Number of device restarts |
| `ESP_ANALYTICS_METRIC_WIFI_CONNECTIONS` | counter | WiFi connection attempts |
| `ESP_ANALYTICS_METRIC_ZIGBEE_MESSAGES` | counter | Zigbee messages sent/received |
| `ESP_ANALYTICS_METRIC_SENSOR_READINGS` | counter | Total sensor readings |
| `ESP_ANALYTICS_METRIC_ERROR_COUNT` | counter | Error occurrences |
| `ESP_ANALYTICS_METRIC_BATTERY_LEVEL` | gauge | Battery percentage (if available) |
| `ESP_ANALYTICS_METRIC_MEMORY_USAGE` | gauge | Free heap memory in bytes |

## Configuration

### Kconfig Options

Add to your project's `sdkconfig` or configure via `idf.py menuconfig`:

```
CONFIG_ESP_ANALYTICS_SERVER_URL="http://your-server.com/api/v1/telemetry"
CONFIG_ESP_ANALYTICS_API_KEY="your-api-key"
CONFIG_ESP_ANALYTICS_DEVICE_ID="esp32-device"
CONFIG_ESP_ANALYTICS_TRANSMISSION_INTERVAL_SEC=300
CONFIG_ESP_ANALYTICS_ENABLE_DEEP_SLEEP=y
CONFIG_ESP_ANALYTICS_BUFFER_SIZE=100
CONFIG_ESP_ANALYTICS_ENABLE_WIFI_POWER_SAVE=y
CONFIG_ESP_ANALYTICS_RETRY_ATTEMPTS=3
```

### NVS Storage

The component automatically saves configuration to NVS for persistence across reboots.

## Power Management

### Deep Sleep Integration

When `enable_deep_sleep` is true, the component:
- Buffers data in memory
- Enters deep sleep between transmissions
- Uses RTC timer for wake-up
- Preserves critical metrics across sleep cycles

### Adaptive Transmission

- **Battery-powered**: 15-30 minute intervals
- **USB-powered**: 1-5 minute intervals  
- **Mains-powered**: 30-60 second intervals

### WiFi Optimization

- Connects only when transmitting data
- Uses power saving modes
- Disconnects immediately after transmission
- Implements retry logic with exponential backoff

## Data Format

The component sends JSON payloads compliant with the telemetry service OpenAPI specification:

```json
{
  "deviceId": "esp32-device-001",
  "timestamp": "2024-03-31T23:17:00Z",
  "metrics": [
    {
      "name": "device_uptime_seconds",
      "value": 3600.0,
      "type": "counter",
      "labels": {
        "device_type": "esp32c6",
        "device_id": "esp32-device-001"
      }
    }
  ],
  "events": [
    {
      "type": "sensor_read",
      "message": "CO2 reading: 450 ppm",
      "severity": "INFO",
      "timestamp": "2024-03-31T23:17:00Z"
    }
  ],
  "otel": []
}
```

## Integration Examples

### Zigbee CO2 Sensor Integration

```c
void app_main() {
    // Initialize analytics
    esp_analytics_init_simple(
        CONFIG_ESP_ANALYTICS_SERVER_URL,
        CONFIG_ESP_ANALYTICS_API_KEY
    );
    
    // Your existing sensor initialization code...
    
    while (1) {
        // Read sensor
        uint16_t co2_ppm;
        esp_err_t err = scd40_read_measurement(&co2_ppm, NULL, NULL);
        
        if (err == ESP_OK) {
            // Increment sensor reading metric
            esp_analytics_increase(ESP_ANALYTICS_METRIC_SENSOR_READINGS);
            
            // Log the reading
            esp_analytics_log_info("co2_reading", "CO2: %d ppm", co2_ppm);
        } else {
            // Log error
            esp_analytics_log_error("sensor_error", "Failed to read CO2 sensor");
            esp_analytics_increase(ESP_ANALYTICS_METRIC_ERROR_COUNT);
        }
        
        // Update battery level if available
        esp_analytics_set(ESP_ANALYTICS_METRIC_BATTERY_LEVEL, get_battery_percentage());
        
        // Update memory usage
        esp_analytics_set(ESP_ANALYTICS_METRIC_MEMORY_USAGE, esp_get_free_heap_size());
        
        // Sleep between readings
        vTaskDelay(pdMS_TO_TICKS(30000)); // 30 seconds
    }
}
```

## Error Handling

The component handles various error conditions gracefully:

- **Network Failures**: Exponential backoff retry, local buffering
- **Memory Constraints**: Circular buffer, oldest data replacement
- **Configuration Issues**: Default fallback values, validation
- **Server Unavailable**: Continue operation, retry later

## Dependencies

- `esp_http_client` - HTTP communication
- `nvs_flash` - Configuration storage  
- `freertos` - Task management
- `esp_log` - Logging
- `esp_wifi` - WiFi connectivity
- `esp_sleep` - Power management
- `esp_timer` - Timing
- `cjson` - JSON serialization

## Performance

- **Memory Usage**: ~2KB RAM for buffers (configurable)
- **Flash Usage**: ~15KB flash storage
- **Power Consumption**: <1mA in deep sleep, ~80mA during transmission
- **Network Usage**: ~1-5KB per transmission (depends on buffer size)

## Troubleshooting

### Common Issues

1. **WiFi Connection Failed**
   - Check WiFi credentials and network availability
   - Ensure WiFi is initialized before calling analytics functions

2. **HTTP Request Failed**
   - Verify server URL is accessible
   - Check API key authentication
   - Monitor server logs for request details

3. **Memory Allocation Failed**
   - Reduce buffer size in configuration
   - Check available heap memory
   - Use `esp_analytics_flush()` more frequently

### Debug Logging

Enable debug logging:
```
CONFIG_ESP_ANALYTICS_LOG_LEVEL=4
```

Monitor logs:
```
I (xxx) esp_analytics: Analytics initialized successfully
I (xxx) esp_analytics: Event: co2_reading - CO2: 450 ppm
I (xxx) esp_analytics: Sending analytics data: {"deviceId":...}
I (xxx) esp_analytics: Analytics data sent successfully, status: 200
```
