package com.telemetry.config

import io.swagger.v3.oas.models.OpenAPI
import io.swagger.v3.oas.models.info.Contact
import io.swagger.v3.oas.models.info.Info
import io.swagger.v3.oas.models.info.License
import io.swagger.v3.oas.models.security.SecurityRequirement
import io.swagger.v3.oas.models.security.SecurityScheme
import io.swagger.v3.oas.models.servers.Server
import org.springframework.context.annotation.Bean
import org.springframework.context.annotation.Configuration

@Configuration
class OpenApiConfig {

    @Bean
    fun customOpenAPI(): OpenAPI {
        return OpenAPI()
            .info(
                Info()
                    .title("ESP32 Telemetry Collection Service")
                    .description(
                        """
                        A self-hosted Kotlin Spring Boot service that collects telemetry data from ESP32 devices via HTTP API calls and forwards the data to Grafana's collector. 
                        The service provides data validation, authentication, and comprehensive observability features.
                        
                        ## Features
                        - API key-based authentication for ESP32 clients
                        - Data validation with whitelist-based filtering for Prometheus metrics
                        - Size limit enforcement for OTEL data and events arrays
                        - Comprehensive error handling and logging
                        - Health check endpoints and metrics exposure
                        - Structured JSON logging with MDC context
                        
                        ## Authentication
                        The service uses API key authentication. Include a valid API key in the `X-API-Key` header for all requests.
                        """.trimIndent()
                    )
                    .version("1.0.0")
                    .contact(
                        Contact()
                            .name("ESP32 Telemetry Service Team")
                            .email("support@telemetry-service.com")
                    )
                    .license(
                        License()
                            .name("MIT")
                            .url("https://opensource.org/licenses/MIT")
                    )
            )
            .servers(
                listOf(
                    Server().url("http://localhost:8080").description("Local development server"),
                    Server().url("https://telemetry-service.example.com").description("Production server")
                )
            )
            .addSecurityItem(
                SecurityRequirement().addList("ApiKeyAuth")
            )
            .components(
                io.swagger.v3.oas.models.Components()
                    .addSecuritySchemes(
                        "ApiKeyAuth",
                        SecurityScheme()
                            .type(SecurityScheme.Type.APIKEY)
                            .`in`(SecurityScheme.In.HEADER)
                            .name("X-API-Key")
                            .description(
                                """
                                API key for authenticating ESP32 devices.
                                
                                Configure valid API keys using the `TELEMETRY_API_KEYS` environment variable:
                                ```
                                TELEMETRY_API_KEYS=esp32-device-key-001,esp32-device-key-002,esp32-device-key-003
                                ```
                                """.trimIndent()
                            )
                    )
            )
    }
}
