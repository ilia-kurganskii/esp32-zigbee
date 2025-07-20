package com.telemetry.config

import jakarta.validation.constraints.NotEmpty

data class SecurityConfig(
    @field:NotEmpty(message = "API keys set cannot be empty")
    var apiKeys: Set<String> = emptySet()
)