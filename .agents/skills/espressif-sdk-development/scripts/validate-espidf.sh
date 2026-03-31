#!/bin/bash

# ESP-IDF skill validation script
echo "Validating ESP-IDF development setup..."

# Check if ESP-IDF environment is set up
if [ -z "$IDF_PATH" ]; then
    echo "WARNING: IDF_PATH not set - ESP-IDF environment may not be configured"
    echo "Run '. ./export.sh' from ESP-IDF directory or use 'get_idf' alias"
fi

# Check for idf.py command
if ! command -v idf.py &> /dev/null; then
    echo "WARNING: idf.py not found - ESP-IDF may not be in PATH"
    echo "Try running 'get_idf' or sourcing export.sh"
fi

# Check for get_idf alias (common in .zshrc)
if alias get_idf &> /dev/null; then
    echo "INFO: get_idf alias found - can be used for ESP-IDF initialization"
fi

# Check for required tools
if ! command -v cmake &> /dev/null; then
    echo "ERROR: cmake not found"
    exit 1
fi

if ! command -v python3 &> /dev/null; then
    echo "ERROR: python3 not found"
    exit 1
fi

# Check for serial ports (Linux/Mac)
if [[ "$OSTYPE" == "linux-gnu"* ]] || [[ "$OSTYPE" == "darwin"* ]]; then
    if ! ls /dev/tty* 2>/dev/null | head -1 > /dev/null; then
        echo "WARNING: No serial ports found"
    fi
fi

# Validate project structure if in project directory
if [ -f "CMakeLists.txt" ]; then
    echo "Validating ESP-IDF project structure..."
    
    # Check for main directory
    if [ ! -d "main" ]; then
        echo "ERROR: Missing main directory"
        exit 1
    fi
    
    # Check for main CMakeLists.txt
    if [ ! -f "main/CMakeLists.txt" ]; then
        echo "ERROR: Missing main/CMakeLists.txt"
        exit 1
    fi
    
    # Check for main source file
    if [ ! -f "main/main.c" ] && [ ! -f "main/main.cpp" ]; then
        echo "WARNING: No main source file found (main.c or main.cpp)"
    fi
    
    echo "Project structure validation passed!"
fi

echo "ESP-IDF validation completed!"
