package com.telemetry.model

import com.fasterxml.jackson.annotation.JsonProperty
import jakarta.validation.Valid
import jakarta.validation.constraints.NotBlank
import jakarta.validation.constraints.NotNull
import jakarta.validation.constraints.Size
import java.time.Instant

data class TelemetryBatchRequest(
    @field:NotBlank(message = "Device ID cannot be blank")
    @field:Size(min = 1, max = 50, message = "Device ID must be between 1 and 50 characters")
    @JsonProperty("deviceId")
    val deviceId: String,
    
    @field:NotNull(message = "Timestamp cannot be null")
    @JsonProperty("timestamp")
    val timestamp: Instant,
    
    @field:Valid
    @field:Size(max = 1000, message = "Maximum 1000 OTEL metrics allowed per request")
    @JsonProperty("otel")
    val otel: List<OtelMetric>? = null,
    
    @field:Valid
    @field:Size(max = 100, message = "Maximum 100 events allowed per request")
    @JsonProperty("events")
    val events: List<Event>? = null,
    
    @field:Valid
    @field:Size(max = 100, message = "Maximum 100 Prometheus metrics allowed per request")
    @JsonProperty("metrics")
    val metrics: List<PrometheusMetric>? = null
)