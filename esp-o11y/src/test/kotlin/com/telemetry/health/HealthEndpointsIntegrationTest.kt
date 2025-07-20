package com.telemetry.health

import org.junit.jupiter.api.Test
import org.springframework.beans.factory.annotation.Autowired
import org.springframework.boot.test.autoconfigure.web.servlet.AutoConfigureMockMvc
import org.springframework.boot.test.context.SpringBootTest
import org.springframework.test.context.TestPropertySource
import org.springframework.test.web.servlet.MockMvc
import org.springframework.test.web.servlet.request.MockMvcRequestBuilders.get
import org.springframework.test.web.servlet.result.MockMvcResultMatchers.*

@SpringBootTest(webEnvironment = SpringBootTest.WebEnvironment.RANDOM_PORT)
@AutoConfigureMockMvc
@TestPropertySource(properties = [
    "telemetry.validation.prometheus-whitelist=cpu_usage,memory_usage",
    "telemetry.security.api-keys=test-api-key-12345"
])
class HealthEndpointsIntegrationTest {

    @Autowired
    private lateinit var mockMvc: MockMvc

    @Test
    fun `actuator health endpoint returns OK`() {
        mockMvc.perform(get("/actuator/health"))
            .andExpect(status().isOk)
            .andExpect(content().contentType("application/vnd.spring-boot.actuator.v3+json"))
            .andExpect(jsonPath("$.status").value("UP"))
            .andExpect(jsonPath("$.components").exists())
    }

    @Test
    fun `health endpoint includes custom health indicators`() {
        mockMvc.perform(get("/actuator/health"))
            .andExpect(status().isOk)
            .andExpect(jsonPath("$.components.configuration").exists())
            .andExpect(jsonPath("$.components.memory").exists())
            .andExpect(jsonPath("$.components.configuration.status").value("UP"))
            .andExpect(jsonPath("$.components.memory.status").exists())
    }

    @Test
    fun `configuration health indicator provides details`() {
        mockMvc.perform(get("/actuator/health"))
            .andExpect(status().isOk)
            .andExpect(jsonPath("$.components.configuration.details.configuration").value("All configurations are valid"))
            .andExpect(jsonPath("$.components.configuration.details.prometheus_whitelist_size").value(2))
            .andExpect(jsonPath("$.components.configuration.details.api_keys_configured").value(true))
            .andExpect(jsonPath("$.components.configuration.details.grafana_url").exists())
    }

    @Test
    fun `memory health indicator provides memory details`() {
        mockMvc.perform(get("/actuator/health"))
            .andExpect(status().isOk)
            .andExpect(jsonPath("$.components.memory.details.memory_used_bytes").exists())
            .andExpect(jsonPath("$.components.memory.details.memory_free_bytes").exists())
            .andExpect(jsonPath("$.components.memory.details.memory_total_bytes").exists())
            .andExpect(jsonPath("$.components.memory.details.memory_max_bytes").exists())
            .andExpect(jsonPath("$.components.memory.details.memory_usage_ratio").exists())
            .andExpect(jsonPath("$.components.memory.details.memory_usage_percent").exists())
            .andExpect(jsonPath("$.components.memory.details.thresholds").exists())
            .andExpect(jsonPath("$.components.memory.details.status").exists())
    }

    @Test
    fun `liveness probe endpoint is available`() {
        mockMvc.perform(get("/actuator/health/liveness"))
            .andExpect(status().isOk)
            .andExpect(content().contentType("application/vnd.spring-boot.actuator.v3+json"))
            .andExpect(jsonPath("$.status").value("UP"))
    }

    @Test
    fun `readiness probe endpoint is available`() {
        mockMvc.perform(get("/actuator/health/readiness"))
            .andExpect(status().isOk)
            .andExpect(content().contentType("application/vnd.spring-boot.actuator.v3+json"))
            .andExpect(jsonPath("$.status").value("UP"))
    }

    // Note: Prometheus endpoint requires micrometer-registry-prometheus to be fully configured
    // This test is commented out as Prometheus metrics are handled separately

}