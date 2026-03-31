#!/bin/bash

# Script to update OpenAPI snapshot
# Usage: ./scripts/update-openapi.sh

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

cd "$PROJECT_ROOT"

echo "🔄 Updating OpenAPI snapshot..."

# Check if application is running on port 8081
if ! curl -s -f http://localhost:8081/actuator/health > /dev/null 2>&1; then
    echo "❌ Application is not running on port 8081"
    echo "🚀 Starting application..."
    
    # Start application in background
    export JAVA_HOME=/opt/homebrew/Cellar/openjdk@21/21.0.10/libexec/openjdk.jdk/Contents/Home
    ./gradlew bootRun > /dev/null 2>&1 &
    APP_PID=$!
    
    echo "⏳ Waiting for application to start..."
    for i in {1..30}; do
        if curl -s -f http://localhost:8081/actuator/health > /dev/null 2>&1; then
            echo "✅ Application is ready"
            break
        fi
        if [ $i -eq 30 ]; then
            echo "❌ Application failed to start within 30 seconds"
            kill $APP_PID 2>/dev/null || true
            exit 1
        fi
        sleep 1
    done
    
    # Give it a bit more time to fully initialize
    sleep 5
fi

# Create docs directory if it doesn't exist
mkdir -p docs

# Fetch OpenAPI documentation
echo "📥 Fetching OpenAPI documentation..."
if curl -s -f http://localhost:8081/api-docs -o docs/openapi-snapshot.json; then
    echo "✅ OpenAPI snapshot updated successfully"
    echo "📍 Saved to: docs/openapi-snapshot.json"
    
    # Show some stats
    echo ""
    echo "📊 Snapshot stats:"
    echo "   File size: $(wc -c < docs/openapi-snapshot.json) bytes"
    echo "   Endpoints: $(grep -o '"post":\|"get":' docs/openapi-snapshot.json | wc -l | tr -d ' ')"
    echo "   Last updated: $(date)"
    
    # If we started the app, stop it
    if [ ! -z "$APP_PID" ]; then
        echo ""
        echo "🛑 Stopping application..."
        kill $APP_PID 2>/dev/null || true
        wait $APP_PID 2>/dev/null || true
    fi
else
    echo "❌ Failed to fetch OpenAPI documentation"
    if [ ! -z "$APP_PID" ]; then
        kill $APP_PID 2>/dev/null || true
    fi
    exit 1
fi

echo ""
echo "🎉 OpenAPI snapshot update completed!"
