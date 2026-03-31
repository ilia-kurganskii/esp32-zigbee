package com.telemetry

import org.springframework.boot.autoconfigure.SpringBootApplication
import org.springframework.boot.context.properties.EnableConfigurationProperties
import org.springframework.boot.runApplication

@SpringBootApplication
@EnableConfigurationProperties
class TelemetryServiceApplication

fun main(args: Array<String>) {
    runApplication<TelemetryServiceApplication>(*args)
}