package com.telemetry.model

import com.fasterxml.jackson.annotation.JsonProperty
import jakarta.validation.constraints.NotBlank
import jakarta.validation.constraints.NotNull
import java.time.Instant

data class OtelMetric(
    @field:NotBlank(message = "Metric name cannot be blank")
    @JsonProperty("name")
    val name: String,
    
    @field:NotNull(message = "Metric value cannot be null")
    @JsonProperty("value")
    val value: Double,
    
    @field:NotNull(message = "Labels cannot be null")
    @JsonProperty("labels")
    val labels: Map<String, String>,
    
    @field:NotNull(message = "Timestamp cannot be null")
    @JsonProperty("timestamp")
    val timestamp: Instant
)