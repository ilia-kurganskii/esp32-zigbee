package com.telemetry.model

import com.fasterxml.jackson.annotation.JsonProperty

data class TelemetryResponse(
    @JsonProperty("status")
    val status: String,
    
    @JsonProperty("validatedMetrics")
    val validatedMetrics: Int,
    
    @JsonProperty("truncatedOtel")
    val truncatedOtel: Boolean,
    
    @JsonProperty("truncatedEvents")
    val truncatedEvents: Boolean,
    
    @JsonProperty("droppedPrometheusMetrics")
    val droppedPrometheusMetrics: Int,
    
    @JsonProperty("warnings")
    val warnings: List<String>? = null
)