package com.telemetry.model

import com.fasterxml.jackson.annotation.JsonProperty
import jakarta.validation.constraints.NotBlank
import jakarta.validation.constraints.NotNull
import jakarta.validation.constraints.Size
import jakarta.validation.constraints.Pattern
import java.time.Instant

data class Event(
    @field:NotBlank(message = "Event type cannot be blank")
    @field:Size(min = 1, max = 50, message = "Event type must be between 1 and 50 characters")
    @field:Pattern(regexp = "^[a-zA-Z0-9_.-]+$", message = "Event type must contain only alphanumeric characters, underscores, dots, and hyphens")
    @JsonProperty("type")
    val type: String,
    
    @field:NotBlank(message = "Event message cannot be blank")
    @field:Size(min = 1, max = 1000, message = "Event message must be between 1 and 1000 characters")
    @JsonProperty("message")
    val message: String,
    
    @field:NotBlank(message = "Event severity cannot be blank")
    @field:Pattern(regexp = "^(DEBUG|INFO|WARN|ERROR|FATAL)$", message = "Event severity must be one of: DEBUG, INFO, WARN, ERROR, FATAL")
    @JsonProperty("severity")
    val severity: String,
    
    @field:NotNull(message = "Timestamp cannot be null")
    @JsonProperty("timestamp")
    val timestamp: Instant,
    
    @field:Size(max = 20, message = "Maximum 20 metadata entries allowed per event")
    @JsonProperty("metadata")
    val metadata: Map<String, Any>? = null
)