package com.telemetry.model

import com.fasterxml.jackson.databind.ObjectMapper
import com.fasterxml.jackson.databind.SerializationFeature
import com.fasterxml.jackson.datatype.jsr310.JavaTimeModule
import com.fasterxml.jackson.module.kotlin.KotlinModule
import com.fasterxml.jackson.module.kotlin.readValue
import org.junit.jupiter.api.Test
import org.junit.jupiter.api.Assertions.*
import java.time.Instant

class TelemetryDataModelsTest {

    private val objectMapper: ObjectMapper = ObjectMapper().apply {
        registerModule(KotlinModule.Builder().build())
        registerModule(JavaTimeModule())
        disable(SerializationFeature.WRITE_DATES_AS_TIMESTAMPS)
        configure(com.fasterxml.jackson.databind.DeserializationFeature.FAIL_ON_UNKNOWN_PROPERTIES, false)
    }

    @Test
    fun `TelemetryBatchRequest serializes and deserializes correctly`() {
        val timestamp = Instant.parse("2025-07-20T10:30:00Z")
        
        val otelMetrics = listOf(
            OtelMetric(
                name = "system.cpu.usage",
                value = 45.2,
                labels = mapOf("core" to "0"),
                timestamp = timestamp
            )
        )
        
        val events = listOf(
            Event(
                type = "wifi.connected",
                message = "Connected to WiFi network",
                severity = "INFO",
                timestamp = timestamp,
                metadata = mapOf("ssid" to "IoT-Network", "rssi" to -45)
            )
        )
        
        val prometheusMetrics = listOf(
            PrometheusMetric(
                name = "esp32_temperature_celsius",
                value = 23.5,
                labels = mapOf("sensor" to "internal"),
                help = "ESP32 internal temperature",
                type = "gauge"
            )
        )
        
        val request = TelemetryBatchRequest(
            deviceId = "esp32-001",
            timestamp = timestamp,
            otel = otelMetrics,
            events = events,
            metrics = prometheusMetrics
        )
        
        // Test serialization
        val json = objectMapper.writeValueAsString(request)
        assertNotNull(json)
        assertTrue(json.contains("esp32-001"))
        assertTrue(json.contains("system.cpu.usage"))
        
        // Test deserialization
        val deserializedRequest: TelemetryBatchRequest = objectMapper.readValue(json)
        assertEquals(request.deviceId, deserializedRequest.deviceId)
        assertEquals(request.timestamp, deserializedRequest.timestamp)
        assertEquals(request.otel?.size, deserializedRequest.otel?.size)
        assertEquals(request.events?.size, deserializedRequest.events?.size)
        assertEquals(request.metrics?.size, deserializedRequest.metrics?.size)
    }

    @Test
    fun `OtelMetric handles all field types correctly`() {
        val timestamp = Instant.parse("2025-07-20T10:30:00Z")
        val metric = OtelMetric(
            name = "memory.usage",
            value = 1024.0,
            labels = mapOf("type" to "heap", "unit" to "bytes"),
            timestamp = timestamp
        )
        
        val json = objectMapper.writeValueAsString(metric)
        val deserialized: OtelMetric = objectMapper.readValue(json)
        
        assertEquals(metric.name, deserialized.name)
        assertEquals(metric.value, deserialized.value)
        assertEquals(metric.labels, deserialized.labels)
        assertEquals(metric.timestamp, deserialized.timestamp)
    }

    @Test
    fun `Event handles optional metadata correctly`() {
        val timestamp = Instant.parse("2025-07-20T10:30:00Z")
        
        // Test with metadata
        val eventWithMetadata = Event(
            type = "error",
            message = "Connection failed",
            severity = "ERROR",
            timestamp = timestamp,
            metadata = mapOf("error_code" to 500, "retry_count" to 3)
        )
        
        val jsonWithMetadata = objectMapper.writeValueAsString(eventWithMetadata)
        val deserializedWithMetadata: Event = objectMapper.readValue(jsonWithMetadata)
        assertEquals(eventWithMetadata.metadata, deserializedWithMetadata.metadata)
        
        // Test without metadata
        val eventWithoutMetadata = Event(
            type = "info",
            message = "System started",
            severity = "INFO",
            timestamp = timestamp,
            metadata = null
        )
        
        val jsonWithoutMetadata = objectMapper.writeValueAsString(eventWithoutMetadata)
        val deserializedWithoutMetadata: Event = objectMapper.readValue(jsonWithoutMetadata)
        assertNull(deserializedWithoutMetadata.metadata)
    }

    @Test
    fun `PrometheusMetric handles optional fields correctly`() {
        // Test with all fields
        val fullMetric = PrometheusMetric(
            name = "temperature",
            value = 25.0,
            labels = mapOf("location" to "indoor"),
            help = "Temperature reading",
            type = "gauge"
        )
        
        val fullJson = objectMapper.writeValueAsString(fullMetric)
        val deserializedFull: PrometheusMetric = objectMapper.readValue(fullJson)
        assertEquals(fullMetric.help, deserializedFull.help)
        assertEquals(fullMetric.type, deserializedFull.type)
        
        // Test with minimal fields
        val minimalMetric = PrometheusMetric(
            name = "counter",
            value = 1.0,
            labels = emptyMap(),
            help = null,
            type = null
        )
        
        val minimalJson = objectMapper.writeValueAsString(minimalMetric)
        val deserializedMinimal: PrometheusMetric = objectMapper.readValue(minimalJson)
        assertNull(deserializedMinimal.help)
        assertNull(deserializedMinimal.type)
    }

    @Test
    fun `TelemetryResponse serializes correctly`() {
        val response = TelemetryResponse(
            status = "accepted",
            validatedMetrics = 5,
            truncatedOtel = false,
            truncatedEvents = false,
            droppedPrometheusMetrics = 2
        )
        
        val json = objectMapper.writeValueAsString(response)
        val deserialized: TelemetryResponse = objectMapper.readValue(json)
        
        assertEquals(response.status, deserialized.status)
        assertEquals(response.validatedMetrics, deserialized.validatedMetrics)
        assertEquals(response.truncatedOtel, deserialized.truncatedOtel)
        assertEquals(response.truncatedEvents, deserialized.truncatedEvents)
        assertEquals(response.droppedPrometheusMetrics, deserialized.droppedPrometheusMetrics)
    }

    @Test
    fun `handles empty and null arrays correctly`() {
        val timestamp = Instant.parse("2025-07-20T10:30:00Z")
        
        // Test with empty arrays
        val requestWithEmpty = TelemetryBatchRequest(
            deviceId = "esp32-001",
            timestamp = timestamp,
            otel = emptyList(),
            events = emptyList(),
            metrics = emptyList()
        )
        
        val jsonEmpty = objectMapper.writeValueAsString(requestWithEmpty)
        val deserializedEmpty: TelemetryBatchRequest = objectMapper.readValue(jsonEmpty)
        assertTrue(deserializedEmpty.otel?.isEmpty() == true)
        assertTrue(deserializedEmpty.events?.isEmpty() == true)
        assertTrue(deserializedEmpty.metrics?.isEmpty() == true)
        
        // Test with null arrays
        val requestWithNull = TelemetryBatchRequest(
            deviceId = "esp32-001",
            timestamp = timestamp,
            otel = null,
            events = null,
            metrics = null
        )
        
        val jsonNull = objectMapper.writeValueAsString(requestWithNull)
        val deserializedNull: TelemetryBatchRequest = objectMapper.readValue(jsonNull)
        assertNull(deserializedNull.otel)
        assertNull(deserializedNull.events)
        assertNull(deserializedNull.metrics)
    }
}