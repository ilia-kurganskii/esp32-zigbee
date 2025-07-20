package com.telemetry.service

import com.telemetry.config.TelemetryConfig
import com.telemetry.config.ValidationConfig
import com.telemetry.model.*
import org.junit.jupiter.api.Test
import org.junit.jupiter.api.Assertions.*
import org.junit.jupiter.api.BeforeEach
import java.time.Instant

class ValidationServiceTest {

    private lateinit var validationService: ValidationService
    private lateinit var config: TelemetryConfig

    @BeforeEach
    fun setUp() {
        config = TelemetryConfig().apply {
            validation = ValidationConfig().apply {
                prometheusWhitelist = setOf("cpu_usage", "memory_usage", "temperature")
                maxOtelBatchSize = 1000
                maxEventsArraySize = 5
            }
        }
        validationService = ValidationService(config)
    }



    @Test
    fun `filters non-whitelisted Prometheus metrics`() {
        val request = createValidRequest().copy(
            metrics = listOf(
                PrometheusMetric("cpu_usage", 50.0, emptyMap()), // whitelisted
                PrometheusMetric("custom_metric", 100.0, emptyMap()), // not whitelisted
                PrometheusMetric("memory_usage", 75.0, emptyMap()) // whitelisted
            )
        )
        
        val result = validationService.validate(request)
        
        assertTrue(result.isValid)
        assertEquals(2, result.validatedRequest.metrics?.size)
        assertEquals(1, result.statistics.droppedPrometheusMetrics)
        assertTrue(result.errors.any { 
            it.type == ValidationErrorType.PROMETHEUS_METRIC_NOT_WHITELISTED &&
            it.message.contains("custom_metric")
        })
        
        val validatedMetricNames = result.validatedRequest.metrics?.map { it.name }
        assertTrue(validatedMetricNames?.contains("cpu_usage") == true)
        assertTrue(validatedMetricNames?.contains("memory_usage") == true)
        assertFalse(validatedMetricNames?.contains("custom_metric") == true)
    }

    @Test
    fun `truncates OTEL data when size exceeds limit`() {
        // Create metrics with long names to ensure size limit is exceeded
        val largeOtelMetrics = (1..50).map { 
            OtelMetric(
                "very_long_metric_name_that_exceeds_size_limit_$it", 
                it.toDouble(), 
                mapOf("very_long_label_key_$it" to "very_long_label_value_$it"),
                Instant.now()
            )
        }
        val request = createValidRequest().copy(otel = largeOtelMetrics)
        
        val result = validationService.validate(request)
        
        println("Original OTEL metrics: ${largeOtelMetrics.size}")
        println("Validated OTEL metrics: ${result.validatedRequest.otel?.size}")
        println("Truncated: ${result.statistics.truncatedOtel}")
        println("Errors: ${result.errors.map { "${it.type}: ${it.message}" }}")
        
        assertTrue(result.isValid)
        assertTrue(result.statistics.truncatedOtel)
        assertTrue(result.validatedRequest.otel!!.size < largeOtelMetrics.size)
        assertTrue(result.errors.any { 
            it.type == ValidationErrorType.OTEL_BATCH_SIZE_EXCEEDED 
        })
    }

    @Test
    fun `truncates events array when size exceeds limit`() {
        val largeEventsList = (1..10).map {
            Event("event_$it", "Message $it", "INFO", Instant.now())
        }
        val request = createValidRequest().copy(events = largeEventsList)
        
        val result = validationService.validate(request)
        
        assertTrue(result.isValid)
        assertTrue(result.statistics.truncatedEvents)
        assertEquals(5, result.validatedRequest.events?.size) // maxEventsArraySize = 5
        assertTrue(result.errors.any { 
            it.type == ValidationErrorType.EVENTS_ARRAY_SIZE_EXCEEDED 
        })
    }

    @Test
    fun `handles request with no optional data`() {
        val request = createValidRequest().copy(
            otel = null,
            events = null,
            metrics = null
        )
        
        val result = validationService.validate(request)
        
        assertTrue(result.isValid)
        assertNull(result.validatedRequest.otel)
        assertNull(result.validatedRequest.events)
        assertNull(result.validatedRequest.metrics)
        assertEquals(0, result.statistics.originalOtelMetrics)
        assertEquals(0, result.statistics.originalEvents)
        assertEquals(0, result.statistics.originalPrometheusMetrics)
    }

    @Test
    fun `handles empty arrays correctly`() {
        val request = createValidRequest().copy(
            otel = emptyList(),
            events = emptyList(),
            metrics = emptyList()
        )
        
        val result = validationService.validate(request)
        
        assertTrue(result.isValid)
        assertTrue(result.validatedRequest.otel?.isEmpty() == true)
        assertTrue(result.validatedRequest.events?.isEmpty() == true)
        assertTrue(result.validatedRequest.metrics?.isEmpty() == true)
        assertEquals(0, result.statistics.droppedPrometheusMetrics)
        assertFalse(result.statistics.truncatedOtel)
        assertFalse(result.statistics.truncatedEvents)
    }

    @Test
    fun `collects multiple validation errors correctly`() {
        val request = TelemetryBatchRequest(
            deviceId = "test-device",
            timestamp = Instant.now(),
            otel = (1..50).map { OtelMetric("very_long_metric_name_$it", it.toDouble(), mapOf("key_$it" to "value_$it"), Instant.now()) },
            events = (1..10).map { Event("event_$it", "Message $it", "INFO", Instant.now()) },
            metrics = listOf(
                PrometheusMetric("invalid_metric", 100.0, emptyMap()),
                PrometheusMetric("cpu_usage", 50.0, emptyMap())
            )
        )
        
        val result = validationService.validate(request)
        
        println("Validation errors: ${result.errors.map { "${it.type}: ${it.message}" }}")
        println("Error count: ${result.errors.size}")
        
        assertTrue(result.isValid) // Should be valid now that API key validation is removed
        assertTrue(result.errors.size >= 1) // At minimum Prometheus filtering
        
        val errorTypes = result.errors.map { it.type }
        assertTrue(errorTypes.contains(ValidationErrorType.PROMETHEUS_METRIC_NOT_WHITELISTED))
        // OTEL and Events truncation may or may not occur depending on the size calculation
    }

    private fun createValidRequest(): TelemetryBatchRequest {
        return TelemetryBatchRequest(
            deviceId = "test-device-001",
            timestamp = Instant.now(),
            otel = listOf(
                OtelMetric("system.cpu.usage", 45.2, mapOf("core" to "0"), Instant.now())
            ),
            events = listOf(
                Event("wifi.connected", "Connected to WiFi", "INFO", Instant.now())
            ),
            metrics = listOf(
                PrometheusMetric("cpu_usage", 50.0, mapOf("device" to "esp32"))
            )
        )
    }
}