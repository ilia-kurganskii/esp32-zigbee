---
name: kotlin-openapi-manager
description: Manage OpenAPI documentation for Kotlin Spring Boot applications with automatic snapshot generation and updates
---

# Kotlin OpenAPI Manager

This skill handles OpenAPI documentation management for Kotlin Spring Boot projects, including automatic snapshot generation, validation, and updates when controllers change.

## When to Use This Skill

- Setting up OpenAPI documentation for new Kotlin Spring Boot projects
- Adding OpenAPI annotations to existing controllers
- Updating API documentation after controller changes
- Validating OpenAPI specification completeness
- Generating API documentation snapshots

## Prerequisites

- Kotlin Spring Boot project with SpringDoc OpenAPI dependency
- Application running on configurable port (default: 8081)
- Gradle build system
- Git repository (optional, for version control)

## Execution Steps

1. **Analyze Project Structure** - Identify Kotlin controllers and OpenAPI configuration
2. **Setup OpenAPI Dependencies** - Ensure SpringDoc OpenAPI is properly configured
3. **Annotate Controllers** - Add comprehensive OpenAPI annotations to REST controllers
4. **Configure Documentation** - Setup OpenAPI configuration class and application properties
5. **Generate Snapshot** - Start application and generate OpenAPI JSON snapshot
6. **Create Update Scripts** - Setup automated update mechanisms
7. **Validate Documentation** - Verify OpenAPI specification completeness and accuracy

## Output Format

# OpenAPI Documentation Setup

## Configuration Summary
- SpringDoc version and configuration
- Server endpoints and ports
- Authentication setup
- Documentation paths

## Controllers Annotated
- List of controllers with OpenAPI annotations
- Endpoint counts and coverage
- Authentication requirements

## Generated Artifacts
- OpenAPI snapshot location and size
- Update scripts created
- Documentation links

## Validation Results
- OpenAPI specification validation
- Endpoint coverage analysis
- Example completeness check

## Usage Instructions
- How to access live documentation
- How to update snapshots
- Integration with development workflow

## Examples

### Example 1: New Project Setup
**Input**: "Setup OpenAPI for new Kotlin Spring Boot telemetry service"
**Output**: 
```kotlin
// OpenApiConfig.kt
@Configuration
class OpenApiConfig {
    @Bean
    fun customOpenAPI(): OpenAPI {
        return OpenAPI()
            .info(Info()
                .title("ESP32 Telemetry Service")
                .version("1.0.0")
            )
            .addSecurityItem(SecurityRequirement().addList("ApiKeyAuth"))
    }
}

// Controller annotations
@Operation(summary = "Submit telemetry data")
@PostMapping("/telemetry")
fun receiveTelemetry(@Valid @RequestBody request: TelemetryBatchRequest): ResponseEntity<TelemetryResponse>
```

### Example 2: Update Existing Documentation
**Input**: "Update OpenAPI docs after adding new health check endpoint"
**Output**: Updated snapshot file and validation report showing new endpoint coverage

## Validation Scripts

### scripts/validate-openapi.sh
```bash
#!/bin/bash
echo "Validating OpenAPI documentation..."

# Check if application is running
if ! curl -s -f http://localhost:8081/actuator/health > /dev/null; then
    echo "ERROR: Application not running on port 8081"
    exit 1
fi

# Validate OpenAPI spec
if ! curl -s -f http://localhost:8081/api-docs | jq . > /dev/null; then
    echo "ERROR: Invalid OpenAPI JSON"
    exit 1
fi

# Check required endpoints
REQUIRED_ENDPOINTS=("/api/v1/telemetry" "/actuator/health" "/actuator/prometheus")
for endpoint in "${REQUIRED_ENDPOINTS[@]}"; do
    if ! curl -s "http://localhost:8081/api-docs" | jq -r ".paths | keys[]" | grep -q "$endpoint"; then
        echo "WARNING: Missing endpoint documentation: $endpoint"
    fi
done

echo "✅ OpenAPI validation passed"
```

### scripts/update-openapi-snapshot.sh
```bash
#!/bin/bash
set -e

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$PROJECT_ROOT"

echo "🔄 Updating OpenAPI snapshot..."

# Ensure application is running
if ! curl -s -f http://localhost:8081/actuator/health > /dev/null 2>&1; then
    echo "🚀 Starting application..."
    ./gradlew bootRun > /dev/null 2>&1 &
    APP_PID=$!
    
    # Wait for startup
    for i in {1..30}; do
        if curl -s -f http://localhost:8081/actuator/health > /dev/null 2>&1; then
            break
        fi
        sleep 1
    done
fi

# Create docs directory
mkdir -p docs

# Fetch OpenAPI documentation
curl -s -f http://localhost:8081/api-docs -o docs/openapi-snapshot.json

echo "✅ OpenAPI snapshot updated: docs/openapi-snapshot.json"
echo "📊 Size: $(wc -c < docs/openapi-snapshot.json) bytes"

# Stop app if we started it
if [ ! -z "$APP_PID" ]; then
    kill $APP_PID 2>/dev/null || true
fi
```

## Common Issues and Solutions

### Issue 1: Application Won't Start
**Problem**: Port conflicts or missing dependencies
**Solution**: 
```bash
# Check port usage
lsof -i :8081

# Change port in application.yml
server:
  port: 8082
```

### Issue 2: Missing OpenAPI Annotations
**Problem**: Controllers not appearing in documentation
**Solution**: Add proper annotations:
```kotlin
@Tag(name = "Telemetry")
@Operation(summary = "Endpoint description")
@ApiResponse(responseCode = "200", description = "Success")
```

### Issue 3: Authentication Not Documented
**Problem**: API key requirements not shown
**Solution**: Configure security scheme:
```kotlin
.addSecurityItem(SecurityRequirement().addList("ApiKeyAuth"))
.components(
    Components().addSecuritySchemes(
        "ApiKeyAuth",
        SecurityScheme()
            .type(SecurityScheme.Type.APIKEY)
            .in(SecurityScheme.In.HEADER)
            .name("X-API-Key")
    )
)
```

## Integration with Development Workflow

### Gradle Tasks
```kotlin
task<Exec>("updateOpenApi") {
    group = "documentation"
    description = "Update OpenAPI snapshot from running application"
    commandLine = listOf("curl", "-s", "-f", "http://localhost:8081/api-docs", "-o", "docs/openapi-snapshot.json")
}
```

### IDE Integration
- Add live template for OpenAPI annotations
- Configure code inspection for missing documentation
- Setup bookmark for Swagger UI: http://localhost:8081/swagger-ui.html

## Quality Checklist

Before completing OpenAPI setup:

- [ ] SpringDoc OpenAPI dependency added to build.gradle.kts
- [ ] OpenAPI configuration class created
- [ ] All REST controllers have @Tag annotations
- [ ] All endpoints have @Operation annotations
- [ ] Request/response schemas documented
- [ ] Authentication requirements specified
- [ ] Examples provided for complex requests
- [ ] Error responses documented
- [ ] OpenAPI snapshot generated and validated
- [ ] Update scripts created and tested
- [ ] Documentation links verified
- [ ] Integration with build system configured
