package com.telemetry.model

import com.fasterxml.jackson.annotation.JsonProperty
import jakarta.validation.constraints.NotBlank
import jakarta.validation.constraints.NotNull
import jakarta.validation.constraints.Size
import jakarta.validation.constraints.DecimalMin
import jakarta.validation.constraints.DecimalMax
import jakarta.validation.constraints.Pattern

data class PrometheusMetric(
    @field:NotBlank(message = "Metric name cannot be blank")
    @field:Size(min = 1, max = 100, message = "Metric name must be between 1 and 100 characters")
    @field:Pattern(regexp = "^[a-zA-Z_:][a-zA-Z0-9_:]*$", message = "Metric name must follow Prometheus naming conventions")
    @JsonProperty("name")
    val name: String,
    
    @field:NotNull(message = "Metric value cannot be null")
    @field:DecimalMin(value = "-1E+308", message = "Metric value too small")
    @field:DecimalMax(value = "1E+308", message = "Metric value too large")
    @JsonProperty("value")
    val value: Double,
    
    @field:NotNull(message = "Labels cannot be null")
    @field:Size(max = 30, message = "Maximum 30 labels allowed per Prometheus metric")
    @JsonProperty("labels")
    val labels: Map<String, String>,
    
    @field:Size(max = 500, message = "Help text must not exceed 500 characters")
    @JsonProperty("help")
    val help: String? = null,
    
    @field:Pattern(regexp = "^(counter|gauge|histogram|summary)$", message = "Metric type must be one of: counter, gauge, histogram, summary")
    @JsonProperty("type")
    val type: String? = null
)