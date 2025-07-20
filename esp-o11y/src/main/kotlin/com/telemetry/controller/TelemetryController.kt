package com.telemetry.controller

import com.telemetry.model.*
import com.telemetry.service.ValidationService
import org.slf4j.LoggerFactory
import org.springframework.http.HttpStatus
import org.springframework.http.ResponseEntity
import org.springframework.web.bind.annotation.*
import org.springframework.web.bind.MethodArgumentNotValidException
import org.springframework.http.converter.HttpMessageNotReadableException
import jakarta.validation.Valid
import java.time.Instant

@RestController
@RequestMapping("/api/v1")
class TelemetryController(
    private val validationService: ValidationService
) {

    private val logger = LoggerFactory.getLogger(TelemetryController::class.java)

    @PostMapping("/telemetry", consumes = ["application/json"], produces = ["application/json"])
    fun receiveTelemetryData(
        @Valid @RequestBody request: TelemetryBatchRequest
    ): ResponseEntity<Any> {
        
        logger.info("Received telemetry data from device: {}", request.deviceId)
        
        try {
            // Validate the request using ValidationService
            val validationResult = validationService.validate(request)
            
            // Note: Authentication is now handled by Spring Security
            // All validation issues are treated as warnings
            
            // Create response based on validation results
            val response = createSuccessResponse(validationResult)
            
            logger.info("Successfully processed telemetry data from device: {} with status: {}", 
                       request.deviceId, response.status)
            
            return ResponseEntity.ok(response)
            
        } catch (e: Exception) {
            logger.error("Unexpected error processing telemetry data from device: {}", request.deviceId, e)
            throw e
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