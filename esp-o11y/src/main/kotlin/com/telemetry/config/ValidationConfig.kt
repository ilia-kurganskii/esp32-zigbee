package com.telemetry.config

import jakarta.validation.constraints.NotNull
import jakarta.validation.constraints.Positive

data class ValidationConfig(
    @field:NotNull(message = "Prometheus whitelist cannot be null")
    var prometheusWhitelist: Set<String> = setOf(
        "esp32_temperature_celsius",
        "esp32_memory_usage_bytes", 
        "esp32_wifi_signal_strength"
    ),
    
    @field:Positive(message = "Max OTEL batch size must be positive")
    var maxOtelBatchSize: Int = 1048576, // 1MB default
    
    @field:Positive(message = "Max events array size must be positive")
    var maxEventsArraySize: Int = 100
)