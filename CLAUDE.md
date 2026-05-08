# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This repository contains ESP32 Zigbee examples and implementations using the ESP-IDF framework and ESP-Zigbee SDK:

- **zigbee-light/**: HA-compliant dimmable light device with color control and NVS storage for persistent settings
- **zigbee-remote/**: HA-compliant coordinator/switch device for controlling lights
- **zigbee-wtw/**: Custom Zigbee button controller with 3-channel GPIO output control
- **deep_sleep/**: OpenThread sleepy end device example with power management

## Development Commands

### ESP-IDF in agents and fresh shells

Load the toolchain before **`idf.py`** (Cursor and scripted shells often omit it from `PATH`): run **`get_idf`** or **`$HOME/esp/esp-idf/export.sh`** (many setups alias `get_idf` to sourcing that script). After substantive firmware edits, **`idf.py build`** must be run from the relevant project directory; do not rely on claiming success without building.

Include **`freertos/FreeRTOS.h` before `freertos/task.h`** in C sources.

### Building and Flashing
```bash
# Load IDF environment first (fresh terminal / automation)
get_idf   # or: . $HOME/esp/esp-idf/export.sh

# Set target chip (run from project directory)
idf.py --preview set-target esp32c6  # or esp32h2

# Build project
idf.py build

# Erase flash (recommended before first flash)
idf.py -p PORT erase-flash

# Flash and monitor
idf.py -p PORT flash monitor

# Monitor only
idf.py -p PORT monitor
```

### Configuration
```bash
# Configure project settings
idf.py menuconfig
```

## Architecture Overview

### Core Components
- **ESP-Zigbee SDK**: Provides high-level Zigbee API and HA device profiles
- **ZBOSS Stack**: Low-level Zigbee protocol implementation (binary library)
- **Light Driver**: Hardware abstraction for LED control with RMT/LED strip support
- **NVS Storage**: Non-volatile storage for device settings and color states

### Device Types
- **End Device (ED)**: Battery-powered devices (zigbee-light)
- **Coordinator (ZC)**: Network controller devices (zigbee-remote)
- **Router**: Mains-powered relay devices

### Key Files Structure
- `main/`: Application entry point and device-specific logic
- `CMakeLists.txt`: Build configuration
- `sdkconfig*`: SDK configuration files
- `partitions.csv`: Flash memory partitioning
- `dependencies.lock`: Component version locks

### Message Handling Pattern
Projects use a common pattern with:
- `esp_zb_app_signal_handler()`: Zigbee stack events (network join/leave, commissioning)
- `esp_zb_message_handlers.c`: ZCL attribute and command processing
- `light_driver.c`: Hardware control abstraction

### NVS Integration
The zigbee-light project implements persistent storage for:
- Color temperature, hue, saturation values
- On/off state and brightness levels
- Restored automatically on device restart

## Supported Hardware
- ESP32-C6: Zigbee 3.0 + WiFi dual-mode
- ESP32-H2: Dedicated Zigbee/Thread radio
- Both support native Zigbee radio (no external coordinator needed)

## Configuration Notes
- Channel 11 is default Zigbee channel
- Use `idf.py menuconfig` to configure Zigbee network parameters
- Install code policy can be enabled for enhanced security
- Device aging timeout and keep-alive intervals are configurable per device type