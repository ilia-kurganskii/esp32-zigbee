package com.telemetry

import org.springframework.boot.autoconfigure.SpringBootApplication
import org.springframework.boot.runApplication

@SpringBootApplication
class TelemetryServiceApplication

fun main(args: Array<String>) {
    runApplication<TelemetryServiceApplication>(*args)
}