# OpenAPI Snapshot Documentation

This directory contains the automatically generated OpenAPI snapshot for the ESP32 Telemetry Service.

## Files

- `openapi-snapshot.json` - Latest OpenAPI specification snapshot
- `OPENAPI_SNAPSHOT.md` - This documentation file

## Automatic Updates

The OpenAPI snapshot is managed through the **Kotlin OpenAPI Manager** skill and can be updated using several methods:

### 1. Update Script (Recommended)
```bash
# Update snapshot (auto-detects if app is running)
./scripts/update-openapi-snapshot.sh

# Update with custom port
./scripts/update-openapi-snapshot.sh 8082
```

### 2. Validation Script
```bash
# Validate current OpenAPI documentation
./scripts/validate-openapi.sh
```

### 3. Gradle Tasks
```bash
# Quick update (app must be running)
./gradlew updateOpenApi

# Update with automatic app start
./gradlew updateOpenApiSnapshot
```

### 4. Manual Update
```bash
# Direct curl (app must be running)
curl -s http://localhost:8081/api-docs > docs/openapi-snapshot.json
```

## Skill Integration

This project uses the **kotlin-openapi-manager** skill which provides:

- **Automatic Detection**: Identifies SpringDoc OpenAPI configuration
- **Application Management**: Starts/stops application as needed
- **Validation**: Comprehensive OpenAPI specification validation
- **Statistics**: Endpoint counts, schema validation, example coverage
- **Error Handling**: Graceful failure recovery and helpful error messages

## Accessing Live Documentation

When the application is running, you can access:

- **Swagger UI**: http://localhost:8081/swagger-ui.html
- **OpenAPI JSON**: http://localhost:8081/api-docs
- **Health Checks**: http://localhost:8081/actuator/health
- **Prometheus Metrics**: http://localhost:8081/actuator/prometheus

## Configuration

The OpenAPI documentation is configured in:
- `src/main/kotlin/com/telemetry/config/OpenApiConfig.kt` - OpenAPI configuration
- `src/main/resources/application.yml` - SpringDoc settings

## Snapshot Contents

The snapshot includes:
- All REST endpoints with detailed descriptions
- Request/response schemas and examples
- Authentication requirements (API key)
- Health check endpoints
- Metrics endpoints
- Validation rules and constraints

## Validation Features

The validation script checks:

- ✅ Application health status
- ✅ OpenAPI JSON format validity
- ✅ Required endpoint documentation
- ✅ Authentication scheme documentation
- ✅ Request/response examples
- ✅ Schema definitions
- ✅ API metadata completeness

## Version Control

The OpenAPI snapshot is version controlled to:
- Track API changes over time
- Enable documentation review in pull requests
- Provide offline access to API specification
- Support automated API testing and validation

## Troubleshooting

### Application Not Running
```bash
# The update script will automatically start the application
./scripts/update-openapi-snapshot.sh
```

### Port Conflicts
```bash
# Use a different port
./scripts/update-openapi-snapshot.sh 8082

# Or update application.yml
server:
  port: 8082
```

### Missing SpringDoc Dependency
If you see "SpringDoc OpenAPI not configured", add to `build.gradle.kts`:
```kotlin
implementation("org.springdoc:springdoc-openapi-starter-webmvc-ui:2.2.0")
```

### Validation Failures
```bash
# Run validation to see detailed issues
./scripts/validate-openapi.sh

# Common fixes:
# - Add @Tag annotations to controllers
# - Add @Operation annotations to endpoints
# - Add @ApiResponse annotations for error cases
```

## Integration with Development Workflow

### IDE Integration
- Add bookmark for Swagger UI: http://localhost:8081/swagger-ui.html
- Configure code inspection for missing OpenAPI annotations
- Add live template for common annotations

### CI/CD Integration
```yaml
# Example GitHub Actions step
- name: Update OpenAPI Documentation
  run: |
    ./scripts/update-openapi-snapshot.sh
    git add docs/openapi-snapshot.json
```

### Code Review
The snapshot should be updated when:
- Adding new REST endpoints
- Modifying request/response schemas
- Changing authentication requirements
- Updating validation rules

## Skill Usage

The **kotlin-openapi-manager** skill can be invoked for:
- Setting up OpenAPI for new projects
- Adding comprehensive annotations
- Validating documentation completeness
- Generating automated updates

See `.agents/skills/kotlin-openapi-manager/SKILL.md` for complete skill documentation.
