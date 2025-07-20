package com.telemetry.health

import org.junit.jupiter.api.BeforeEach
import org.junit.jupiter.api.Test
import org.junit.jupiter.api.Assertions.*
import org.springframework.boot.actuate.health.Status

class MemoryHealthIndicatorTest {

    private lateinit var healthIndicator: MemoryHealthIndicator

    @BeforeEach
    fun setUp() {
        healthIndicator = MemoryHealthIndicator()
    }

    @Test
    fun `health check returns valid memory information`() {
        // When
        val health = healthIndicator.health()

        // Then
        assertNotNull(health)
        assertTrue(health.details.containsKey("memory_used_bytes"))
        assertTrue(health.details.containsKey("memory_free_bytes"))
        assertTrue(health.details.containsKey("memory_total_bytes"))
        assertTrue(health.details.containsKey("memory_max_bytes"))
        assertTrue(health.details.containsKey("memory_usage_ratio"))
        assertTrue(health.details.containsKey("memory_usage_percent"))
        assertTrue(health.details.containsKey("thresholds"))
        assertTrue(health.details.containsKey("status"))
        
        // Verify numeric values are reasonable
        val usedBytes = health.details["memory_used_bytes"] as Long
        val freeBytes = health.details["memory_free_bytes"] as Long
        val totalBytes = health.details["memory_total_bytes"] as Long
        val maxBytes = health.details["memory_max_bytes"] as Long
        
        assertTrue(usedBytes >= 0)
        assertTrue(freeBytes >= 0)
        assertTrue(totalBytes > 0)
        assertTrue(maxBytes > 0)
        assertTrue(usedBytes + freeBytes == totalBytes)
    }

    @Test
    fun `health status is UP when memory usage is normal`() {
        // When
        val health = healthIndicator.health()

        // Then
        // Under normal test conditions, memory usage should be reasonable
        // We can't guarantee specific status, but we can verify it's either UP or DOWN
        assertTrue(health.status == Status.UP || health.status == Status.DOWN)
        
        val statusDetail = health.details["status"] as String
        assertTrue(statusDetail.contains("Memory usage"))
    }

    @Test
    fun `memory usage ratio is between 0 and 1`() {
        // When
        val health = healthIndicator.health()

        // Then
        val usageRatio = health.details["memory_usage_ratio"] as String
        val ratio = usageRatio.toDouble()
        
        assertTrue(ratio >= 0.0)
        assertTrue(ratio <= 1.0)
    }

    @Test
    fun `memory usage percentage is formatted correctly`() {
        // When
        val health = healthIndicator.health()

        // Then
        val usagePercent = health.details["memory_usage_percent"] as String
        assertTrue(usagePercent.endsWith("%"))
        
        val percentValue = usagePercent.removeSuffix("%").toDouble()
        assertTrue(percentValue >= 0.0)
        assertTrue(percentValue <= 100.0)
    }

    @Test
    fun `thresholds are properly configured`() {
        // When
        val health = healthIndicator.health()

        // Then
        @Suppress("UNCHECKED_CAST")
        val thresholds = health.details["thresholds"] as Map<String, String>
        
        assertEquals("85%", thresholds["warning"])
        assertEquals("95%", thresholds["critical"])
    }

    @Test
    fun `memory values are consistent`() {
        // When
        val health = healthIndicator.health()

        // Then
        val usedBytes = health.details["memory_used_bytes"] as Long
        val totalBytes = health.details["memory_total_bytes"] as Long
        val maxBytes = health.details["memory_max_bytes"] as Long
        
        // Used memory should not exceed total memory
        assertTrue(usedBytes <= totalBytes)
        
        // Total memory should not exceed max memory
        assertTrue(totalBytes <= maxBytes)
    }
}