package com.telemetry.config

import org.junit.jupiter.api.Test
import org.springframework.boot.test.context.SpringBootTest
import org.springframework.test.context.TestPropertySource
import org.springframework.beans.factory.annotation.Autowired
import org.junit.jupiter.api.Assertions.*

@SpringBootTest
@TestPropertySource(properties = [
    "telemetry.server.port=8080",
    "telemetry.server.host=0.0.0.0",
    "telemetry.validation.prometheus-whitelist=cpu_usage,memory_usage,temperature",
    "telemetry.validation.max-otel-batch-size=1048576",
    "telemetry.validation.max-events-array-size=100",
    "telemetry.grafana.collector-url=http://localhost:3000/api/push",
    "telemetry.grafana.timeout=30s",
    "telemetry.security.api-keys=key1,key2,key3"
])
class TelemetryConfigTest {

    @Autowired
    private lateinit var telemetryConfig: TelemetryConfig

    @Test
    fun `configuration loads successfully with all properties`() {
        assertNotNull(telemetryConfig)
        
        // Test server configuration
        assertEquals(8080, telemetryConfig.server.port)
        assertEquals("0.0.0.0", telemetryConfig.server.host)
        
        // Test validation configuration
        assertTrue(telemetryConfig.validation.prometheusWhitelist.contains("cpu_usage"))
        assertTrue(telemetryConfig.validation.prometheusWhitelist.contains("memory_usage"))
        assertTrue(telemetryConfig.validation.prometheusWhitelist.contains("temperature"))
        assertEquals(1048576, telemetryConfig.validation.maxOtelBatchSize)
        assertEquals(100, telemetryConfig.validation.maxEventsArraySize)
        
        // Test Grafana configuration
        assertEquals("http://localhost:3000/api/push", telemetryConfig.grafana.collectorUrl)
        assertEquals(30, telemetryConfig.grafana.timeout.seconds)
        
        // Test security configuration
        assertTrue(telemetryConfig.security.apiKeys.contains("key1"))
        assertTrue(telemetryConfig.security.apiKeys.contains("key2"))
        assertTrue(telemetryConfig.security.apiKeys.contains("key3"))
        assertEquals(3, telemetryConfig.security.apiKeys.size)
    }

    @Test
    fun `configuration has proper defaults when not specified`() {
        // This test will verify that when properties are not set,
        // the configuration uses sensible defaults
        // We'll need a separate test context for this
    }
}