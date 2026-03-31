# Multi-stage build for better optimization
FROM openjdk:21-jdk-slim AS builder

LABEL maintainer="esp32-telemetry-service"
LABEL description="ESP32 Telemetry Collection Service - Build Stage"

# Install curl for health check in runtime stage
RUN apt-get update && apt-get install -y curl && rm -rf /var/lib/apt/lists/*

# Create build directory
WORKDIR /build

# Copy gradle wrapper and build files first (for better layer caching)
COPY gradlew gradle/wrapper/ build.gradle.kts settings.gradle.kts gradle.properties ./

# Make gradlew executable
RUN chmod +x gradlew

# Download dependencies (cached layer)
RUN ./gradlew dependencies --no-daemon

# Copy source code
COPY src ./src

# Build the application
RUN ./gradlew build -x test --no-daemon

# Runtime stage
FROM openjdk:21-jre-slim

LABEL maintainer="esp32-telemetry-service"
LABEL description="ESP32 Telemetry Collection Service - Runtime"

# Install curl for health check
RUN apt-get update && apt-get install -y curl && rm -rf /var/lib/apt/lists/*

# Create non-root user for security
RUN groupadd -r telemetry && useradd -r -g telemetry telemetry

# Create app directory
WORKDIR /app

# Copy built jar from builder stage
COPY --from=builder /build/build/libs/*.jar app.jar

# Change ownership to non-root user
RUN chown -R telemetry:telemetry /app

# Switch to non-root user
USER telemetry

# Expose ports
EXPOSE 8080/tcp
EXPOSE 8080/udp

# Health check
HEALTHCHECK --interval=30s --timeout=10s --start-period=60s --retries=3 \
  CMD curl -f http://localhost:8080/actuator/health || exit 1

# Run the application with optimized JVM settings
ENTRYPOINT ["java", "-XX:+UseContainerSupport", "-XX:MaxRAMPercentage=75.0", "-jar", "app.jar"]
