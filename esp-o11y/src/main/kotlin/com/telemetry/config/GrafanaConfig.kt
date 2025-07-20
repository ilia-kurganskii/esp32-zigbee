package com.telemetry.config

import jakarta.validation.constraints.NotBlank
import jakarta.validation.constraints.NotNull
import java.time.Duration

data class GrafanaConfig(
    @field:NotBlank(message = "Grafana collector URL cannot be blank")
    var collectorUrl: String = "",
    
    @field:NotNull(message = "Timeout cannot be null")
    var timeout: Duration = Duration.ofSeconds(30),
    
    var retryEnabled: Boolean = false
)