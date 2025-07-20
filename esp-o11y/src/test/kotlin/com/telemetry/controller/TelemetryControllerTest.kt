package com.telemetry.controller

import com.telemetry.model.*
import com.telemetry.service.ValidationService
import com.fasterxml.jackson.databind.ObjectMapper
import org.junit.jupiter.api.Test
import org.mockito.kotlin.any
import org.mockito.kotlin.verify
import org.mockito.kotlin.whenever
import org.springframework.beans.factory.annotation.Autowired
import org.springframework.boot.test.autoconfigure.web.servlet.WebMvcTest
import org.springframework.boot.test.mock.mockito.MockBean
import org.springframework.http.MediaType
import org.springframework.test.context.TestPropertySource
import org.springframework.test.web.servlet.MockMvc
import org.springframework.test.web.servlet.request.MockMvcRequestBuilders.post
import org.springframework.test.web.servlet.result.MockMvcResultMatchers.*
import java.time.Instant

@WebMvcTest(controllers = [TelemetryController::class],
    excludeAutoConfiguration = [
        org.springframework.boot.autoconfigure.security.servlet.SecurityAutoConfiguration::class
    ])
@TestPropertySource(properties = [
    "telemetry.validation.prometheus-whitelist=cpu_usage,memory_usage"
])
class TelemetryControllerTest {

    @Autowired
    private lateinit var mockMvc: MockMvc

    @Autowired
    private lateinit var objectMapper: ObjectMapper

    @MockBean
    private lateinit var validationService: ValidationService
    
    @MockBean 
    private lateinit var telemetryConfig: com.telemetry.config.TelemetryConfig

    @Test
    fun `POST telemetry with valid request returns success response`() {
        // Given
        val request = createValidRequest()
        val validationResult = ValidationResult(
            isValid = true,
            validatedRequest = request,
            errors = emptyList(),
            statistics = ValidationStatistics(
                originalOtelMetrics = 1,
                originalEvents = 1,
                originalPrometheusMetrics = 1,
                validatedOtelMetrics = 1,
                validatedEvents = 1,
                validatedPrometheusMetrics = 1,
                droppedPrometheusMetrics = 0,
                truncatedOtel = false,
                truncatedEvents = false
            )
        )

        whenever(validationService.validate(any())).thenReturn(validationResult)

        // When & Then
        mockMvc.perform(
            post("/api/v1/telemetry")
                .contentType(MediaType.APPLICATION_JSON)
                .content(objectMapper.writeValueAsString(request))
        )
        .andExpect(status().isOk)
        .andExpect(content().contentType(MediaType.APPLICATION_JSON))
        .andExpect(jsonPath("$.status").value("accepted"))
        .andExpect(jsonPath("$.validatedMetrics").value(3)) // otel + events + prometheus
        .andExpect(jsonPath("$.truncatedOtel").value(false))
        .andExpect(jsonPath("$.truncatedEvents").value(false))
        .andExpect(jsonPath("$.droppedPrometheusMetrics").value(0))

        verify(validationService).validate(any())
    }


    @Test
    fun `POST telemetry with malformed JSON returns 400 bad request`() {
        // When & Then
        mockMvc.perform(
            post("/api/v1/telemetry")
                .contentType(MediaType.APPLICATION_JSON)
                .content("{\"invalid\": json}")
        )
        .andExpect(status().isBadRequest)
        .andExpect(content().contentType(MediaType.APPLICATION_JSON))
        .andExpect(jsonPath("$.error").value("Bad Request"))
        .andExpect(jsonPath("$.message").value("Invalid JSON format"))
    }

    @Test
    fun `POST telemetry with validation warnings returns success with warnings`() {
        // Given
        val request = createValidRequest()
        val validationResult = ValidationResult(
            isValid = true,
            validatedRequest = request.copy(
                metrics = listOf(request.metrics!!.first()) // One metric filtered out
            ),
            errors = listOf(
                ValidationError(
                    type = ValidationErrorType.PROMETHEUS_METRIC_NOT_WHITELISTED,
                    message = "Prometheus metric 'custom_metric' not in whitelist",
                    field = "metrics[].name",
                    value = "custom_metric"
                )
            ),
            statistics = ValidationStatistics(
                originalOtelMetrics = 1,
                originalEvents = 1,
                originalPrometheusMetrics = 2,
                validatedOtelMetrics = 1,
                validatedEvents = 1,
                validatedPrometheusMetrics = 1,
                droppedPrometheusMetrics = 1,
                truncatedOtel = false,
                truncatedEvents = false
            )
        )

        whenever(validationService.validate(any())).thenReturn(validationResult)

        // When & Then
        mockMvc.perform(
            post("/api/v1/telemetry")
                .contentType(MediaType.APPLICATION_JSON)
                .content(objectMapper.writeValueAsString(request))
        )
        .andExpect(status().isOk)
        .andExpect(content().contentType(MediaType.APPLICATION_JSON))
        .andExpect(jsonPath("$.status").value("accepted_with_warnings"))
        .andExpect(jsonPath("$.droppedPrometheusMetrics").value(1))
        .andExpect(jsonPath("$.warnings").isArray)
        .andExpect(jsonPath("$.warnings[0]").value("Prometheus metric 'custom_metric' not in whitelist"))
    }

    @Test
    fun `POST telemetry with missing required fields returns 400 bad request`() {
        // When & Then - Empty JSON {} triggers JSON parse error due to missing required fields
        mockMvc.perform(
            post("/api/v1/telemetry")
                .contentType(MediaType.APPLICATION_JSON)
                .content("{}")
        )
        .andExpect(status().isBadRequest)
        .andExpect(content().contentType(MediaType.APPLICATION_JSON))
        .andExpect(jsonPath("$.error").value("Bad Request"))
        .andExpect(jsonPath("$.message").value("Invalid JSON format"))
    }

    @Test
    fun `POST telemetry with wrong content type returns 415 unsupported media type`() {
        // When & Then
        mockMvc.perform(
            post("/api/v1/telemetry")
                .contentType(MediaType.TEXT_PLAIN)
                .content("plain text")
        )
        .andExpect(status().isUnsupportedMediaType)
    }

    @Test
    fun `POST telemetry with empty body returns 400 bad request`() {
        // When & Then
        mockMvc.perform(
            post("/api/v1/telemetry")
                .contentType(MediaType.APPLICATION_JSON)
                .content("")
        )
        .andExpect(status().isBadRequest)
    }

    @Test
    fun `handles large payload truncation correctly`() {
        // Given
        val request = createValidRequest()
        val validationResult = ValidationResult(
            isValid = true,
            validatedRequest = request.copy(
                otel = request.otel?.take(5) // Simulate truncation
            ),
            errors = listOf(
                ValidationError(
                    type = ValidationErrorType.OTEL_BATCH_SIZE_EXCEEDED,
                    message = "OTEL batch size exceeded limit, truncated to 5 metrics",
                    field = "otel",
                    value = 1000
                )
            ),
            statistics = ValidationStatistics(
                originalOtelMetrics = 10,
                originalEvents = 1,
                originalPrometheusMetrics = 1,
                validatedOtelMetrics = 5,
                validatedEvents = 1,
                validatedPrometheusMetrics = 1,
                droppedPrometheusMetrics = 0,
                truncatedOtel = true,
                truncatedEvents = false
            )
        )

        whenever(validationService.validate(any())).thenReturn(validationResult)

        // When & Then
        mockMvc.perform(
            post("/api/v1/telemetry")
                .contentType(MediaType.APPLICATION_JSON)
                .content(objectMapper.writeValueAsString(request))
        )
        .andExpect(status().isOk)
        .andExpect(jsonPath("$.status").value("accepted_with_warnings"))
        .andExpect(jsonPath("$.truncatedOtel").value(true))
        .andExpect(jsonPath("$.warnings[0]").value("OTEL batch size exceeded limit, truncated to 5 metrics"))
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

    private fun createEmptyStatistics(): ValidationStatistics {
        return ValidationStatistics(
            originalOtelMetrics = 0,
            originalEvents = 0,
            originalPrometheusMetrics = 0,
            validatedOtelMetrics = 0,
            validatedEvents = 0,
            validatedPrometheusMetrics = 0,
            droppedPrometheusMetrics = 0,
            truncatedOtel = false,
            truncatedEvents = false
        )
    }
}