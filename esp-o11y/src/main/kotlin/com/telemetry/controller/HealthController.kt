package com.telemetry.controller

import io.swagger.v3.oas.annotations.Operation
import io.swagger.v3.oas.annotations.responses.ApiResponse
import io.swagger.v3.oas.annotations.responses.ApiResponses
import io.swagger.v3.oas.annotations.tags.Tag
import org.springframework.boot.actuate.health.Health
import org.springframework.http.ResponseEntity
import org.springframework.web.bind.annotation.GetMapping
import org.springframework.web.bind.annotation.RequestMapping
import org.springframework.web.bind.annotation.RestController

@Tag(name = "Health", description = "Health check endpoints for monitoring")
@RestController
@RequestMapping("/actuator")
class HealthController {

    @Operation(
        summary = "Get overall health status",
        description = "Returns the overall health status of the application"
    )
    @ApiResponses(
        ApiResponse(
            responseCode = "200",
            description = "Health status"
        )
    )
    @GetMapping("/health")
    fun getHealth(): ResponseEntity<Health> {
        return ResponseEntity.ok().build()
    }

    @Operation(
        summary = "Liveness health check",
        description = "Liveness probe indicates if the application is running. Used by Kubernetes to restart the container if it's no longer alive."
    )
    @ApiResponses(
        ApiResponse(
            responseCode = "200",
            description = "Application is alive"
        ),
        ApiResponse(
            responseCode = "503",
            description = "Application is not alive"
        )
    )
    @GetMapping("/health/liveness")
    fun getLiveness(): ResponseEntity<Health> {
        return ResponseEntity.ok().build()
    }

    @Operation(
        summary = "Readiness health check",
        description = "Readiness probe indicates if the application is ready to serve traffic. Used by Kubernetes to determine if the container is ready to accept requests."
    )
    @ApiResponses(
        ApiResponse(
            responseCode = "200",
            description = "Application is ready"
        ),
        ApiResponse(
            responseCode = "503",
            description = "Application is not ready"
        )
    )
    @GetMapping("/health/readiness")
    fun getReadiness(): ResponseEntity<Health> {
        return ResponseEntity.ok().build()
    }
}

@Tag(name = "Metrics", description = "Metrics and monitoring endpoints")
@RestController
@RequestMapping("/actuator")
class MetricsController {

    @Operation(
        summary = "Prometheus metrics endpoint",
        description = "Exposes Prometheus metrics for monitoring the telemetry service. Includes request metrics, validation metrics, forwarding metrics, and system metrics."
    )
    @ApiResponses(
        ApiResponse(
            responseCode = "200",
            description = "Prometheus metrics in text format"
        )
    )
    @GetMapping("/prometheus")
    fun getPrometheusMetrics(): ResponseEntity<String> {
        return ResponseEntity.ok().build()
    }
}
