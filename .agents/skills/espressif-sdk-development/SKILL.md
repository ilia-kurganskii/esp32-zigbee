---
name: espressif-sdk-development
description: ESP32 development workflow for building projects using Espressif's official ESP-IDF framework
author: Cascade AI Assistant
version: 1.0.0
license: MIT
tags:
  - esp32
  - esp-idf
  - embedded
  - firmware
  - build
  - flash
  - zigbee
  - iot
category: development
framework: esp-idf
target_platforms:
  - esp32
  - esp32-c3
  - esp32-c6
  - esp32-s2
  - esp32-s3
ide_support:
  - windsurf
  - vscode
  - platformio
difficulty: intermediate
prerequisites:
  - esp-idf environment
  - esp32 development board
  - usb connection
  - serial permissions
---

# Espressif SDK Development Skill

This skill provides systematic guidance for building ESP32 projects using Espressif's official ESP-IDF framework. Use this for building firmware, debugging, and deploying ESP32 applications.

## Windsurf Integration

This skill is optimized for Windsurf IDE with the following features:
- **Terminal Integration**: Works seamlessly with Windsurf's integrated terminal
- **File Navigation**: Provides paths for Windsurf's file explorer
- **Build Monitoring**: Real-time build progress and error reporting
- **Configuration Management**: Menuconfig guidance for Windsurf users

### Windsurf-Specific Commands

In Windsurf, use the integrated terminal (Ctrl+` or View → Terminal) for all ESP-IDF commands:

```bash
# Navigate to project (Windsurf will auto-detect)
cd zigbee-co2

# Setup environment
get_idf

# Build with Windsurf terminal output
idf.py build
```

### Windsurf File Structure Support

The skill references standard ESP-IDF project structure that Windsurf recognizes:
- `main/` - Main application code
- `components/` - Custom components
- `CMakeLists.txt` - Build configuration
- `sdkconfig` - Project configuration
- `idf_component.yml` - Component dependencies

Windsurf will automatically provide syntax highlighting and IntelliSense for these files.

## When to Use This Skill
- Building ESP-IDF projects
- Flashing firmware to ESP32 devices
- Debugging ESP32 applications
- Configuring ESP32 peripherals and protocols
- Managing ESP-IDF components and dependencies

## Prerequisites
- ESP-IDF environment installed and `get_idf` alias available
- ESP32 development board connected via USB
- Serial port permissions for flashing
- Project already created and configured

## ESP32 Build Workflow

### Step 1: Environment Setup
```bash
# Navigate to project directory
cd /path/to/esp32-project

# Activate ESP-IDF environment
get_idf

# Verify ESP-IDF is working
idf.py --version
```

### Step 2: Target Configuration
```bash
# Set target chip (example for ESP32-C6)
idf.py --preview set-target esp32c6

# Verify target is set
grep CONFIG_IDF_TARGET sdkconfig
```

### Step 3: Project Configuration
```bash
# Open configuration menu
idf.py menuconfig

# Common configurations needed:
# - Serial flasher config → Flash size (change to 4MB if partition table is large)
# - Component config → Enable/disable components
# - Wi-Fi settings (if needed)
# - Logging levels
```

### Step 4: Build Firmware
```bash
# Build the project
idf.py build

# Build with verbose output (useful for debugging)
idf.py -v build

# Clean build if needed
idf.py clean
```

### Step 5: Flash Device
```bash
# Flash firmware to ESP32
idf.py flash

# Flash with specific port
idf.py -p /dev/ttyUSB0 flash

# Flash and monitor in one command
idf.py flash monitor
```

### Step 6: Monitor Serial Output
```bash
# Monitor serial logs
idf.py monitor

# Monitor with specific port and baud rate
idf.py -p /dev/ttyUSB0 -b 115200 monitor

# Exit monitor with Ctrl+]
```

## Common Build Issues and Solutions

### Issue 1: Flash Size Configuration
**Symptoms**: "Partition table occupies X.XMB of flash which does not fit in configured flash size YMB"
**Solution**:
```bash
idf.py menuconfig
# Navigate to: Serial flasher config → Flash size
# Change from 2MB to 4MB (or larger as needed)
# Save and exit
```

### Issue 2: Component Dependencies
**Symptoms**: "Failed to resolve component 'component_name' required by component"
**Solution**:
```bash
# Add missing dependency using component manager
idf.py add-dependency "component_name"

# Or check CMakeLists.txt for correct component names
```

### Issue 3: Target Mismatch
**Symptoms**: Build errors related to wrong architecture (xtensa vs riscv)
**Solution**:
```bash
# Set correct target for your chip
idf.py --preview set-target esp32c6  # For ESP32-C6
idf.py --preview set-target esp32    # For ESP32
idf.py --preview set-target esp32s3  # For ESP32-S3
```

### Issue 4: Flash Failed
**Symptoms**: "Failed to connect to target" error
**Solutions**:
- Check USB cable connection
- Verify serial port permissions
- Put ESP32 in bootloader mode (hold BOOT button, press RESET)
- Check baud rate settings

### Issue 5: Build Errors
**Symptoms**: Compilation errors during build
**Solutions**:
- Ensure ESP-IDF environment is sourced
- Check CMakeLists.txt syntax
- Verify component dependencies
- Clean build with `idf.py fullclean`

## Build Validation

### Environment Validation
```bash
# Verify ESP-IDF installation
idf.py --version

# Check toolchain
xtensa-esp32-elf-gcc --version  # or riscv32-esp-elf-gcc for ESP32-C6

# Test serial port
ls /dev/ttyUSB*
```

## Quick Build Commands Reference

```bash
# Complete build workflow
get_idf && idf.py menuconfig && idf.py build && idf.py flash monitor

# Build only
idf.py build

# Flash only
idf.py flash

# Monitor only
idf.py monitor

# Clean build
idf.py clean

# Full clean (remove all build artifacts)
idf.py fullclean

# Set target
idf.py --preview set-target esp32c6
```

## Component Management

### Adding Dependencies
```bash
# Add component from IDF Component Manager
idf.py add-dependency "component_name"

# Check current dependencies
cat idf_component.yml
```

### Component Structure
```
project/
├── main/
│   ├── CMakeLists.txt
│   └── main.c
├── components/
│   └── my_component/
│       ├── CMakeLists.txt
│       ├── my_component.h
│       └── my_component.c
├── CMakeLists.txt
├── sdkconfig.defaults
└── idf_component.yml
```

This skill provides a complete, validated workflow for building ESP32 projects using the official Espressif SDK.
