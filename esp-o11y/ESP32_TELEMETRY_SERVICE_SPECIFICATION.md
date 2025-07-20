# ESP32 Telemetry Collection Service - Developer Specification

## Project Overview

A self-hosted Kotlin Spring Boot service that collects telemetry data from ESP32 devices via UDP API calls and forwards the data to Grafana's collector. The service provides data validation, authentication, and comprehensive observability features.

## Requirements

### Functional Requirements

#### Data Collection
- **Protocol**: UDP-based API endpoint
- **Authentication**: API key-based authentication for ESP32 clients
- **Data Format**: Batch requests with structured payload:
  ```json
  {
    "otel": [...],      // OpenTelemetry metrics array
    "events": [...],    // Events array
    "metrics": [...]    // Prometheus-like metrics array
  }
  ```
- **Frequency**: Data collection every minute per device
- **Scale**: Designed to be extendable for growing device count
- **Volume**: ~20MB logs, 10-30 events per minute per device

#### Data Validation
- **Prometheus Metrics**: Whitelist-based filtering
  - Only allow pre-configured metric names
  - Drop non-whitelisted metrics with warning logs
- **OTEL Data**: Size limit enforcement
  - Configurable maximum size per batch
  - Truncate oversized data with warning logs
- **Events**: Array size limit enforcement
  - Configurable maximum array length
  - Truncate oversized arrays with warning logs

#### Data Forwarding
- Forward validated data to Grafana collector endpoint
- No local data storage (Grafana handles persistence)
- Optional buffering for reliability (nice-to-have feature)

#### Error Handling
- **Validation Failures**: Drop/truncate data, log warnings, continue processing
- **Partial Batch Processing**: Process valid portions of batch when possible
- **Network Failures**: Log errors, expose metrics for monitoring

### Non-Functional Requirements

#### Performance
- Near-real-time processing (within minutes)
- Handle concurrent requests from multiple ESP32 devices
- Minimal memory footprint for self-hosted deployment

#### Reliability
- Service should continue operating with partial failures
- Graceful degradation when Grafana collector is unavailable
- Comprehensive error logging for troubleshooting

#### Security
- API key authentication for ESP32 clients
- Input validation and sanitization
- Rate limiting considerations (future enhancement)

## Architecture

### Technology Stack
- **Language**: Kotlin
- **Framework**: Spring Boot 3.x
- **Build Tool**: Gradle
- **Protocol**: UDP
- **Serialization**: JSON (Jackson)
- **Logging**: Structured JSON logging (Logback)
- **Metrics**: Micrometer + Prometheus
- **Containerization**: Docker

### Component Architecture

```
┌─────────────────┐    UDP     ┌──────────────────┐    HTTP     ┌─────────────────┐
│   ESP32 Device  │ ---------> │  Telemetry       │ ----------> │   Grafana       │
│                 │            │  Service         │             │   Collector     │
└─────────────────┘            └──────────────────┘             └─────────────────┘
                                        │
                                        v
                               ┌──────────────────┐
                               │   Prometheus     │
                               │   Metrics        │
                               │   (Service       │
                               │   Monitoring)    │
                               └──────────────────┘
```

### Key Components

#### 1. UDP Controller
- `@Controller` handling UDP requests
- API key validation
- Request parsing and routing

#### 2. Validation Service
- Prometheus metric whitelist validation
- OTEL data size validation
- Events array size validation
- Validation metrics collection

#### 3. Forwarding Service
- HTTP client to Grafana collector
- Async processing for performance
- Error handling and retry logic (optional)

#### 4. Configuration Service
- Environment variable management
- Runtime configuration validation
- Dynamic configuration reloading (future)

#### 5. Observability Components
- Health check endpoints
- Internal metrics collection
- Structured logging

## Data Models

### Incoming Request Model
```kotlin
data class TelemetryBatchRequest(
    val apiKey: String,
    val deviceId: String,
    val timestamp: Instant,
    val otel: List<OtelMetric>?,
    val events: List<Event>?,
    val metrics: List<PrometheusMetric>?
)

data class OtelMetric(
    val name: String,
    val value: Double,
    val labels: Map<String, String>,
    val timestamp: Instant
)

data class Event(
    val type: String,
    val message: String,
    val severity: String,
    val timestamp: Instant,
    val metadata: Map<String, Any>?
)

data class PrometheusMetric(
    val name: String,
    val value: Double,
    val labels: Map<String, String>,
    val help: String?,
    val type: String? // counter, gauge, histogram, summary
)
```

### Configuration Model
```kotlin
@ConfigurationProperties("telemetry")
data class TelemetryConfig(
    val server: ServerConfig,
    val validation: ValidationConfig,
    val grafana: GrafanaConfig,
    val security: SecurityConfig
)

data class ServerConfig(
    val port: Int,
    val host: String
)

data class ValidationConfig(
    val prometheusWhitelist: Set<String>,
    val maxOtelBatchSize: Int,
    val maxEventsArraySize: Int
)

data class GrafanaConfig(
    val collectorUrl: String,
    val timeout: Duration,
    val retryEnabled: Boolean = false
)

data class SecurityConfig(
    val apiKeys: Set<String>
)
```

## Environment Variables

| Variable | Description | Default | Required |
|----------|-------------|---------|----------|
| `TELEMETRY_SERVER_PORT` | UDP server port | 8080 | No |
| `TELEMETRY_SERVER_HOST` | UDP server host | 0.0.0.0 | No |
| `TELEMETRY_PROMETHEUS_WHITELIST` | Comma-separated metric names | "" | Yes |
| `TELEMETRY_MAX_OTEL_BATCH_SIZE` | Max OTEL batch size (bytes) | 1048576 | No |
| `TELEMETRY_MAX_EVENTS_ARRAY_SIZE` | Max events array length | 100 | No |
| `TELEMETRY_GRAFANA_COLLECTOR_URL` | Grafana collector endpoint | "" | Yes |
| `TELEMETRY_GRAFANA_TIMEOUT` | Request timeout (seconds) | 30 | No |
| `TELEMETRY_GRAFANA_RETRY_ENABLED` | Enable retry logic | false | No |
| `TELEMETRY_API_KEYS` | Comma-separated API keys | "" | Yes |
| `LOGGING_LEVEL_ROOT` | Log level | INFO | No |

## API Specification

### Endpoints

#### Data Collection
- **Method**: UDP POST
- **Path**: `/api/v1/telemetry`
- **Content-Type**: `application/json`
- **Authentication**: API key in request body

#### Health Checks
- **Liveness**: `GET /actuator/health/liveness`
- **Readiness**: `GET /actuator/health/readiness`

#### Metrics
- **Prometheus**: `GET /actuator/prometheus`

### Request/Response Examples

#### Successful Request
```json
{
  "apiKey": "esp32-device-key-001",
  "deviceId": "esp32-001",
  "timestamp": "2025-07-20T10:30:00Z",
  "otel": [
    {
      "name": "system.cpu.usage",
      "value": 45.2,
      "labels": {"core": "0"},
      "timestamp": "2025-07-20T10:30:00Z"
    }
  ],
  "events": [
    {
      "type": "wifi.connected",
      "message": "Connected to WiFi network",
      "severity": "INFO",
      "timestamp": "2025-07-20T10:29:58Z",
      "metadata": {"ssid": "IoT-Network", "rssi": -45}
    }
  ],
  "metrics": [
    {
      "name": "esp32_temperature_celsius",
      "value": 23.5,
      "labels": {"sensor": "internal"},
      "help": "ESP32 internal temperature",
      "type": "gauge"
    }
  ]
}
```

#### Response (UDP - No explicit response, but logged)
```json
{
  "status": "accepted",
  "validatedMetrics": 1,
  "truncatedOtel": false,
  "truncatedEvents": false,
  "droppedPrometheusMetrics": 0
}
```

## Error Handling Strategy

### Validation Errors
- **Behavior**: Log warning, continue processing valid data
- **Metrics**: Increment validation failure counters
- **Example**: Non-whitelisted Prometheus metric → drop metric, log warning, process rest

### Network Errors
- **Behavior**: Log error, expose metrics for monitoring
- **Retry**: Optional retry logic for Grafana forwarding
- **Example**: Grafana unavailable → log error, increment failure counter

### Malformed Requests
- **Behavior**: Log error with request details
- **Response**: Silent (UDP nature)
- **Metrics**: Increment malformed request counter

### Resource Exhaustion
- **Behavior**: Implement circuit breaker pattern
- **Monitoring**: Memory and connection pool metrics
- **Graceful Degradation**: Reject new requests when overloaded

## Internal Metrics

The service exposes the following Prometheus metrics:

### Request Metrics
- `telemetry_requests_total{device_id, status}` - Total requests received
- `telemetry_request_duration_seconds{device_id}` - Request processing time
- `telemetry_request_size_bytes{device_id}` - Request payload size

### Validation Metrics
- `telemetry_validation_failures_total{type, reason}` - Validation failures
- `telemetry_prometheus_metrics_dropped_total{device_id}` - Dropped Prometheus metrics
- `telemetry_otel_truncations_total{device_id}` - OTEL data truncations
- `telemetry_events_truncations_total{device_id}` - Events array truncations

### Forwarding Metrics
- `telemetry_grafana_forwards_total{status}` - Grafana forwarding attempts
- `telemetry_grafana_forward_duration_seconds` - Forwarding request time
- `telemetry_grafana_errors_total{error_type}` - Grafana forwarding errors

### System Metrics
- `telemetry_memory_usage_bytes` - Service memory usage
- `telemetry_active_connections` - Active UDP connections

## Logging Strategy

### Log Format
- **Format**: Structured JSON
- **Fields**: timestamp, level, logger, message, context
- **Context**: deviceId, requestId, validation results

### Log Levels
- **ERROR**: System errors, Grafana forwarding failures
- **WARN**: Validation failures, data truncations
- **INFO**: Successful requests, service lifecycle events
- **DEBUG**: Detailed processing information

### Example Log Entries
```json
{
  "timestamp": "2025-07-20T10:30:01.123Z",
  "level": "WARN",
  "logger": "com.telemetry.validation.PrometheusValidator",
  "message": "Dropped non-whitelisted metric",
  "deviceId": "esp32-001",
  "metricName": "custom_sensor_reading",
  "requestId": "req-12345"
}

{
  "timestamp": "2025-07-20T10:30:01.456Z",
  "level": "INFO",
  "logger": "com.telemetry.controller.TelemetryController",
  "message": "Processed telemetry batch",
  "deviceId": "esp32-001",
  "otelMetrics": 5,
  "events": 2,
  "prometheusMetrics": 3,
  "processingTimeMs": 45,
  "requestId": "req-12345"
}
```

## Testing Strategy

### Unit Tests
#### Validation Components
- Test Prometheus whitelist filtering
- Test OTEL size limit enforcement
- Test events array truncation
- Test edge cases (empty arrays, null values)

#### Data Processing
- Test JSON deserialization
- Test data transformation for Grafana
- Test concurrent request handling

#### Configuration
- Test environment variable parsing
- Test configuration validation
- Test default value handling

### Integration Tests
#### API Endpoints
- Test UDP request handling
- Test authentication with valid/invalid API keys
- Test malformed request handling
- Test health check endpoints

#### External Dependencies
- Test Grafana collector integration (with mock)
- Test network failure scenarios
- Test timeout handling

### Performance Tests
#### Load Testing
- Test concurrent requests from multiple devices
- Test large payload handling
- Test memory usage under load
- Test UDP packet loss scenarios

#### Benchmarks
- Measure request processing latency
- Measure throughput (requests per second)
- Measure memory footprint
- Measure CPU usage

### Test Data
#### Sample Payloads
```kotlin
// Valid minimal payload
val minimalPayload = """
{
  "apiKey": "test-key",
  "deviceId": "test-device",
  "timestamp": "2025-07-20T10:30:00Z",
  "metrics": []
}
"""

// Large payload for size testing
val largePayload = // Generate programmatically

// Invalid payloads for error testing
val invalidPayloads = listOf(
  """{"invalid": "json}""",
  """{"apiKey": "invalid-key", ...}""",
  """{"apiKey": "valid-key", "deviceId": "", ...}"""
)
```

## Deployment

### Docker Configuration
```dockerfile
FROM openjdk:21-jdk-slim

LABEL maintainer="your-email@domain.com"
LABEL description="ESP32 Telemetry Collection Service"

# Create app directory
WORKDIR /app

# Copy application JAR
COPY build/libs/telemetry-service-*.jar app.jar

# Expose UDP port and health check port
EXPOSE 8080/udp
EXPOSE 8081

# Health check
HEALTHCHECK --interval=30s --timeout=3s --start-period=5s --retries=3 \
  CMD curl -f http://localhost:8081/actuator/health || exit 1

# Run application
ENTRYPOINT ["java", "-jar", "app.jar"]
```

### Environment Configuration Example
```bash
# Server Configuration
TELEMETRY_SERVER_PORT=8080
TELEMETRY_SERVER_HOST=0.0.0.0

# Validation Configuration
TELEMETRY_PROMETHEUS_WHITELIST=esp32_temperature_celsius,esp32_memory_usage_bytes,esp32_wifi_signal_strength
TELEMETRY_MAX_OTEL_BATCH_SIZE=1048576
TELEMETRY_MAX_EVENTS_ARRAY_SIZE=100

# Grafana Configuration
TELEMETRY_GRAFANA_COLLECTOR_URL=http://grafana:3000/api/v1/push
TELEMETRY_GRAFANA_TIMEOUT=30
TELEMETRY_GRAFANA_RETRY_ENABLED=false

# Security Configuration
TELEMETRY_API_KEYS=esp32-device-key-001,esp32-device-key-002,esp32-device-key-003

# Logging Configuration
LOGGING_LEVEL_ROOT=INFO
LOGGING_LEVEL_COM_TELEMETRY=DEBUG
```

## Development Setup

### Prerequisites
- JDK 21+
- Gradle 8+
- Docker (for containerization)
- IDE with Kotlin support (IntelliJ IDEA recommended)

### Project Structure
```
telemetry-service/
├── src/
│   ├── main/
│   │   ├── kotlin/
│   │   │   └── com/telemetry/
│   │   │       ├── TelemetryServiceApplication.kt
│   │   │       ├── controller/
│   │   │       │   └── TelemetryController.kt
│   │   │       ├── service/
│   │   │       │   ├── ValidationService.kt
│   │   │       │   └── ForwardingService.kt
│   │   │       ├── config/
│   │   │       │   └── TelemetryConfig.kt
│   │   │       └── model/
│   │   │           └── TelemetryModels.kt
│   │   └── resources/
│   │       ├── application.yml
│   │       └── logback-spring.xml
│   └── test/
│       └── kotlin/
│           └── com/telemetry/
│               ├── integration/
│               └── unit/
├── build.gradle.kts
├── Dockerfile
└── README.md
```

### Build Commands
```bash
# Build application
./gradlew build

# Run tests
./gradlew test

# Build Docker image
docker build -t telemetry-service .

# Run locally
./gradlew bootRun
```

## Monitoring and Operations

### Key Metrics to Monitor
- Request throughput and latency
- Validation failure rates
- Grafana forwarding success rates
- Memory and CPU usage
- UDP packet loss rates

### Alerting Recommendations
- High validation failure rate (>10%)
- Grafana forwarding failures (>5%)
- High memory usage (>80%)
- Service health check failures

### Troubleshooting Guide
1. **High validation failures**: Check whitelist configuration
2. **Grafana forwarding errors**: Verify network connectivity and endpoint
3. **Memory issues**: Review payload sizes and batch processing
4. **UDP packet loss**: Check network configuration and server capacity

This specification provides all necessary details for immediate implementation. The developer can proceed with setting up the Kotlin Spring Boot project and implementing the components as outlined.