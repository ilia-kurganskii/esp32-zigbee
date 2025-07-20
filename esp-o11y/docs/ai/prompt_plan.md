# ESP32 Telemetry Service - Implementation Prompt Plan

## Project Blueprint Overview

This document provides a detailed, step-by-step implementation plan for the ESP32 Telemetry Collection Service. The plan is broken down into small, testable increments that build upon each other, ensuring safe implementation with comprehensive testing at each stage.

## Development Philosophy

- **Test-Driven Development**: Each step includes test implementation before production code
- **Incremental Progress**: Each step builds on the previous step's foundation
- **Integration-First**: No orphaned code - everything integrates immediately
- **Safety**: Small steps minimize risk and allow for easy rollback
- **Best Practices**: Follow Kotlin/Spring Boot conventions throughout

## Implementation Phases

### Phase 1: Foundation Setup (Steps 1-3)
Basic project structure, build configuration, and minimal application

### Phase 2: Core Models and Configuration (Steps 4-6)
Data models, configuration management, and validation foundations

### Phase 3: Basic API Framework (Steps 7-9)
HTTP endpoints, request handling, and basic response structure

### Phase 4: UDP Implementation (Steps 10-12)
UDP protocol support, packet handling, and integration

### Phase 5: Validation Engine (Steps 13-15)
Data validation, filtering, and error handling

### Phase 6: External Integration (Steps 16-18)
Grafana forwarding, HTTP client, and retry logic

### Phase 7: Observability (Steps 19-21)
Metrics collection, health checks, and structured logging

### Phase 8: Production Readiness (Steps 22-24)
Dockerization, performance optimization, and final integration

---

## Implementation Steps

### Step 1: Project Bootstrap and Build Configuration

**Context**: Set up the basic Kotlin Spring Boot project with proper dependencies and build configuration.

**Deliverable**: Working Gradle build with minimal Spring Boot application that starts successfully.

```
Create a new Kotlin Spring Boot project for the ESP32 Telemetry Service with the following requirements:

1. Use Spring Boot 3.2.x with Kotlin 1.9.x
2. Set up Gradle build with Kotlin DSL (build.gradle.kts)
3. Include these core dependencies:
   - spring-boot-starter-web
   - spring-boot-starter-actuator
   - spring-boot-starter-validation
   - spring-boot-configuration-processor
   - jackson-module-kotlin
   - micrometer-registry-prometheus
   - spring-boot-starter-test
   - mockk (for Kotlin testing)

4. Create the main application class `TelemetryServiceApplication.kt` with proper Spring Boot annotations
5. Configure application.yml with basic server settings (port 8080)
6. Create a simple health check endpoint that returns "UP" status
7. Write a basic integration test that starts the application context and verifies the health endpoint

Project structure should be:
```
src/
├── main/
│   ├── kotlin/
│   │   └── com/telemetry/
│   │       └── TelemetryServiceApplication.kt
│   └── resources/
│       └── application.yml
└── test/
    └── kotlin/
        └── com/telemetry/
            └── TelemetryServiceApplicationTest.kt
```

Ensure the application starts cleanly with `./gradlew bootRun` and tests pass with `./gradlew test`.
```

### Step 2: Configuration Management Foundation

**Context**: Build upon the basic application to add comprehensive configuration management using Spring Boot's configuration properties.

**Deliverable**: Robust configuration system with validation and environment variable support.

```
Extend the ESP32 Telemetry Service with comprehensive configuration management:

1. Create configuration data classes in `src/main/kotlin/com/telemetry/config/`:
   - `TelemetryConfig.kt` - Main configuration container with @ConfigurationProperties
   - `ServerConfig.kt` - Server-specific settings (host, port)
   - `ValidationConfig.kt` - Validation rules (whitelist, size limits)
   - `GrafanaConfig.kt` - Grafana collector settings
   - `SecurityConfig.kt` - API key management

2. Use proper Kotlin data classes with validation annotations:
   - @NotEmpty for required strings
   - @Positive for positive numbers
   - @Valid for nested configurations

3. Create `application.yml` with default values and clear documentation
4. Support environment variable overrides using Spring Boot conventions
5. Create `@Configuration` class to expose config beans
6. Write comprehensive unit tests for configuration parsing and validation
7. Create integration test that verifies configuration loading from environment variables

Ensure all configurations have sensible defaults and clear error messages for invalid values. The configuration should be immutable and thread-safe.
```

### Step 3: Data Models and Serialization

**Context**: Define the core data models for telemetry data exchange and implement proper JSON serialization.

**Deliverable**: Complete data model definitions with Jackson serialization support and validation.

```
Create the core data models for the ESP32 Telemetry Service:

1. Create data model classes in `src/main/kotlin/com/telemetry/model/`:
   - `TelemetryBatchRequest.kt` - Main request container
   - `OtelMetric.kt` - OpenTelemetry metric representation
   - `Event.kt` - Event log representation  
   - `PrometheusMetric.kt` - Prometheus-style metric
   - `TelemetryResponse.kt` - Response wrapper (for testing/logging)

2. Implement proper JSON serialization using Jackson:
   - Configure Jackson for Kotlin with proper time handling
   - Use @JsonProperty for field mapping
   - Handle optional fields with nullable types
   - Configure Instant serialization to ISO-8601

3. Add validation annotations:
   - @Valid for nested objects
   - @NotBlank for required strings  
   - @NotNull for required fields
   - Custom validation for metric types and values

4. Create comprehensive unit tests for:
   - JSON serialization/deserialization
   - Validation of required fields
   - Handling of optional/null fields
   - Edge cases (empty arrays, special characters)
   - Date/time parsing and formatting

5. Create test fixtures with sample JSON payloads for various scenarios

Ensure all models are immutable, thread-safe, and properly handle edge cases in real-world ESP32 data.
```

### Step 4: Validation Service Foundation

**Context**: Build the core validation engine that will filter and validate incoming telemetry data.

**Deliverable**: Validation service with configurable rules and comprehensive error handling.

```
Implement the core validation service for telemetry data:

1. Create `ValidationService.kt` in `src/main/kotlin/com/telemetry/service/`:
   - Inject TelemetryConfig for validation rules
   - Implement Prometheus metric whitelist validation
   - Implement OTEL batch size validation with truncation
   - Implement events array size validation with truncation
   - Return validation results with detailed feedback

2. Create validation result classes:
   - `ValidationResult.kt` - Contains validation outcome and statistics
   - `ValidationError.kt` - Detailed error information
   - Immutable data classes with clear success/failure states

3. Implement validation logic:
   - Prometheus: filter based on configurable whitelist
   - OTEL: check size and truncate if necessary, preserving most recent data
   - Events: check array size and truncate if necessary
   - NOTE: API key validation will be handled by Spring Security in Step 5

4. Add structured logging for validation events:
   - Log dropped metrics with metric names and device IDs
   - Log truncation events with before/after sizes
   - Use consistent log format for monitoring

5. Write comprehensive unit tests covering:
   - Valid data passes through unchanged
   - Invalid Prometheus metrics are filtered
   - Oversized OTEL data is truncated correctly
   - Oversized events arrays are truncated correctly
   - Multiple validation failures in single request
   - Edge cases (empty data, null values)

6. Create mock configurations for testing various validation scenarios

Ensure validation is fast, thread-safe, and provides clear feedback for monitoring and debugging.
```

### Step 5: HTTP Request Handler Foundation (REVISED)

**Context**: Create HTTP endpoint with proper Spring Boot Security and Hibernate Validator integration.

**Deliverable**: REST controller with Spring Security authentication and proper validation.

```
Create the HTTP-based telemetry endpoint using Spring Boot best practices:

1. Add Spring Security dependency and configure API key authentication:
   - Add spring-boot-starter-security to build.gradle.kts
   - Create SecurityConfig for API key authentication filter
   - Implement custom authentication provider for API key validation
   - Configure security to protect /api/v1/telemetry endpoint

2. Enhance data models with Hibernate Validator annotations:
   - Add @NotBlank, @NotNull, @Size, @Valid to TelemetryBatchRequest
   - Add proper validation annotations to OtelMetric, Event, PrometheusMetric
   - Remove API key validation from ValidationService (handle via security)
   - Update ValidationService to focus on business logic only

3. Create `TelemetryController.kt` with proper Spring integration:
   - Use @RestController with @Valid for automatic field validation
   - Remove manual API key validation (handled by security filter)
   - Handle POST requests to `/api/v1/telemetry` with authentication
   - Use Hibernate Validator for automatic field validation

4. Implement request processing with proper separation of concerns:
   - Authentication handled by Spring Security filter
   - Field validation handled by Hibernate Validator
   - Business logic validation handled by ValidationService
   - Return structured response with validation statistics

5. Add proper error handling:
   - @ExceptionHandler for validation failures (MethodArgumentNotValidException)
   - @ExceptionHandler for JSON parsing errors
   - Security exceptions handled by Spring Security
   - Return consistent error response format

6. Write comprehensive integration tests with security:
   - Test with valid API keys (authenticated requests)
   - Test with invalid/missing API keys (401 errors)
   - Test field validation failures (400 errors)
   - Test business logic validation warnings
   - Integration tests with @WithMockUser or custom security

This provides proper separation of concerns with Spring Security handling authentication,
Hibernate Validator handling field validation, and ValidationService handling business logic.

**IMPORTANT**: After completing each step, update the todo.md checklist to mark tasks as completed
and add any new git commit references for progress tracking.
```

### Step 6: Metrics Collection Infrastructure

**Context**: Implement internal metrics collection using Micrometer to track service performance and validation statistics.

**Deliverable**: Comprehensive metrics collection system integrated with validation and request handling.

```
Implement internal metrics collection for service monitoring:

1. Create `MetricsService.kt` in `src/main/kotlin/com/telemetry/service/`:
   - Inject MeterRegistry for metrics collection
   - Create counters for requests, validation failures, processing times
   - Create gauges for active connections and memory usage
   - Create timers for request processing duration

2. Define metric collections:
   - `telemetry_requests_total{device_id, status}` - Request counters
   - `telemetry_validation_failures_total{type, reason}` - Validation failures
   - `telemetry_prometheus_metrics_dropped_total{device_id}` - Dropped metrics
   - `telemetry_otel_truncations_total{device_id}` - OTEL truncations
   - `telemetry_events_truncations_total{device_id}` - Events truncations
   - `telemetry_request_duration_seconds{device_id}` - Processing times

3. Integrate metrics with existing services:
   - Update ValidationService to record validation metrics
   - Update TelemetryController to record request metrics
   - Add timing measurements for critical operations
   - Record device-specific metrics using device IDs as tags

4. Configure Prometheus endpoint:
   - Enable `/actuator/prometheus` endpoint
   - Ensure proper metric formatting and naming conventions
   - Add custom metric descriptions and help text

5. Write unit tests for metrics collection:
   - Verify counters increment correctly
   - Verify timers record durations
   - Verify tags and labels are applied correctly
   - Test metric collection with various validation scenarios

6. Create integration test that verifies Prometheus endpoint exposure

Ensure metrics are efficient, don't impact performance, and provide actionable monitoring data.
```

### Step 7: Health Checks and Basic Observability

**Context**: Implement comprehensive health checks and basic observability features.

**Deliverable**: Health check endpoints with custom health indicators and structured logging foundation.

```
Implement comprehensive health checks and observability features:

1. Create custom health indicators in `src/main/kotlin/com/telemetry/health/`:
   - `ConfigurationHealthIndicator.kt` - Validates configuration on startup
   - `MemoryHealthIndicator.kt` - Monitors memory usage
   - Implement HealthIndicator interface with proper UP/DOWN logic

2. Configure actuator endpoints in application.yml:
   - Enable `/actuator/health`, `/actuator/health/liveness`, `/actuator/health/readiness`
   - Configure health check details and security
   - Set up custom health check timeouts

3. Implement structured logging foundation:
   - Configure Logback with JSON formatting
   - Create logging utility class for consistent log formatting
   - Define standard log fields (timestamp, level, logger, message, context)
   - Add correlation IDs for request tracking

4. Add logging to existing services:
   - Update ValidationService with structured validation logs
   - Update TelemetryController with request/response logs
   - Include device IDs and processing statistics in logs

5. Write tests for health indicators:
   - Test UP/DOWN conditions for each indicator
   - Test health check response format
   - Integration tests for actuator endpoints

6. Create test logging configuration for testing

Health checks should provide clear operational status and logging should be searchable and actionable for production troubleshooting.
```

### Step 8: HTTP Client for Grafana Integration

**Context**: Implement HTTP client to forward validated telemetry data to Grafana collector endpoint.

**Deliverable**: Robust HTTP client with retry logic, timeout handling, and proper error management.

```
Create HTTP client service for Grafana collector integration:

1. Create `GrafanaForwardingService.kt` in `src/main/kotlin/com/telemetry/service/`:
   - Use Spring's RestTemplate or WebClient for HTTP communication
   - Inject GrafanaConfig for endpoint configuration
   - Implement async forwarding to avoid blocking request processing
   - Add proper timeout and connection pool configuration

2. Implement data transformation:
   - Convert validated TelemetryBatchRequest to Grafana-compatible format
   - Handle different data types (OTEL, events, Prometheus metrics)
   - Preserve device metadata and timestamps
   - Create proper JSON payload for Grafana ingestion

3. Add error handling and resilience:
   - Implement configurable retry logic with exponential backoff
   - Handle network timeouts and connection failures gracefully
   - Log forwarding failures with detailed error information
   - Circuit breaker pattern for repeated failures (optional but recommended)

4. Integrate metrics collection:
   - Record forwarding attempt counters (success/failure)
   - Measure forwarding request duration
   - Track error types and frequencies
   - Add Grafana endpoint health metrics

5. Write comprehensive unit tests:
   - Mock Grafana endpoint responses (success/failure scenarios)
   - Test retry logic with various failure patterns
   - Test timeout handling and error propagation
   - Test data transformation accuracy

6. Create integration tests with mock HTTP server:
   - Test complete forwarding flow
   - Test network failure scenarios
   - Verify request format sent to Grafana

The service should be resilient to Grafana outages and provide clear visibility into forwarding health.
```

### Step 9: Integration of HTTP Flow

**Context**: Wire together all HTTP-based components into a complete telemetry processing pipeline.

**Deliverable**: Fully integrated HTTP telemetry service with end-to-end testing.

```
Integrate all components into complete HTTP telemetry processing flow:

1. Update `TelemetryController.kt` to use complete pipeline:
   - Inject ValidationService, MetricsService, and GrafanaForwardingService
   - Implement full request processing: validate → record metrics → forward → respond
   - Add async processing for Grafana forwarding to avoid blocking responses
   - Include comprehensive error handling for each pipeline stage

2. Create `TelemetryProcessingService.kt` as orchestration layer:
   - Coordinate between validation, metrics, and forwarding services
   - Implement transaction-like semantics (log everything even if forwarding fails)
   - Handle partial success scenarios (validation works, forwarding fails)
   - Provide clear processing status in responses

3. Enhance error handling:
   - Distinguish between client errors (bad data) and server errors (system issues)
   - Implement proper HTTP status codes for different failure types
   - Add detailed error logging with correlation IDs
   - Ensure no data loss during processing failures

4. Write comprehensive integration tests:
   - Test complete happy path: valid request → validation → forwarding → response
   - Test partial failure scenarios (forwarding fails, validation succeeds)
   - Test complete failure scenarios (validation fails)
   - Load testing with concurrent requests
   - Test with realistic ESP32 data payloads

5. Add monitoring and debugging features:
   - Request correlation IDs for tracing
   - Processing pipeline metrics (stage durations, success rates)
   - Debug endpoints for troubleshooting (dev profile only)

6. Create performance benchmarks and establish baseline metrics

This completes the HTTP-based foundation that will be extended with UDP support in the next phase.
```

### Step 10: UDP Protocol Foundation

**Context**: Add UDP protocol support to the existing HTTP infrastructure, reusing validation and processing logic.

**Deliverable**: UDP server that can receive and process telemetry data using existing pipeline.

```
Implement UDP protocol support for telemetry data collection:

1. Create UDP server infrastructure in `src/main/kotlin/com/telemetry/udp/`:
   - `UdpServerConfig.kt` - UDP-specific configuration (port, buffer sizes)
   - `UdpTelemetryReceiver.kt` - UDP packet reception and parsing
   - Use Spring's @EventListener or custom UDP socket management
   - Configure proper buffer sizes for ESP32 packet sizes

2. Implement UDP packet handling:
   - Receive UDP packets and parse JSON payloads
   - Handle packet size limitations and fragmentation concerns
   - Convert UDP payload to TelemetryBatchRequest objects
   - Add UDP-specific error handling (malformed packets, oversized data)

3. Integrate with existing processing pipeline:
   - Reuse TelemetryProcessingService for validation and forwarding
   - Adapt error handling for UDP (no responses sent back to client)
   - Ensure async processing doesn't block UDP receiver
   - Maintain all existing metrics and logging

4. Add UDP-specific metrics:
   - UDP packets received counter
   - UDP packet parsing failures
   - UDP buffer overflow events
   - Network-related error tracking

5. Configure both HTTP and UDP endpoints:
   - Update application.yml to support both protocols
   - Ensure both endpoints use same validation and processing logic
   - Add protocol identifier to logs and metrics for debugging

6. Write comprehensive tests:
   - Unit tests for UDP packet parsing
   - Integration tests sending actual UDP packets
   - Test UDP and HTTP endpoints concurrently
   - Test UDP-specific error scenarios (malformed packets, network issues)

UDP implementation should be robust against network issues and maintain all existing functionality while adding protocol flexibility.
```

### Step 11: UDP Error Handling and Resilience

**Context**: Enhance UDP implementation with robust error handling, packet loss management, and performance optimization.

**Deliverable**: Production-ready UDP service with comprehensive error handling and monitoring.

```
Enhance UDP implementation with production-ready error handling and resilience:

1. Implement advanced UDP error handling in `UdpTelemetryReceiver.kt`:
   - Handle network interface failures and recovery
   - Manage UDP buffer overflow conditions
   - Deal with packet corruption and malformed data gracefully
   - Implement packet size validation before processing

2. Add connection management and resource handling:
   - Proper UDP socket lifecycle management
   - Connection pool management for high throughput
   - Memory management for packet buffers
   - Graceful shutdown procedures

3. Implement monitoring and diagnostics:
   - Packet loss detection and reporting
   - Network interface health monitoring
   - UDP-specific performance metrics (packets/second, processing latency)
   - Buffer utilization metrics

4. Add UDP-specific logging:
   - Log packet reception events with source information
   - Log parsing failures with packet details (truncated for security)
   - Log network errors with detailed error context
   - Implement rate-limited logging to prevent log flooding

5. Optimize for ESP32 constraints:
   - Handle ESP32-typical packet sizes efficiently
   - Support ESP32 retry patterns and timing
   - Implement backpressure handling for high-frequency ESP32 data

6. Write stress and reliability tests:
   - Test with high packet rates to identify bottlenecks
   - Test network failure recovery scenarios
   - Test with various packet sizes and corruption patterns
   - Load test with multiple simulated ESP32 devices

7. Create monitoring dashboard configuration examples for common scenarios

This ensures the UDP service can handle real-world ESP32 deployment scenarios reliably.
```

### Step 12: Protocol Integration and Optimization

**Context**: Optimize the dual HTTP/UDP service for production performance and create unified monitoring.

**Deliverable**: Optimized telemetry service supporting both protocols with unified observability.

```
Optimize and unify the dual-protocol telemetry service:

1. Create unified protocol abstraction in `src/main/kotlin/com/telemetry/protocol/`:
   - `TelemetryProtocolHandler.kt` interface for protocol-agnostic processing
   - `HttpProtocolHandler.kt` and `UdpProtocolHandler.kt` implementations
   - Shared request processing pipeline regardless of transport protocol
   - Protocol-specific error handling while maintaining common processing logic

2. Optimize performance for both protocols:
   - Implement connection pooling and resource sharing
   - Add request batching for improved throughput
   - Optimize JSON parsing and object allocation
   - Implement async processing pipelines for both HTTP and UDP

3. Create unified monitoring and metrics:
   - Protocol-agnostic metrics with protocol tags
   - Comparative performance metrics (HTTP vs UDP throughput)
   - Unified health checks covering both protocols
   - Cross-protocol correlation for debugging

4. Implement advanced configuration:
   - Protocol-specific tuning parameters
   - Dynamic configuration updates without restart
   - Environment-specific optimizations (dev vs production)
   - Resource allocation controls

5. Add comprehensive integration testing:
   - Test both protocols simultaneously under load
   - Test protocol switching and failover scenarios
   - Performance comparison tests between protocols
   - Real-world simulation with multiple ESP32 devices

6. Create operational runbooks:
   - Troubleshooting guides for each protocol
   - Performance tuning recommendations
   - Monitoring alert configurations
   - Capacity planning guidelines

This creates a production-ready service that efficiently handles both HTTP and UDP telemetry collection.
```

### Step 13: Advanced Validation Engine

**Context**: Enhance the validation engine with advanced rules, custom validators, and dynamic configuration support.

**Deliverable**: Flexible validation system supporting complex rules and runtime configuration updates.

```
Enhance the validation engine with advanced capabilities:

1. Create advanced validation framework in `src/main/kotlin/com/telemetry/validation/`:
   - `ValidatorRegistry.kt` - Manages different validator types
   - `PrometheusValidator.kt` - Advanced Prometheus metric validation
   - `OtelValidator.kt` - Enhanced OTEL data validation
   - `EventValidator.kt` - Sophisticated event validation
   - `CustomValidator.kt` interface for extensible validation rules

2. Implement advanced Prometheus validation:
   - Metric name pattern validation (regex support)
   - Label validation and sanitization
   - Value range validation by metric type
   - Timestamp validation and drift detection
   - Rate limiting per device and metric type

3. Enhance OTEL validation:
   - Schema validation against OTEL standards
   - Resource attribute validation
   - Metric type validation (gauge, counter, histogram)
   - Unit validation and normalization
   - Cardinality limits to prevent explosion

4. Improve event validation:
   - Event schema validation with configurable schemas
   - Severity level validation and normalization
   - Message content sanitization for security
   - Event rate limiting per device
   - Structured metadata validation

5. Add dynamic configuration support:
   - Runtime validation rule updates
   - A/B testing capabilities for validation rules
   - Device-specific validation profiles
   - Validation rule versioning and rollback

6. Write comprehensive validation tests:
   - Test all validation scenarios with edge cases
   - Performance tests for validation overhead
   - Test dynamic configuration updates
   - Test validation rule combinations and conflicts

This creates a flexible, extensible validation system that can adapt to changing requirements without code changes.
```

### Step 14: Data Transformation and Enrichment

**Context**: Add data transformation capabilities to enrich telemetry data and optimize it for Grafana ingestion.

**Deliverable**: Data transformation pipeline with enrichment, normalization, and optimization features.

```
Implement data transformation and enrichment pipeline:

1. Create transformation services in `src/main/kotlin/com/telemetry/transform/`:
   - `DataTransformationService.kt` - Main transformation orchestrator
   - `MetricEnrichmentService.kt` - Add derived metrics and metadata
   - `DataNormalizationService.kt` - Standardize data formats
   - `GrafanaOptimizationService.kt` - Optimize for Grafana ingestion

2. Implement metric enrichment:
   - Add device metadata to all metrics (location, firmware version, etc.)
   - Calculate derived metrics (rates, moving averages, health scores)
   - Add timestamp normalization and timezone handling
   - Include data quality indicators

3. Add data normalization:
   - Standardize metric naming conventions
   - Normalize units and value formats
   - Standardize label formats and hierarchies
   - Clean and validate string data

4. Optimize for Grafana:
   - Convert to Grafana's preferred data formats
   - Optimize metric cardinality for performance
   - Implement data compression for large payloads
   - Add Grafana-specific metadata and annotations

5. Create transformation configuration:
   - Configurable transformation rules per device type
   - Custom enrichment templates
   - Conditional transformations based on data content
   - Performance tuning parameters

6. Write comprehensive tests:
   - Test data enrichment accuracy
   - Test normalization consistency
   - Performance tests for transformation overhead
   - Test Grafana compatibility with transformed data

This ensures data sent to Grafana is optimized, consistent, and provides maximum value for monitoring and analysis.
```

### Step 15: Batch Processing and Performance Optimization

**Context**: Implement efficient batch processing and optimize the service for high-throughput scenarios.

**Deliverable**: High-performance telemetry service with batch processing and resource optimization.

```
Implement batch processing and performance optimizations:

1. Create batch processing infrastructure in `src/main/kotlin/com/telemetry/batch/`:
   - `BatchProcessor.kt` - Manages batching logic and timing
   - `BatchAccumulator.kt` - Collects data into optimally-sized batches
   - `BatchScheduler.kt` - Handles timing and triggering of batch processing
   - Configurable batch sizes and timing intervals

2. Implement intelligent batching:
   - Device-aware batching (batch per device or across devices)
   - Time-based batching with maximum age limits
   - Size-based batching with memory management
   - Priority-based batching for critical vs. normal data

3. Add performance optimizations:
   - Object pooling for frequently used objects
   - Zero-copy operations where possible
   - Efficient JSON parsing with streaming
   - Memory allocation optimization
   - CPU-efficient data structures

4. Implement resource management:
   - Memory pressure detection and adaptation
   - CPU utilization monitoring and throttling
   - Network bandwidth management
   - Disk I/O optimization for logging

5. Add performance monitoring:
   - Throughput metrics (requests/second, data volume/second)
   - Latency percentiles (p50, p95, p99)
   - Resource utilization metrics
   - Batch processing efficiency metrics

6. Create performance testing suite:
   - Load testing with realistic ESP32 data patterns
   - Stress testing to find breaking points
   - Endurance testing for memory leaks
   - Performance regression testing

7. Add auto-tuning capabilities:
   - Automatic batch size optimization based on load
   - Dynamic resource allocation
   - Adaptive timeout adjustments

This ensures the service can handle production workloads efficiently while maintaining low latency and resource usage.
```

### Step 16: Advanced Grafana Integration

**Context**: Enhance Grafana integration with advanced features like authentication, data routing, and error recovery.

**Deliverable**: Robust Grafana integration with enterprise-grade features and reliability.

```
Enhance Grafana integration with advanced capabilities:

1. Implement advanced Grafana client in `src/main/kotlin/com/telemetry/grafana/`:
   - `GrafanaClientManager.kt` - Connection and authentication management
   - `GrafanaDataRouter.kt` - Route different data types to appropriate Grafana endpoints
   - `GrafanaAuthService.kt` - Handle various authentication methods
   - Support multiple Grafana instances for high availability

2. Add authentication support:
   - API key authentication with rotation support
   - Basic authentication for legacy setups
   - Token-based authentication with refresh capabilities
   - Certificate-based authentication for secure environments

3. Implement intelligent data routing:
   - Route OTEL data to OTEL collector endpoint
   - Route Prometheus metrics to Prometheus endpoint
   - Route events to Loki or event ingestion endpoint
   - Dynamic routing based on data content and configuration

4. Add advanced error recovery:
   - Circuit breaker with intelligent recovery
   - Failover to secondary Grafana instances
   - Data persistence during outages with replay capability
   - Dead letter queue for failed forwards

5. Implement data buffering and queuing:
   - In-memory buffering with overflow to disk
   - Priority queuing for critical vs. normal data
   - Compression for stored data during outages
   - Automatic retry with exponential backoff

6. Add comprehensive monitoring:
   - Grafana endpoint health monitoring
   - Data delivery success rates
   - Buffer utilization and overflow metrics
   - Authentication failure tracking

7. Write integration tests:
   - Test against real Grafana instances
   - Test failover and recovery scenarios
   - Test various authentication methods
   - Performance testing with high data volumes

This creates enterprise-grade Grafana integration suitable for production environments with high availability requirements.
```

### Step 17: Security Hardening

**Context**: Implement comprehensive security measures including authentication, authorization, input validation, and audit logging.

**Deliverable**: Security-hardened service with comprehensive protection against common threats.

```
Implement comprehensive security hardening:

1. Enhance authentication and authorization in `src/main/kotlin/com/telemetry/security/`:
   - `ApiKeyAuthenticationService.kt` - Secure API key management
   - `AuthorizationService.kt` - Role-based access control
   - `SecurityAuditService.kt` - Security event logging
   - Rate limiting per API key and device

2. Implement advanced API key management:
   - API key rotation with grace periods
   - Device-specific API keys with permissions
   - API key expiration and renewal
   - Secure key storage and retrieval

3. Add input security validation:
   - Input sanitization to prevent injection attacks
   - Size limits to prevent DoS attacks
   - Content validation to prevent malicious data
   - Encoding validation and normalization

4. Implement network security:
   - IP whitelisting for restricted environments
   - TLS termination and certificate management
   - Request signature validation
   - DDoS protection with rate limiting

5. Add audit logging and monitoring:
   - Security event logging (authentication failures, suspicious activity)
   - Audit trail for configuration changes
   - Real-time security alerting
   - Integration with SIEM systems

6. Implement data protection:
   - Sensitive data redaction in logs
   - Data encryption at rest (if local storage is added)
   - Secure data transmission to Grafana
   - PII detection and handling

7. Create security testing:
   - Penetration testing scenarios
   - Authentication bypass testing
   - Input validation testing
   - Rate limiting effectiveness testing

8. Add security configuration:
   - Security policy configuration
   - Compliance reporting capabilities
   - Security metric collection

This ensures the service meets enterprise security requirements and protects against common attack vectors.
```

### Step 18: Production Deployment and Operations

**Context**: Prepare the service for production deployment with containerization, configuration management, and operational tools.

**Deliverable**: Production-ready deployment artifacts with comprehensive operational support.

```
Prepare service for production deployment:

1. Create production-ready containerization:
   - Multi-stage Dockerfile with optimized layers
   - Security scanning and vulnerability management
   - Minimal base image with only required components
   - Non-root user execution for security
   - Health check integration for container orchestration

2. Implement configuration management:
   - Environment-specific configuration profiles
   - Secret management integration (Kubernetes secrets, Vault)
   - Configuration validation on startup
   - Dynamic configuration updates without restart

3. Add operational tooling in `src/main/kotlin/com/telemetry/ops/`:
   - `DiagnosticsService.kt` - System diagnostics and troubleshooting
   - `MaintenanceService.kt` - Maintenance operations and health checks
   - `BackupService.kt` - Configuration and state backup (if needed)
   - Administrative endpoints for operations

4. Create deployment artifacts:
   - Kubernetes manifests (Deployment, Service, ConfigMap, Secret)
   - Docker Compose for local development
   - Helm charts for parameterized deployments
   - Terraform modules for infrastructure provisioning

5. Implement monitoring and alerting:
   - Production-ready metrics collection
   - Alert rules for critical conditions
   - Dashboard configurations for Grafana
   - Log aggregation configuration

6. Add operational procedures:
   - Startup and shutdown procedures
   - Rolling update procedures
   - Disaster recovery procedures
   - Performance tuning guidelines

7. Create comprehensive documentation:
   - Deployment guide
   - Operations runbook
   - Troubleshooting guide
   - Performance tuning guide

8. Write production tests:
   - End-to-end deployment testing
   - Production load simulation
   - Disaster recovery testing
   - Security validation in production environment

This provides everything needed for reliable production deployment and operations.
```

### Step 19: Comprehensive Testing and Quality Assurance

**Context**: Implement comprehensive testing strategy covering unit, integration, performance, and security testing.

**Deliverable**: Complete test suite ensuring service reliability and performance under all conditions.

```
Implement comprehensive testing and quality assurance:

1. Expand unit testing coverage:
   - Achieve >90% code coverage with meaningful tests
   - Test all error conditions and edge cases
   - Mock external dependencies comprehensively
   - Test concurrent access and thread safety
   - Performance unit tests for critical paths

2. Create integration testing suite in `src/test/kotlin/com/telemetry/integration/`:
   - `FullPipelineIntegrationTest.kt` - End-to-end testing
   - `ProtocolIntegrationTest.kt` - HTTP and UDP protocol testing
   - `GrafanaIntegrationTest.kt` - External system integration
   - `ConfigurationIntegrationTest.kt` - Configuration and environment testing

3. Implement performance testing:
   - Load testing with realistic ESP32 data patterns
   - Stress testing to identify breaking points
   - Endurance testing for memory leaks and stability
   - Network latency and packet loss simulation
   - Concurrent user simulation

4. Add security testing:
   - Authentication and authorization testing
   - Input validation and injection testing
   - Rate limiting and DoS protection testing
   - Network security testing
   - Dependency vulnerability scanning

5. Create test data management:
   - Realistic ESP32 data generators
   - Test data versioning and management
   - Performance baseline data
   - Security test scenarios

6. Implement continuous testing:
   - Pre-commit hooks for test execution
   - Automated test execution in CI/CD
   - Performance regression detection
   - Security vulnerability scanning

7. Add testing utilities:
   - Mock ESP32 device simulators
   - Test environment setup and teardown
   - Test data assertion libraries
   - Performance measurement utilities

8. Create quality gates:
   - Code coverage requirements
   - Performance benchmarks
   - Security scan requirements
   - Integration test requirements

This ensures comprehensive quality assurance and reliable service behavior under all conditions.
```

### Step 20: Documentation and Knowledge Transfer

**Context**: Create comprehensive documentation for development, deployment, and operations.

**Deliverable**: Complete documentation suite enabling effective knowledge transfer and maintenance.

```
Create comprehensive documentation and knowledge transfer materials:

1. Create technical documentation in `docs/`:
   - `architecture.md` - System architecture and design decisions
   - `api-reference.md` - Complete API documentation with examples
   - `configuration-reference.md` - All configuration options and examples
   - `performance-guide.md` - Performance tuning and optimization
   - `security-guide.md` - Security configuration and best practices

2. Create operational documentation:
   - `deployment-guide.md` - Step-by-step deployment instructions
   - `operations-runbook.md` - Day-to-day operational procedures
   - `troubleshooting-guide.md` - Common issues and solutions
   - `monitoring-guide.md` - Monitoring setup and alert configuration
   - `disaster-recovery.md` - Backup and recovery procedures

3. Create developer documentation:
   - `development-setup.md` - Local development environment setup
   - `contributing-guide.md` - Code contribution guidelines
   - `testing-guide.md` - Testing strategy and execution
   - `code-style-guide.md` - Coding standards and conventions

4. Add inline documentation:
   - Comprehensive KDoc for all public APIs
   - Code comments explaining complex logic
   - Configuration property documentation
   - Example usage in code comments

5. Create training materials:
   - Architecture overview presentation
   - Hands-on workshop materials
   - Video tutorials for key operations
   - FAQ document for common questions

6. Implement documentation automation:
   - Automated API documentation generation
   - Configuration reference auto-generation
   - Code example validation
   - Documentation version control

7. Create knowledge transfer artifacts:
   - System overview diagrams
   - Data flow diagrams
   - Sequence diagrams for key operations
   - Decision matrices for troubleshooting

This ensures effective knowledge transfer and enables teams to maintain and extend the service effectively.
```

### Step 21: Final Integration and Production Readiness

**Context**: Complete final integration, perform comprehensive validation, and prepare for production release.

**Deliverable**: Production-ready ESP32 Telemetry Service with all features integrated and validated.

```
Complete final integration and production readiness validation:

1. Perform final system integration:
   - Integrate all components into cohesive system
   - Validate end-to-end data flow from ESP32 to Grafana
   - Test all configuration combinations
   - Validate all error handling paths
   - Test graceful startup and shutdown procedures

2. Conduct comprehensive system validation:
   - Full-scale load testing with simulated ESP32 fleet
   - Network failure simulation and recovery testing
   - Configuration change testing under load
   - Security validation in production-like environment
   - Performance validation against requirements

3. Create production deployment validation:
   - Deploy to staging environment identical to production
   - Validate monitoring and alerting configuration
   - Test backup and recovery procedures
   - Validate security configurations
   - Test operational procedures

4. Perform final performance optimization:
   - Profile application under production load
   - Optimize resource usage and allocation
   - Tune JVM settings for production workload
   - Optimize container resource limits
   - Validate auto-scaling behavior

5. Complete production readiness checklist:
   - Security review and penetration testing
   - Performance acceptance testing
   - Operational readiness review
   - Documentation completeness review
   - Disaster recovery testing

6. Create production release artifacts:
   - Final Docker image with version tagging
   - Production deployment manifests
   - Configuration templates for different environments
   - Monitoring dashboard exports
   - Alert rule configurations

7. Conduct final validation:
   - End-to-end testing with real ESP32 devices
   - Integration testing with real Grafana instance
   - Load testing at expected production scale
   - Failover and recovery testing
   - User acceptance testing

This completes the development process with a fully integrated, tested, and production-ready ESP32 Telemetry Service.
```

---

## Summary

This implementation plan provides 21 carefully structured steps that build incrementally from basic project setup to production-ready deployment. Each step is designed to be:

- **Small enough** to be implemented safely with minimal risk
- **Large enough** to make meaningful progress
- **Testable** with comprehensive validation at each stage
- **Integrated** with no orphaned code or hanging dependencies

The plan emphasizes:
- Test-driven development throughout
- Incremental complexity increases
- Continuous integration of components
- Production readiness from the start
- Comprehensive documentation and knowledge transfer

Each prompt builds on previous work and maintains a working system at every stage, enabling safe iteration and easy rollback if needed.