package com.telemetry.model

import com.fasterxml.jackson.annotation.JsonProperty
import java.time.Instant

data class ErrorResponse(
    @JsonProperty("error")
    val error: String,
    
    @JsonProperty("message")
    val message: String,
    
    @JsonProperty("timestamp")
    val timestamp: Instant = Instant.now(),
    
    @JsonProperty("path")
    val path: String? = null,
    
    @JsonProperty("details")
    val details: List<String>? = null
)