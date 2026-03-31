# ESP32 Telemetry Service - Agent Guide

## Quick Start
```bash
# Set up Java environment
export PATH="/opt/homebrew/opt/openjdk@21/bin:$PATH"
export JAVA_HOME="/opt/homebrew/opt/openjdk@21"

# Run the service
./gradlew bootRun

# Health check
curl http://localhost:8080/actuator/health
```

## Project Overview
- **Purpose**: UDP telemetry collection service for ESP32 devices
- **Tech Stack**: Spring Boot 3.2.0, Kotlin 1.9.20, Java 21
- **Ports**: HTTP 8080, UDP 8080
- **Build**: Gradle 8.5 (use gradlew, not system gradle)

## Key Components
- `TelemetryController` - UDP endpoint for telemetry data
- `ValidationService` - Data validation and filtering
- `MetricsService` - Internal metrics collection
- `SecurityConfig` - API key authentication

## Configuration
- **File**: `src/main/resources/application.yml`
- **Environment**: Override with env vars (prefixed with `TELEMETRY_`)
- **Required**: `TELEMETRY_API_KEYS`, `TELEMETRY_GRAFANA_COLLECTOR_URL`

## API Endpoints
- **Health**: `GET /actuator/health`
- **Metrics**: `GET /actuator/prometheus`
- **Telemetry**: UDP POST `/api/v1/telemetry`

## Testing
```bash
# Build (skip tests if needed)
./gradlew build -x test

# Run tests
./gradlew test

# Integration tests
./gradlew integrationTest
```

## Common Issues
- **Java version**: Must use Java 21 (OpenJDK 21)
- **Gradle**: Use `./gradlew` not system gradle
- **Tests**: Memory health tests may fail due to locale formatting

## Development Notes
- Structured JSON logging enabled
- Prometheus metrics auto-configured
- Security uses generated password in dev mode
- UDP endpoint needs proper API key in request body
