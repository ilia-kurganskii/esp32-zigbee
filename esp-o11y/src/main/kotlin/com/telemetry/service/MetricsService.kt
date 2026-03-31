package com.telemetry.service

import com.telemetry.model.ValidationErrorType
import io.micrometer.core.instrument.Counter
import io.micrometer.core.instrument.MeterRegistry
import org.springframework.stereotype.Service

@Service
class MetricsService(private val meterRegistry: MeterRegistry) {

    /**
     * Record validation failure with type and reason
     */
    fun recordValidationFailure(deviceId: String, errorType: ValidationErrorType, reason: String) {
        Counter.builder("telemetry_validation_failures_total")
            .tag("device_id", deviceId)
            .tag("type", errorType.name)
            .tag("reason", reason)
            .description("Total number of validation failures")
            .register(meterRegistry)
            .increment()
    }

    /**
     * Record Prometheus metrics dropped
     */
    fun recordPrometheusMetricsDropped(deviceId: String, count: Int) {
        Counter.builder("telemetry_prometheus_metrics_dropped_total")
            .tag("device_id", deviceId)
            .tag("count", count.toString())
            .description("Total number of Prometheus metrics dropped due to whitelist filtering")
            .register(meterRegistry)
            .increment()
    }

    /**
     * Record OTEL batch truncation
     */
    fun recordOtelTruncation(deviceId: String, originalSize: Int, truncatedSize: Int) {
        Counter.builder("telemetry_otel_truncations_total")
            .tag("device_id", deviceId)
            .tag("original_size", originalSize.toString())
            .tag("truncated_size", truncatedSize.toString())
            .description("Total number of OTEL batch truncations")
            .register(meterRegistry)
            .increment()
    }

    /**
     * Record events array truncation
     */
    fun recordEventsTruncation(deviceId: String, originalSize: Int, truncatedSize: Int) {
        Counter.builder("telemetry_events_truncations_total")
            .tag("device_id", deviceId)
            .tag("original_size", originalSize.toString())
            .tag("truncated_size", truncatedSize.toString())
            .description("Total number of events array truncations")
            .register(meterRegistry)
            .increment()
    }

    /**
     * Get current metrics registry for custom metrics
     */
    fun getMeterRegistry(): MeterRegistry = meterRegistry
}