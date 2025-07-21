package com.telemetry.model

data class ValidationResult(
    val isValid: Boolean,
    val validatedRequest: TelemetryBatchRequest,
    val errors: List<ValidationError> = emptyList(),
    val statistics: ValidationStatistics
)

data class ValidationError(
    val type: ValidationErrorType,
    val message: String,
    val field: String? = null,
    val value: Any? = null
)

enum class ValidationErrorType {
    API_KEY_INVALID,
    PROMETHEUS_METRIC_NOT_WHITELISTED,
    OTEL_BATCH_SIZE_EXCEEDED,
    EVENTS_ARRAY_SIZE_EXCEEDED,
    INVALID_FIELD_VALUE
}

data class ValidationStatistics(
    val originalOtelMetrics: Int,
    val originalEvents: Int,
    val originalPrometheusMetrics: Int,
    val validatedOtelMetrics: Int,
    val validatedEvents: Int,
    val validatedPrometheusMetrics: Int,
    val droppedPrometheusMetrics: Int,
    val truncatedOtel: Boolean,
    val truncatedEvents: Boolean
)