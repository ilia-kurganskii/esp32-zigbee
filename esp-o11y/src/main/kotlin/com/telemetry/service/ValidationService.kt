package com.telemetry.service

import com.telemetry.config.TelemetryConfig
import com.telemetry.model.*
import org.slf4j.MDC
import org.slf4j.LoggerFactory
import org.springframework.stereotype.Service
import kotlin.math.min

@Service
class ValidationService(
    private val config: TelemetryConfig,
    private val metricsService: MetricsService
) {

    private val logger = LoggerFactory.getLogger(ValidationService::class.java)

    fun validate(request: TelemetryBatchRequest): ValidationResult {
        val errors = mutableListOf<ValidationError>()
        
        // Track original counts
        val originalOtel = request.otel?.size ?: 0
        val originalEvents = request.events?.size ?: 0
        val originalPrometheus = request.metrics?.size ?: 0

        // Note: API key validation is now handled by Spring Security

        // Validate and filter Prometheus metrics
        val (validatedPrometheusMetrics, droppedPrometheusCount) = validatePrometheusMetrics(request.metrics, errors, request.deviceId)

        // Validate and truncate OTEL data
        val (validatedOtelMetrics, otelTruncated) = validateOtelMetrics(request.otel, errors, request.deviceId)

        // Validate and truncate events
        val (validatedEvents, eventsTruncated) = validateEvents(request.events, errors, request.deviceId)

        // Create validated request
        val validatedRequest = request.copy(
            otel = validatedOtelMetrics,
            events = validatedEvents,
            metrics = validatedPrometheusMetrics
        )

        // Create statistics
        val statistics = ValidationStatistics(
            originalOtelMetrics = originalOtel,
            originalEvents = originalEvents,
            originalPrometheusMetrics = originalPrometheus,
            validatedOtelMetrics = validatedOtelMetrics?.size ?: 0,
            validatedEvents = validatedEvents?.size ?: 0,
            validatedPrometheusMetrics = validatedPrometheusMetrics?.size ?: 0,
            droppedPrometheusMetrics = droppedPrometheusCount,
            truncatedOtel = otelTruncated,
            truncatedEvents = eventsTruncated
        )

        val isValid = true // All validation issues are now warnings, authentication handled by Spring Security

        // Record validation failure metrics
        errors.forEach { error ->
            metricsService.recordValidationFailure(request.deviceId, error.type, error.message)
        }

        // Log validation results with structured logging
        logValidationResults(request, statistics, errors)

        return ValidationResult(
            isValid = isValid,
            validatedRequest = validatedRequest,
            errors = errors,
            statistics = statistics
        )
    }

    // API key validation removed - now handled by Spring Security

    private fun validatePrometheusMetrics(
        metrics: List<PrometheusMetric>?,
        errors: MutableList<ValidationError>,
        deviceId: String
    ): Pair<List<PrometheusMetric>?, Int> {
        if (metrics == null) return null to 0

        val whitelist = config.validation.prometheusWhitelist
        val validMetrics = mutableListOf<PrometheusMetric>()
        var droppedCount = 0

        for (metric in metrics) {
            if (whitelist.contains(metric.name)) {
                validMetrics.add(metric)
            } else {
                droppedCount++
                errors.add(ValidationError(
                    type = ValidationErrorType.PROMETHEUS_METRIC_NOT_WHITELISTED,
                    message = "Prometheus metric '${metric.name}' not in whitelist",
                    field = "metrics[].name",
                    value = metric.name
                ))
                logger.warn("Dropped non-whitelisted Prometheus metric: {}", metric.name)
            }
        }

        // Record metrics for dropped Prometheus metrics
        if (droppedCount > 0) {
            metricsService.recordPrometheusMetricsDropped(deviceId, droppedCount)
        }

        return validMetrics to droppedCount
    }

    private fun validateOtelMetrics(
        otelMetrics: List<OtelMetric>?,
        errors: MutableList<ValidationError>,
        deviceId: String
    ): Pair<List<OtelMetric>?, Boolean> {
        if (otelMetrics == null) return null to false

        val maxSize = config.validation.maxOtelBatchSize
        val currentSize = estimateOtelDataSize(otelMetrics)

        if (currentSize <= maxSize) {
            return otelMetrics to false
        }

        // Truncate to fit within size limit - keep most recent entries
        val truncatedMetrics = truncateOtelToSize(otelMetrics, maxSize)
        
        errors.add(ValidationError(
            type = ValidationErrorType.OTEL_BATCH_SIZE_EXCEEDED,
            message = "OTEL batch size exceeded limit (${currentSize} > ${maxSize}), truncated to ${truncatedMetrics.size} metrics",
            field = "otel",
            value = currentSize
        ))
        
        logger.warn("OTEL batch size exceeded: {} > {}, truncated to {} metrics", 
                   currentSize, maxSize, truncatedMetrics.size)

        // Record metrics for OTEL truncation
        metricsService.recordOtelTruncation(deviceId, otelMetrics.size, truncatedMetrics.size)

        return truncatedMetrics to true
    }

    private fun validateEvents(
        events: List<Event>?,
        errors: MutableList<ValidationError>,
        deviceId: String
    ): Pair<List<Event>?, Boolean> {
        if (events == null) return null to false

        val maxSize = config.validation.maxEventsArraySize
        if (events.size <= maxSize) {
            return events to false
        }

        // Truncate to max size - keep most recent events
        val truncatedEvents = events.takeLast(maxSize)
        
        errors.add(ValidationError(
            type = ValidationErrorType.EVENTS_ARRAY_SIZE_EXCEEDED,
            message = "Events array size exceeded limit (${events.size} > ${maxSize}), truncated to ${maxSize} events",
            field = "events",
            value = events.size
        ))
        
        logger.warn("Events array size exceeded: {} > {}, truncated to {} events", 
                   events.size, maxSize, maxSize)

        // Record metrics for events truncation
        metricsService.recordEventsTruncation(deviceId, events.size, maxSize)

        return truncatedEvents to true
    }

    private fun estimateOtelDataSize(metrics: List<OtelMetric>): Int {
        // Simple estimation: each metric name + value + labels + timestamp
        return metrics.sumOf { metric ->
            metric.name.length + 
            8 + // Double value
            metric.labels.entries.sumOf { it.key.length + it.value.length } +
            20 // Timestamp
        }
    }

    private fun truncateOtelToSize(metrics: List<OtelMetric>, maxSize: Int): List<OtelMetric> {
        // Simple approach: calculate how many metrics we can fit
        val avgMetricSize = if (metrics.isNotEmpty()) estimateOtelDataSize(metrics) / metrics.size else 100
        val maxMetrics = maxSize / avgMetricSize
        
        return if (maxMetrics > 0) {
            metrics.takeLast(min(maxMetrics, metrics.size))
        } else {
            emptyList()
        }
    }

    private fun logValidationResults(
        request: TelemetryBatchRequest,
        statistics: ValidationStatistics,
        errors: List<ValidationError>
    ) {
        val errorMessages = errors.map { "${it.type.name}: ${it.message}" }
        
        // Log validation results with MDC context
        MDC.put("deviceId", request.deviceId)
        MDC.put("operation", "validation")
        MDC.put("otel_events", statistics.validatedOtelMetrics.toString())
        MDC.put("custom_events", statistics.validatedEvents.toString())
        MDC.put("prometheus_metrics", statistics.validatedPrometheusMetrics.toString())
        MDC.put("error_count", errors.size.toString())
        
        if (errors.isNotEmpty()) {
            logger.warn("Validation completed with errors: {}", errorMessages.joinToString())
        } else {
            logger.info("Validation completed successfully")
        }
        
        MDC.clear()
    }
}