package com.telemetry.health

import com.telemetry.config.*
import org.junit.jupiter.api.BeforeEach
import org.junit.jupiter.api.Test
import org.junit.jupiter.api.Assertions.*
import org.springframework.boot.actuate.health.Status

class ConfigurationHealthIndicatorTest {

    private lateinit var healthIndicator: ConfigurationHealthIndicator
    private lateinit var validConfig: TelemetryConfig

    @BeforeEach
    fun setUp() {
        validConfig = TelemetryConfig().apply {
            validation = ValidationConfig().apply {
                prometheusWhitelist = setOf("cpu_usage", "memory_usage")
                maxOtelBatchSize = 1000
                maxEventsArraySize = 100
            }
            security = SecurityConfig().apply {
                apiKeys = setOf("valid-api-key-12345")
            }
            grafana = GrafanaConfig().apply {
                collectorUrl = "http://localhost:3000/api/push"
                timeout = java.time.Duration.ofSeconds(30)
                retryEnabled = false
            }
        }
        healthIndicator = ConfigurationHealthIndicator(validConfig)
    }

    @Test
    fun `health returns UP when configuration is valid`() {
        // When
        val health = healthIndicator.health()

        // Then
        assertEquals(Status.UP, health.status)
        assertEquals("All configurations are valid", health.details["configuration"])
        assertEquals(2, health.details["prometheus_whitelist_size"])
        assertEquals(true, health.details["api_keys_configured"])
        assertEquals("http://localhost:3000/api/push", health.details["grafana_url"])
    }

    @Test
    fun `health returns DOWN when prometheus whitelist is empty`() {
        // Given
        validConfig.validation.prometheusWhitelist = emptySet()
        val healthIndicator = ConfigurationHealthIndicator(validConfig)

        // When
        val health = healthIndicator.health()

        // Then
        assertEquals(Status.DOWN, health.status)
        assertEquals("Configuration validation failed", health.details["configuration"])
        assertTrue(health.details["error"].toString().contains("Prometheus whitelist cannot be empty"))
    }

    @Test
    fun `health returns DOWN when no API keys are configured`() {
        // Given
        validConfig.security.apiKeys = emptySet()
        val healthIndicator = ConfigurationHealthIndicator(validConfig)

        // When
        val health = healthIndicator.health()

        // Then
        assertEquals(Status.DOWN, health.status)
        assertEquals("Configuration validation failed", health.details["configuration"])
        assertTrue(health.details["error"].toString().contains("At least one API key must be configured"))
    }

    @Test
    fun `health returns DOWN when Grafana URL is blank`() {
        // Given
        validConfig.grafana.collectorUrl = ""
        val healthIndicator = ConfigurationHealthIndicator(validConfig)

        // When
        val health = healthIndicator.health()

        // Then
        assertEquals(Status.DOWN, health.status)
        assertEquals("Configuration validation failed", health.details["configuration"])
        assertTrue(health.details["error"].toString().contains("Grafana collector URL must be configured"))
    }

    @Test
    fun `health returns DOWN when API key is too short`() {
        // Given
        validConfig.security.apiKeys = setOf("short")
        val healthIndicator = ConfigurationHealthIndicator(validConfig)

        // When
        val health = healthIndicator.health()

        // Then
        assertEquals(Status.DOWN, health.status)
        assertEquals("Configuration validation failed", health.details["configuration"])
        assertTrue(health.details["error"].toString().contains("API keys must be at least 10 characters long"))
    }

    @Test
    fun `health returns DOWN when OTEL batch size is not positive`() {
        // Given
        validConfig.validation.maxOtelBatchSize = 0
        val healthIndicator = ConfigurationHealthIndicator(validConfig)

        // When
        val health = healthIndicator.health()

        // Then
        assertEquals(Status.DOWN, health.status)
        assertEquals("Configuration validation failed", health.details["configuration"])
        assertTrue(health.details["error"].toString().contains("OTEL batch size must be positive"))
    }

    @Test
    fun `health returns DOWN when events array size is not positive`() {
        // Given
        validConfig.validation.maxEventsArraySize = -1
        val healthIndicator = ConfigurationHealthIndicator(validConfig)

        // When
        val health = healthIndicator.health()

        // Then
        assertEquals(Status.DOWN, health.status)
        assertEquals("Configuration validation failed", health.details["configuration"])
        assertTrue(health.details["error"].toString().contains("Events array size must be positive"))
    }
}