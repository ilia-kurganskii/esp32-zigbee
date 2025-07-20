package com.telemetry.model

import com.fasterxml.jackson.annotation.JsonProperty
import jakarta.validation.Valid
import jakarta.validation.constraints.NotBlank
import jakarta.validation.constraints.NotNull
import java.time.Instant

data class TelemetryBatchRequest(
    @field:NotBlank(message = "API key cannot be blank")
    @JsonProperty("apiKey")
    val apiKey: String,
    
    @field:NotBlank(message = "Device ID cannot be blank")
    @JsonProperty("deviceId")
    val deviceId: String,
    
    @field:NotNull(message = "Timestamp cannot be null")
    @JsonProperty("timestamp")
    val timestamp: Instant,
    
    @field:Valid
    @JsonProperty("otel")
    val otel: List<OtelMetric>? = null,
    
    @field:Valid
    @JsonProperty("events")
    val events: List<Event>? = null,
    
    @field:Valid
    @JsonProperty("metrics")
    val metrics: List<PrometheusMetric>? = null
)