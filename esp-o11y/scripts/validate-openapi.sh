#!/bin/bash

# Validation script for OpenAPI documentation
# Usage: ./scripts/validate-openapi.sh

set -e

echo "🔍 Validating OpenAPI documentation..."

# Configuration
APP_PORT=${APP_PORT:-8081}
APP_URL="http://localhost:$APP_PORT"

# Check if application is running
echo "📡 Checking application health..."
if ! curl -s -f "$APP_URL/actuator/health" > /dev/null 2>&1; then
    echo "❌ ERROR: Application not running on port $APP_PORT"
    echo "💡 Start the application with: ./gradlew bootRun"
    exit 1
fi

echo "✅ Application is running"

# Validate OpenAPI spec JSON format
echo "📋 Validating OpenAPI JSON format..."
if ! curl -s -f "$APP_URL/api-docs" | jq . > /dev/null 2>&1; then
    echo "❌ ERROR: Invalid OpenAPI JSON format"
    exit 1
fi

echo "✅ OpenAPI JSON is valid"

# Check for required endpoints
echo "🔗 Checking required endpoints..."
REQUIRED_ENDPOINTS=("/api/v1/telemetry" "/actuator/health" "/actuator/prometheus")
MISSING_ENDPOINTS=()

for endpoint in "${REQUIRED_ENDPOINTS[@]}"; do
    if ! curl -s "$APP_URL/api-docs" | jq -r ".paths | keys[]" 2>/dev/null | grep -q "^$endpoint$"; then
        MISSING_ENDPOINTS+=("$endpoint")
    fi
done

if [ ${#MISSING_ENDPOINTS[@]} -gt 0 ]; then
    echo "⚠️  WARNING: Missing endpoint documentation:"
    for endpoint in "${MISSING_ENDPOINTS[@]}"; do
        echo "   - $endpoint"
    done
else
    echo "✅ All required endpoints documented"
fi

# Check for authentication setup
echo "🔐 Checking authentication documentation..."
if curl -s "$APP_URL/api-docs" | jq -r '.components.securitySchemes | keys[]' 2>/dev/null | grep -q "ApiKeyAuth"; then
    echo "✅ API key authentication documented"
else
    echo "⚠️  WARNING: API key authentication not documented"
fi

# Check for request/response examples
echo "📝 Checking examples in documentation..."
EXAMPLE_COUNT=$(curl -s "$APP_URL/api-docs" | jq -r '.paths | to_entries[] | .value | to_entries[] | select(.key | contains("post") or contains("get")) | .value.responses | to_entries[] | select(.value.content) | .value.content | to_entries[] | select(.value.examples) | length' 2>/dev/null | awk '{sum+=$1} END {print sum+0}')

if [ "$EXAMPLE_COUNT" -gt 0 ]; then
    echo "✅ Found $EXAMPLE_COUNT request/response examples"
else
    echo "⚠️  WARNING: No request/response examples found"
fi

# Validate schema completeness
echo "🏗️  Checking schema definitions..."
SCHEMA_COUNT=$(curl -s "$APP_URL/api-docs" | jq -r '.components.schemas | keys | length' 2>/dev/null || echo "0")

if [ "$SCHEMA_COUNT" -gt 0 ]; then
    echo "✅ Found $SCHEMA_COUNT schema definitions"
else
    echo "⚠️  WARNING: No schema definitions found"
fi

# Check documentation metadata
echo "📊 Checking documentation metadata..."
TITLE=$(curl -s "$APP_URL/api-docs" | jq -r '.info.title // "Untitled"' 2>/dev/null)
VERSION=$(curl -s "$APP_URL/api-docs" | jq -r '.info.version // "unknown"' 2>/dev/null)

echo "📋 API Title: $TITLE"
echo "📋 Version: $VERSION"

if [ "$TITLE" = "Untitled" ] || [ "$VERSION" = "unknown" ]; then
    echo "⚠️  WARNING: Missing API metadata"
else
    echo "✅ API metadata complete"
fi

# Generate validation report
echo ""
echo "📈 Validation Summary:"
echo "   Application Status: ✅ Running"
echo "   OpenAPI Format: ✅ Valid JSON"
echo "   Required Endpoints: $(${#MISSING_ENDPOINTS[@]} missing)"
echo "   Authentication: $(if curl -s "$APP_URL/api-docs" | jq -e '.components.securitySchemes.ApiKeyAuth' > /dev/null 2>&1; then echo "✅ Configured"; else echo "⚠️ Missing"; fi)"
echo "   Examples: $EXAMPLE_COUNT found"
echo "   Schemas: $SCHEMA_COUNT defined"

# Exit with warning if there are issues
if [ ${#MISSING_ENDPOINTS[@]} -gt 0 ] || [ "$EXAMPLE_COUNT" -eq 0 ] || [ "$SCHEMA_COUNT" -eq 0 ]; then
    echo ""
    echo "⚠️  Validation completed with warnings"
    exit 1
else
    echo ""
    echo "🎉 Validation completed successfully!"
    exit 0
fi
