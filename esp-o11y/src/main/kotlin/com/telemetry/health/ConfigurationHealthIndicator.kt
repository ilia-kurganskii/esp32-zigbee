package com.telemetry.health

import com.telemetry.config.TelemetryConfig
import org.slf4j.MDC
import org.slf4j.LoggerFactory
import org.springframework.boot.actuate.health.Health
import org.springframework.boot.actuate.health.HealthIndicator
import org.springframework.stereotype.Component

@Component
class ConfigurationHealthIndicator(
    private val telemetryConfig: TelemetryConfig
) : HealthIndicator {

    private val logger = LoggerFactory.getLogger(ConfigurationHealthIndicator::class.java)

    override fun health(): Health {
        return try {
            validateConfiguration()
            // Log health check with MDC context
            MDC.put("component", "configuration")
            MDC.put("health_status", "UP")
            logger.info("Configuration health check passed")
            MDC.clear()
            
            Health.up()
                .withDetail("configuration", "All configurations are valid")
                .withDetail("prometheus_whitelist_size", telemetryConfig.validation.prometheusWhitelist.size)
                .withDetail("api_keys_configured", telemetryConfig.security.apiKeys.isNotEmpty())
                .withDetail("grafana_url", telemetryConfig.grafana.collectorUrl)
                .build()
        } catch (e: Exception) {
            // Log health check failure with MDC context
            MDC.put("component", "configuration")
            MDC.put("health_status", "DOWN")
            MDC.put("error", e.message ?: "Unknown error")
            logger.error("Configuration health check failed: {}", e.message)
            MDC.clear()
            
            Health.down()
                .withDetail("configuration", "Configuration validation failed")
                .withDetail("error", e.message)
                .withException(e)
                .build()
        }
    }

    private fun validateConfiguration() {
        // Validate essential configuration
        require(telemetryConfig.validation.prometheusWhitelist.isNotEmpty()) {
            "Prometheus whitelist cannot be empty"
        }
        
        require(telemetryConfig.security.apiKeys.isNotEmpty()) {
            "At least one API key must be configured"
        }
        
        require(telemetryConfig.grafana.collectorUrl.isNotBlank()) {
            "Grafana collector URL must be configured"
        }
        
        require(telemetryConfig.validation.maxOtelBatchSize > 0) {
            "OTEL batch size must be positive"
        }
        
        require(telemetryConfig.validation.maxEventsArraySize > 0) {
            "Events array size must be positive"
        }
        
        // Validate API keys format
        telemetryConfig.security.apiKeys.forEach { key ->
            require(key.length >= 10) {
                "API keys must be at least 10 characters long"
            }
        }
    }
}