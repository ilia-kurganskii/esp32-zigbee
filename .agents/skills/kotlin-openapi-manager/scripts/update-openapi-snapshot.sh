#!/bin/bash

# OpenAPI snapshot update script for Kotlin Spring Boot applications
# Usage: ./scripts/update-openapi-snapshot.sh [port]

set -e

# Configuration
APP_PORT=${1:-8081}
APP_URL="http://localhost:$APP_PORT"
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
SNAPSHOT_FILE="$PROJECT_ROOT/docs/openapi-snapshot.json"

echo "🔄 OpenAPI Snapshot Update"
echo "=========================="

# Change to project directory
cd "$PROJECT_ROOT"

# Check if project has OpenAPI setup
if [ ! -f "build.gradle.kts" ] || ! grep -q "springdoc" build.gradle.kts; then
    echo "❌ ERROR: This project doesn't appear to have SpringDoc OpenAPI configured"
    echo "💡 Add to build.gradle.kts:"
    echo "   implementation('org.springdoc:springdoc-openapi-starter-webmvc-ui:2.2.0')"
    exit 1
fi

# Function to start application
start_application() {
    echo "🚀 Starting application..."
    
    # Set Java home if needed
    if [ -z "$JAVA_HOME" ]; then
        export JAVA_HOME=/opt/homebrew/Cellar/openjdk@21/21.0.10/libexec/openjdk.jdk/Contents/Home
    fi
    
    # Start application in background
    ./gradlew bootRun > /dev/null 2>&1 &
    APP_PID=$!
    
    echo "⏳ Waiting for application to start..."
    
    # Wait for application to be ready
    for i in {1..60}; do
        if curl -s -f "$APP_URL/actuator/health" > /dev/null 2>&1; then
            echo "✅ Application is ready (took ${i}s)"
            return 0
        fi
        
        if [ $i -eq 60 ]; then
            echo "❌ ERROR: Application failed to start within 60 seconds"
            kill $APP_PID 2>/dev/null || true
            exit 1
        fi
        
        sleep 1
    done
}

# Function to stop application
stop_application() {
    if [ ! -z "$APP_PID" ]; then
        echo "🛑 Stopping application..."
        kill $APP_PID 2>/dev/null || true
        wait $APP_PID 2>/dev/null || true
    fi
}

# Check if application is already running
if curl -s -f "$APP_URL/actuator/health" > /dev/null 2>&1; then
    echo "✅ Application is already running on port $APP_PORT"
    APP_STARTED=false
else
    start_application
    APP_STARTED=true
fi

# Create docs directory if it doesn't exist
mkdir -p "$(dirname "$SNAPSHOT_FILE")"

# Fetch OpenAPI documentation
echo "📥 Fetching OpenAPI documentation from $APP_URL/api-docs..."

HTTP_STATUS=$(curl -s -w "%{http_code}" -o "$SNAPSHOT_FILE" "$APP_URL/api-docs")

if [ "$HTTP_STATUS" = "200" ]; then
    echo "✅ OpenAPI documentation fetched successfully"
else
    echo "❌ ERROR: Failed to fetch OpenAPI documentation (HTTP $HTTP_STATUS)"
    if [ "$APP_STARTED" = true ]; then
        stop_application
    fi
    exit 1
fi

# Validate the fetched JSON
if ! jq . "$SNAPSHOT_FILE" > /dev/null 2>&1; then
    echo "❌ ERROR: Invalid JSON in OpenAPI documentation"
    if [ "$APP_STARTED" = true ]; then
        stop_application
    fi
    exit 1
fi

# Generate statistics
FILE_SIZE=$(wc -c < "$SNAPSHOT_FILE")
ENDPOINT_COUNT=$(jq -r '.paths | keys | length' "$SNAPSHOT_FILE" 2>/dev/null || echo "0")
SCHEMA_COUNT=$(jq -r '.components.schemas | keys | length' "$SNAPSHOT_FILE" 2>/dev/null || echo "0")
API_TITLE=$(jq -r '.info.title // "Untitled"' "$SNAPSHOT_FILE" 2>/dev/null)
API_VERSION=$(jq -r '.info.version // "unknown"' "$SNAPSHOT_FILE" 2>/dev/null)

# Display results
echo ""
echo "📊 Snapshot Statistics:"
echo "   File: $SNAPSHOT_FILE"
echo "   Size: $FILE_SIZE bytes"
echo "   API Title: $API_TITLE"
echo "   Version: $API_VERSION"
echo "   Endpoints: $ENDPOINT_COUNT"
echo "   Schemas: $SCHEMA_COUNT"
echo "   Updated: $(date)"

# Run validation if available
if [ -f "scripts/validate-openapi.sh" ]; then
    echo ""
    echo "🔍 Running validation..."
    if ./scripts/validate-openapi.sh; then
        echo "✅ Validation passed"
    else
        echo "⚠️  Validation completed with warnings"
    fi
fi

# Stop application if we started it
if [ "$APP_STARTED" = true ]; then
    stop_application
fi

echo ""
echo "🎉 OpenAPI snapshot update completed!"
echo ""
echo "📚 Access your documentation:"
echo "   Live API Docs: $APP_URL/swagger-ui.html"
echo "   OpenAPI JSON: $APP_URL/api-docs"
echo "   Snapshot File: $SNAPSHOT_FILE"
echo ""
echo "💡 To update again, run: ./scripts/update-openapi-snapshot.sh"
