package com.telemetry.model

import com.fasterxml.jackson.annotation.JsonProperty
import jakarta.validation.constraints.NotBlank
import jakarta.validation.constraints.NotNull
import jakarta.validation.constraints.Size
import jakarta.validation.constraints.DecimalMin
import jakarta.validation.constraints.DecimalMax
import java.time.Instant

data class OtelMetric(
    @field:NotBlank(message = "Metric name cannot be blank")
    @field:Size(min = 1, max = 100, message = "Metric name must be between 1 and 100 characters")
    @JsonProperty("name")
    val name: String,
    
    @field:NotNull(message = "Metric value cannot be null")
    @field:DecimalMin(value = "-1E+308", message = "Metric value too small")
    @field:DecimalMax(value = "1E+308", message = "Metric value too large")
    @JsonProperty("value")
    val value: Double,
    
    @field:NotNull(message = "Labels cannot be null")
    @field:Size(max = 50, message = "Maximum 50 labels allowed per metric")
    @JsonProperty("labels")
    val labels: Map<String, String>,
    
    @field:NotNull(message = "Timestamp cannot be null")
    @JsonProperty("timestamp")
    val timestamp: Instant
)