package com.telemetry.config

import org.springframework.boot.context.properties.ConfigurationProperties
import org.springframework.stereotype.Component
import jakarta.validation.Valid
import jakarta.validation.constraints.NotNull

@Component
@ConfigurationProperties(prefix = "telemetry")
data class TelemetryConfig(
    @field:Valid
    @field:NotNull
    var server: ServerConfig = ServerConfig(),
    
    @field:Valid
    @field:NotNull
    var validation: ValidationConfig = ValidationConfig(),
    
    @field:Valid
    @field:NotNull
    var grafana: GrafanaConfig = GrafanaConfig(),
    
    @field:Valid
    @field:NotNull
    var security: SecurityConfig = SecurityConfig()
)