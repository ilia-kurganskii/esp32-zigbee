package com.telemetry.config

import jakarta.validation.constraints.Max
import jakarta.validation.constraints.Min
import jakarta.validation.constraints.NotBlank

data class ServerConfig(
    @field:Min(value = 1024, message = "Server port must be at least 1024")
    @field:Max(value = 65535, message = "Server port must be at most 65535")
    var port: Int = 8080,
    
    @field:NotBlank(message = "Server host cannot be blank")
    var host: String = "0.0.0.0"
)