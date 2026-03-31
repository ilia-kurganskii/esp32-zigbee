package com.telemetry.service

import com.telemetry.model.ValidationErrorType
import io.micrometer.core.instrument.MeterRegistry
import io.micrometer.core.instrument.simple.SimpleMeterRegistry
import org.junit.jupiter.api.BeforeEach
import org.junit.jupiter.api.Test
import org.junit.jupiter.api.Assertions.*
import java.time.Duration

class MetricsServiceTest {

    private lateinit var meterRegistry: MeterRegistry
    private lateinit var metricsService: MetricsService

    @BeforeEach
    fun setUp() {
        meterRegistry = SimpleMeterRegistry()
        metricsService = MetricsService(meterRegistry)
    }

    // Note: Request timing and counting is now handled by Spring Boot's built-in HTTP metrics

    @Test
    fun `recordValidationFailure increments validation failure counter with correct tags`() {
        // Given
        val deviceId = "test-device-001"
        val errorType = ValidationErrorType.PROMETHEUS_METRIC_NOT_WHITELISTED
        val reason = "Metric not in whitelist"

        // When
        metricsService.recordValidationFailure(deviceId, errorType, reason)

        // Then
        val counter = meterRegistry.get("telemetry_validation_failures_total")
            .tag("device_id", deviceId)
            .tag("type", errorType.name)
            .tag("reason", reason)
            .counter()
        
        assertEquals(1.0, counter.count())
    }

    @Test
    fun `recordPrometheusMetricsDropped increments dropped metrics counter`() {
        // Given
        val deviceId = "test-device-001"
        val count = 5

        // When
        metricsService.recordPrometheusMetricsDropped(deviceId, count)

        // Then
        val counter = meterRegistry.get("telemetry_prometheus_metrics_dropped_total")
            .tag("device_id", deviceId)
            .tag("count", count.toString())
            .counter()
        
        assertEquals(1.0, counter.count())
    }

    @Test
    fun `recordOtelTruncation increments OTEL truncation counter with size info`() {
        // Given
        val deviceId = "test-device-001"
        val originalSize = 100
        val truncatedSize = 50

        // When
        metricsService.recordOtelTruncation(deviceId, originalSize, truncatedSize)

        // Then
        val counter = meterRegistry.get("telemetry_otel_truncations_total")
            .tag("device_id", deviceId)
            .tag("original_size", originalSize.toString())
            .tag("truncated_size", truncatedSize.toString())
            .counter()
        
        assertEquals(1.0, counter.count())
    }

    @Test
    fun `recordEventsTruncation increments events truncation counter with size info`() {
        // Given
        val deviceId = "test-device-001"
        val originalSize = 20
        val truncatedSize = 10

        // When
        metricsService.recordEventsTruncation(deviceId, originalSize, truncatedSize)

        // Then
        val counter = meterRegistry.get("telemetry_events_truncations_total")
            .tag("device_id", deviceId)
            .tag("original_size", originalSize.toString())
            .tag("truncated_size", truncatedSize.toString())
            .counter()
        
        assertEquals(1.0, counter.count())
    }

    @Test
    fun `multiple validation failures accumulate metrics correctly`() {
        // Given
        val deviceId = "test-device-001"
        val errorType = ValidationErrorType.PROMETHEUS_METRIC_NOT_WHITELISTED

        // When
        metricsService.recordValidationFailure(deviceId, errorType, "first error")
        metricsService.recordValidationFailure(deviceId, errorType, "second error")

        // Then
        val firstErrorCounter = meterRegistry.get("telemetry_validation_failures_total")
            .tag("device_id", deviceId)
            .tag("type", errorType.name)
            .tag("reason", "first error")
            .counter()
        
        val secondErrorCounter = meterRegistry.get("telemetry_validation_failures_total")
            .tag("device_id", deviceId)
            .tag("type", errorType.name)
            .tag("reason", "second error")
            .counter()
        
        assertEquals(1.0, firstErrorCounter.count())
        assertEquals(1.0, secondErrorCounter.count())
    }

    @Test
    fun `different devices have separate validation failure metrics`() {
        // Given
        val device1 = "test-device-001"
        val device2 = "test-device-002"
        val errorType = ValidationErrorType.OTEL_BATCH_SIZE_EXCEEDED

        // When
        metricsService.recordValidationFailure(device1, errorType, "error message")
        metricsService.recordValidationFailure(device2, errorType, "error message")

        // Then
        val device1Counter = meterRegistry.get("telemetry_validation_failures_total")
            .tag("device_id", device1)
            .tag("type", errorType.name)
            .tag("reason", "error message")
            .counter()
        
        val device2Counter = meterRegistry.get("telemetry_validation_failures_total")
            .tag("device_id", device2)
            .tag("type", errorType.name)
            .tag("reason", "error message")
            .counter()
        
        assertEquals(1.0, device1Counter.count())
        assertEquals(1.0, device2Counter.count())
    }

    @Test
    fun `getMeterRegistry returns the injected registry`() {
        // When
        val registry = metricsService.getMeterRegistry()

        // Then
        assertSame(meterRegistry, registry)
    }
}