package com.telemetry.health

import org.springframework.boot.actuate.health.Health
import org.springframework.boot.actuate.health.HealthIndicator
import org.springframework.stereotype.Component

@Component
class MemoryHealthIndicator : HealthIndicator {

    companion object {
        private const val MEMORY_WARNING_THRESHOLD = 0.85 // 85%
        private const val MEMORY_CRITICAL_THRESHOLD = 0.95 // 95%
    }

    override fun health(): Health {
        val runtime = Runtime.getRuntime()
        val totalMemory = runtime.totalMemory()
        val freeMemory = runtime.freeMemory()
        val usedMemory = totalMemory - freeMemory
        val maxMemory = runtime.maxMemory()
        
        val memoryUsageRatio = usedMemory.toDouble() / maxMemory.toDouble()
        
        val healthBuilder = when {
            memoryUsageRatio >= MEMORY_CRITICAL_THRESHOLD -> {
                Health.down()
                    .withDetail("status", "CRITICAL - Memory usage is critically high")
            }
            memoryUsageRatio >= MEMORY_WARNING_THRESHOLD -> {
                Health.up()
                    .withDetail("status", "WARNING - Memory usage is high")
            }
            else -> {
                Health.up()
                    .withDetail("status", "OK - Memory usage is normal")
            }
        }
        
        return healthBuilder
            .withDetail("memory_used_bytes", usedMemory)
            .withDetail("memory_free_bytes", freeMemory)
            .withDetail("memory_total_bytes", totalMemory)
            .withDetail("memory_max_bytes", maxMemory)
            .withDetail("memory_usage_ratio", String.format("%.2f", memoryUsageRatio))
            .withDetail("memory_usage_percent", String.format("%.1f%%", memoryUsageRatio * 100))
            .withDetail("thresholds", mapOf(
                "warning" to "${(MEMORY_WARNING_THRESHOLD * 100).toInt()}%",
                "critical" to "${(MEMORY_CRITICAL_THRESHOLD * 100).toInt()}%"
            ))
            .build()
    }
}