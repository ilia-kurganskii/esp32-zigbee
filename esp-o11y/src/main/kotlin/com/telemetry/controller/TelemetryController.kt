package com.telemetry.controller

import com.telemetry.model.*
import com.telemetry.service.ValidationService
import org.slf4j.MDC
import io.micrometer.core.annotation.Timed
import org.slf4j.LoggerFactory
import org.springframework.http.HttpStatus
import org.springframework.http.ResponseEntity
import org.springframework.web.bind.annotation.*
import org.springframework.web.bind.MethodArgumentNotValidException
import org.springframework.http.converter.HttpMessageNotReadableException
import jakarta.validation.Valid
import java.time.Instant
import io.swagger.v3.oas.annotations.Operation
import io.swagger.v3.oas.annotations.Parameter
import io.swagger.v3.oas.annotations.media.Content
import io.swagger.v3.oas.annotations.media.ExampleObject
import io.swagger.v3.oas.annotations.media.Schema
import io.swagger.v3.oas.annotations.responses.ApiResponse
import io.swagger.v3.oas.annotations.responses.ApiResponses
import io.swagger.v3.oas.annotations.security.SecurityRequirement
import io.swagger.v3.oas.annotations.tags.Tag

@Tag(name = "Telemetry", description = "Telemetry data collection endpoints")
@RestController
@RequestMapping("/api/v1")
@SecurityRequirement(name = "ApiKeyAuth")
class TelemetryController(
    private val validationService: ValidationService,
    private val metricsService: com.telemetry.service.MetricsService
) {

    private val logger = LoggerFactory.getLogger(TelemetryController::class.java)

    @Operation(
        summary = "Submit telemetry data from ESP32 device",
        description = "Submit a batch of telemetry data including OpenTelemetry metrics, events, and Prometheus metrics. The service validates the data and forwards it to Grafana collector. Validation Rules: Prometheus metrics are filtered using a whitelist, OTEL data size is limited, Events array size is limited. Rate Limits: Maximum 1000 OTEL metrics, 100 events, 100 Prometheus metrics per request.",
        requestBody = io.swagger.v3.oas.annotations.parameters.RequestBody(
            description = "Telemetry data batch from ESP32 device",
            required = true,
            content = [
                Content(
                    mediaType = "application/json",
                    examples = [
                        ExampleObject(
                            name = "validRequest",
                            summary = "Valid telemetry request with all data types",
                            value = "{\"deviceId\":\"esp32-001\",\"timestamp\":\"2025-07-20T10:30:00Z\",\"otel\":[{\"name\":\"system.cpu.usage\",\"value\":45.2,\"labels\":{\"core\":\"0\"},\"timestamp\":\"2025-07-20T10:30:00Z\"}],\"events\":[{\"type\":\"wifi.connected\",\"message\":\"Connected to WiFi network\",\"severity\":\"INFO\",\"timestamp\":\"2025-07-20T10:29:58Z\",\"metadata\":{\"ssid\":\"IoT-Network\",\"rssi\":-45}}],\"metrics\":[{\"name\":\"esp32_temperature_celsius\",\"value\":23.5,\"labels\":{\"sensor\":\"internal\"},\"help\":\"ESP32 internal temperature\",\"type\":\"gauge\"}]}"
                        ),
                        ExampleObject(
                            name = "minimalRequest",
                            summary = "Minimal valid request",
                            value = "{\"deviceId\":\"esp32-002\",\"timestamp\":\"2025-07-20T10:30:00Z\"}"
                        )
                    ]
                )
            ]
        ),
        responses = [
            ApiResponse(
                responseCode = "200",
                description = "Telemetry data processed successfully",
                content = [
                    Content(
                        mediaType = "application/json",
                        examples = [
                            ExampleObject(
                                name = "success",
                                summary = "Successfully processed without warnings",
                                value = "{\"status\":\"accepted\",\"validatedMetrics\":5,\"truncatedOtel\":false,\"truncatedEvents\":false,\"droppedPrometheusMetrics\":0}"
                            ),
                            ExampleObject(
                                name = "successWithWarnings",
                                summary = "Successfully processed with warnings",
                                value = "{\"status\":\"accepted_with_warnings\",\"validatedMetrics\":3,\"truncatedOtel\":true,\"truncatedEvents\":false,\"droppedPrometheusMetrics\":2,\"warnings\":[\"OTEL data truncated due to size limit\",\"Dropped 2 non-whitelisted Prometheus metrics\"]}"
                            )
                        ]
                    )
                ]
            ),
            ApiResponse(
                responseCode = "400",
                description = "Bad request - invalid JSON or validation failed",
                content = [
                    Content(
                        mediaType = "application/json",
                        examples = [
                            ExampleObject(
                                name = "invalidJson",
                                summary = "Invalid JSON format",
                                value = "{\"error\":\"Bad Request\",\"message\":\"Invalid JSON format\",\"path\":\"/api/v1/telemetry\"}"
                            ),
                            ExampleObject(
                                name = "validationFailed",
                                summary = "Request validation failed",
                                value = "{\"error\":\"Validation Failed\",\"message\":\"Request validation failed\",\"path\":\"/api/v1/telemetry\",\"details\":[\"deviceId: Device ID cannot be blank\",\"timestamp: Timestamp cannot be null\"]}"
                            )
                        ]
                    )
                ]
            ),
            ApiResponse(
                responseCode = "401",
                description = "Unauthorized - invalid or missing API key",
                content = [
                    Content(
                        mediaType = "application/json"
                    )
                ]
            ),
            ApiResponse(
                responseCode = "500",
                description = "Internal server error",
                content = [
                    Content(
                        mediaType = "application/json"
                    )
                ]
            )
        ]
    )
    @PostMapping("/telemetry", consumes = ["application/json"], produces = ["application/json"])
    @Timed(value = "telemetry_requests", description = "Time taken to process telemetry requests")
    fun receiveTelemetryData(
        @Valid @RequestBody request: TelemetryBatchRequest
    ): ResponseEntity<Any> {
        
        // Set MDC context for this request
        MDC.put("deviceId", request.deviceId)
        MDC.put("operation", "telemetry_request")
        
        try {
            logger.info("Received telemetry data from device: {}", request.deviceId)
            
            // Validate the request using ValidationService
            val validationResult = validationService.validate(request)
            
            // Note: Authentication is now handled by Spring Security
            // All validation issues are treated as warnings
            
            // Create response based on validation results
            val response = createSuccessResponse(validationResult)
            
            // Log successful processing
            MDC.put("validated_metrics", response.validatedMetrics.toString())
            MDC.put("warnings_count", (response.warnings?.size ?: 0).toString())
            logger.info("Telemetry request processed successfully: status={}", response.status)
            
            return ResponseEntity.ok(response)
            
        } catch (e: Exception) {
            logger.error("Unexpected error processing telemetry data from device: {}", request.deviceId, e)
            throw e
        } finally {
            MDC.clear()
        }
    }

    @ExceptionHandler(HttpMessageNotReadableException::class)
    fun handleJsonParseException(e: HttpMessageNotReadableException): ResponseEntity<ErrorResponse> {
        logger.warn("Invalid JSON format in request: {}", e.message)
        
        val errorResponse = ErrorResponse(
            error = "Bad Request",
            message = "Invalid JSON format",
            path = "/api/v1/telemetry"
        )
        
        return ResponseEntity.badRequest().body(errorResponse)
    }

    @ExceptionHandler(MethodArgumentNotValidException::class)
    fun handleValidationException(e: MethodArgumentNotValidException): ResponseEntity<ErrorResponse> {
        logger.warn("Request validation failed: {}", e.message)
        
        val details = e.bindingResult.fieldErrors.map { 
            "${it.field}: ${it.defaultMessage}" 
        }
        
        val errorResponse = ErrorResponse(
            error = "Validation Failed",
            message = "Request validation failed",
            path = "/api/v1/telemetry",
            details = details
        )
        
        return ResponseEntity.badRequest().body(errorResponse)
    }

    @ExceptionHandler(Exception::class)
    fun handleGeneralException(e: Exception): ResponseEntity<ErrorResponse> {
        logger.error("Internal server error: {}", e.message, e)
        
        val errorResponse = ErrorResponse(
            error = "Internal Server Error",
            message = "An unexpected error occurred",
            path = "/api/v1/telemetry"
        )
        
        return ResponseEntity.status(HttpStatus.INTERNAL_SERVER_ERROR).body(errorResponse)
    }

    private fun createSuccessResponse(validationResult: ValidationResult): TelemetryResponse {
        val statistics = validationResult.statistics
        val hasWarnings = validationResult.errors.isNotEmpty()
        
        val status = if (hasWarnings) "accepted_with_warnings" else "accepted"
        
        val warnings = if (hasWarnings) {
            validationResult.errors.map { it.message }
        } else null
        
        val totalValidatedMetrics = statistics.validatedOtelMetrics + 
                                  statistics.validatedEvents + 
                                  statistics.validatedPrometheusMetrics
        
        return TelemetryResponse(
            status = status,
            validatedMetrics = totalValidatedMetrics,
            truncatedOtel = statistics.truncatedOtel,
            truncatedEvents = statistics.truncatedEvents,
            droppedPrometheusMetrics = statistics.droppedPrometheusMetrics,
            warnings = warnings
        )
    }

}