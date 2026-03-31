package com.telemetry

import org.junit.jupiter.api.Test
import org.springframework.boot.test.context.SpringBootTest
import org.springframework.boot.test.web.client.TestRestTemplate
import org.springframework.boot.test.web.server.LocalServerPort
import org.springframework.http.HttpStatus
import org.springframework.test.context.TestPropertySource

@SpringBootTest(webEnvironment = SpringBootTest.WebEnvironment.RANDOM_PORT)
class TelemetryServiceApplicationTest {

    @LocalServerPort
    private var port: Int = 0

    private val restTemplate = TestRestTemplate()

    @Test
    fun `application context loads successfully`() {
        // This test verifies that the Spring Boot application context loads without errors
        // If this test passes, it means all auto-configuration and bean wiring is working
    }

    @Test
    fun `health endpoint returns UP status`() {
        val response = restTemplate.getForEntity(
            "http://localhost:$port/actuator/health",
            String::class.java
        )
        
        println("Response status: ${response.statusCode}")
        println("Response body: ${response.body}")
        
        assert(response.statusCode == HttpStatus.OK) { "Expected 200 OK but got ${response.statusCode}" }
        assert(response.body?.contains("UP") == true) { "Expected response body to contain 'UP' but got: ${response.body}" }
    }
}