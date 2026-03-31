import org.jetbrains.kotlin.gradle.tasks.KotlinCompile

plugins {
    id("org.springframework.boot") version "3.2.0"
    id("io.spring.dependency-management") version "1.1.4"
    kotlin("jvm") version "1.9.20"
    kotlin("plugin.spring") version "1.9.20"
    kotlin("plugin.jpa") version "1.9.20"
}

group = "com.telemetry"
version = "0.0.1-SNAPSHOT"

java {
    sourceCompatibility = JavaVersion.VERSION_21
}

repositories {
    mavenCentral()
}

dependencies {
    // Spring Boot starters
    implementation("org.springframework.boot:spring-boot-starter-web")
    implementation("org.springframework.boot:spring-boot-starter-actuator")
    implementation("org.springframework.boot:spring-boot-starter-validation")
    implementation("org.springframework.boot:spring-boot-starter-security")
    
    // Configuration processing
    implementation("org.springframework.boot:spring-boot-configuration-processor")
    
    // OpenAPI documentation
    implementation("org.springdoc:springdoc-openapi-starter-webmvc-ui:2.2.0")
    
    // Kotlin support
    implementation("com.fasterxml.jackson.module:jackson-module-kotlin")
    implementation("org.jetbrains.kotlin:kotlin-reflect")
    
    // Metrics and monitoring
    implementation("io.micrometer:micrometer-registry-prometheus")
    
    // Structured logging
    implementation("net.logstash.logback:logstash-logback-encoder:7.4")
    
    // Testing
    testImplementation("org.springframework.boot:spring-boot-starter-test")
    testImplementation("io.mockk:mockk:1.13.8")
    testImplementation("com.ninja-squad:springmockk:4.0.2")
    testImplementation("org.mockito.kotlin:mockito-kotlin:4.1.0")
}

tasks.withType<KotlinCompile> {
    kotlinOptions {
        freeCompilerArgs += "-Xjsr305=strict"
        jvmTarget = "21"
    }
}

tasks.withType<Test> {
    useJUnitPlatform()
}

// Simple task to update OpenAPI snapshot (assumes app is running)
task<Exec>("updateOpenApi") {
    group = "documentation"
    description = "Updates OpenAPI snapshot from running application on port 8081"
    
    doFirst {
        file("docs").mkdirs()
    }
    
    commandLine = listOf("curl", "-s", "-f", "http://localhost:8081/api-docs", "-o", "docs/openapi-snapshot.json")
    
    doLast {
        println("✅ OpenAPI snapshot updated: docs/openapi-snapshot.json")
        println("📊 File size: ${file("docs/openapi-snapshot.json").length()} bytes")
    }
}

// Task to update OpenAPI snapshot
task<Exec>("updateOpenApiSnapshot") {
    group = "documentation"
    description = "Updates the OpenAPI JSON snapshot from the running application"
    
    // Start the application in the background
    doFirst {
        println("Starting application to generate OpenAPI snapshot...")
    }
    
    // Wait for application to start and then fetch the OpenAPI docs
    commandLine = listOf("sh", "-c", """
        echo "Waiting for application to start..."
        sleep 5
        
        echo "Fetching OpenAPI documentation..."
        curl -s -f http://localhost:8081/api-docs -o docs/openapi-snapshot.json
        
        if [ \$? -eq 0 ]; then
            echo "OpenAPI snapshot updated successfully"
            echo "Snapshot saved to: docs/openapi-snapshot.json"
        else
            echo "Failed to fetch OpenAPI documentation"
            echo "Make sure the application is running on port 8081"
            exit 1
        fi
    """.trimIndent())
    
    // Ensure docs directory exists
    doFirst {
        file("docs").mkdirs()
    }
}

// Make updateOpenApiSnapshot depend on bootRun
task<JavaExec>("startAndUpdateOpenApi") {
    group = "documentation"
    description = "Starts the application, updates OpenAPI snapshot, then stops"
    
    classpath = sourceSets.main.get().runtimeClasspath
    mainClass.set("com.telemetry.TelemetryServiceApplicationKt")
    
    // Set system properties for the application
    systemProperty("server.port", "8081")
    
    doLast {
        println("Application started. Waiting 10 seconds before fetching OpenAPI...")
        Thread.sleep(10000)
        
        val docsDir = file("docs")
        if (!docsDir.exists()) {
            docsDir.mkdirs()
        }
        
        println("Fetching OpenAPI documentation...")
        val process = ProcessBuilder("curl", "-s", "-f", "http://localhost:8081/api-docs")
            .redirectOutput(file("docs/openapi-snapshot.json"))
            .start()
        
        val exitCode = process.waitFor()
        if (exitCode == 0) {
            println("OpenAPI snapshot updated successfully")
        } else {
            println("Failed to fetch OpenAPI documentation")
        }
    }
}