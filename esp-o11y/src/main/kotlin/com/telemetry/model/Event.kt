package com.telemetry.model

import com.fasterxml.jackson.annotation.JsonProperty
import jakarta.validation.constraints.NotBlank
import jakarta.validation.constraints.NotNull
import java.time.Instant

data class Event(
    @field:NotBlank(message = "Event type cannot be blank")
    @JsonProperty("type")
    val type: String,
    
    @field:NotBlank(message = "Event message cannot be blank")
    @JsonProperty("message")
    val message: String,
    
    @field:NotBlank(message = "Event severity cannot be blank")
    @JsonProperty("severity")
    val severity: String,
    
    @field:NotNull(message = "Timestamp cannot be null")
    @JsonProperty("timestamp")
    val timestamp: Instant,
    
    @JsonProperty("metadata")
    val metadata: Map<String, Any>? = null
)