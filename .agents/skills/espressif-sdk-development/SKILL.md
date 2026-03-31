---
name: espressif-sdk-development
description: Complete ESP32 development workflow using Espressif's official ESP-IDF framework with step-by-step guidance, validation, and best practices
---

# Espressif SDK Development Skill

This skill provides systematic guidance for ESP32 development using Espressif's official ESP-IDF framework. Use this for setting up projects, building firmware, debugging, and deploying ESP32 applications.

## When to Use This Skill
- Setting up new ESP32 development environment
- Creating ESP-IDF projects from scratch
- Building and flashing ESP32 firmware
- Debugging ESP32 applications
- Configuring ESP32 peripherals and protocols
- Managing ESP-IDF components and dependencies

## Prerequisites
- Python 3.8+ installed and accessible
- Git available for version control
- CMake 3.16+ for build system
- ESP32 development board connected via USB
- Serial port permissions for flashing

## Environment Setup Steps

1. **Install ESP-IDF** - Clone and set up the official framework
2. **Configure dependencies** - Install required tools and packages
3. **Set up environment** - Configure shell environment variables
4. **Verify installation** - Test ESP-IDF functionality

### Step 1: Install ESP-IDF
```bash
# Clone ESP-IDF repository
git clone --recursive https://github.com/espressif/esp-idf.git

# Navigate to ESP-IDF directory
cd esp-idf

# Install dependencies for ESP32 chips
./install.sh esp32,esp32c3,esp32s3,esp32c6
```

### Step 2: Configure Environment
```bash
# Set up environment variables (run this in new terminal sessions)
. ./export.sh

# OR use the get_idf alias if configured in .zshrc
get_idf

# Verify installation
idf.py --version
```

**Note**: Many users add an alias to their `.zshrc` for convenience:
```bash
# Add to ~/.zshrc
alias get_idf='. $HOME/esp/esp-idf/export.sh'

# Then simply run:
get_idf
```

### Step 3: VS Code Setup (Optional but Recommended)
1. Install ESP-IDF extension from Espressif in VS Code
2. Configure ESP-IDF path in extension settings
3. Use command palette: "ESP-IDF: Configure ESP-IDF extension"

## Project Creation Steps

1. **Create project structure** - Set up standard ESP-IDF layout
2. **Configure CMake files** - Define build configuration
3. **Set up main component** - Create application entry point
4. **Configure project settings** - Set up sdkconfig defaults

### Step 1: Create Project Structure
```bash
# Create new project directory
mkdir my-esp32-project
cd my-esp32-project

# Create standard directories
mkdir main components
```

### Step 2: Root CMakeLists.txt
Create `CMakeLists.txt` in project root:
```cmake
cmake_minimum_required(VERSION 3.16.0)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(my-esp32-project)
```

### Step 3: Main Component Setup
Create `main/CMakeLists.txt`:
```cmake
idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES freertos
)
```

Create `main/main.c`:
```c
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "main";

void app_main(void) {
    ESP_LOGI(TAG, "ESP32 application started!");
    
    while(1) {
        ESP_LOGI(TAG, "Running main loop...");
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
```

### Step 4: Default Configuration
Create `sdkconfig.defaults`:
```bash
# WiFi configuration
CONFIG_ESP32_WIFI_ENABLED=y
CONFIG_ESP32_WIFI_SW_COEXIST_ENABLED=y

# FreeRTOS configuration
CONFIG_FREERTOS_HZ=1000

# Logging configuration
CONFIG_LOG_DEFAULT_LEVEL_INFO=y
```

## Build and Flash Steps

1. **Verify environment** - Check ESP-IDF installation and target
2. **Configure project** - Use menuconfig for project settings
3. **Build firmware** - Compile the application
4. **Flash device** - Upload firmware to ESP32
5. **Monitor output** - View serial logs

### Step 1: Verify Environment
```bash
# Check ESP-IDF is installed and sourced
idf.py --version

# Set target (example for ESP32-C6)
idf.py --preview set-target esp32c6

# Verify target is set
grep CONFIG_IDF_TARGET sdkconfig
```

### Step 2: Project Configuration
```bash
# Open configuration menu
idf.py menuconfig

# Navigate through menus to configure:
# - Serial flasher config (set port and speed)
# - Component config (enable/disable components)
# - Wi-Fi settings (SSID, password, etc.)
# - Logging levels
```

### Step 3: Build Firmware
```bash
# Build the project
idf.py build

# Build with verbose output (useful for debugging)
idf.py -v build

# Clean build if needed
idf.py clean
```

### Step 4: Flash Device
```bash
# Flash firmware to ESP32
idf.py flash

# Flash with specific port
idf.py -p /dev/ttyUSB0 flash

# Flash and monitor in one command
idf.py flash monitor
```

### Step 5: Monitor Serial Output
```bash
# Monitor serial logs
idf.py monitor

# Monitor with specific port and baud rate
idf.py -p /dev/ttyUSB0 -b 115200 monitor

# Exit monitor with Ctrl+]
```

## Component Development Steps

1. **Create component directory** - Set up component structure
2. **Write component code** - Implement component functionality
3. **Configure component build** - Set up CMakeLists.txt
4. **Integrate component** - Use component in main application

### Step 1: Create Component
```bash
# Create component directory
mkdir components/my_component
cd components/my_component

# Create component files
touch CMakeLists.txt my_component.h my_component.c
```

### Step 2: Component Code
Create `my_component.h`:
```c
#ifndef MY_COMPONENT_H
#define MY_COMPONENT_H

#include "esp_err.h"

esp_err_t my_component_init(void);
void my_component_do_something(void);

#endif // MY_COMPONENT_H
```

Create `my_component.c`:
```c
#include "my_component.h"
#include "esp_log.h"

static const char *TAG = "my_component";

esp_err_t my_component_init(void) {
    ESP_LOGI(TAG, "Initializing component");
    return ESP_OK;
}

void my_component_do_something(void) {
    ESP_LOGI(TAG, "Component action performed");
}
```

### Step 3: Component Build Configuration
Create `components/my_component/CMakeLists.txt`:
```cmake
idf_component_register(
    SRCS "my_component.c"
    INCLUDE_DIRS "."
    REQUIRES driver esp_wifi
)
```

### Step 4: Use Component in Main
Update `main/main.c`:
```c
#include "my_component.h"

void app_main(void) {
    ESP_LOGI(TAG, "Application starting...");
    
    // Initialize component
    esp_err_t ret = my_component_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Component initialization failed");
        return;
    }
    
    // Use component
    my_component_do_something();
    
    ESP_LOGI(TAG, "Application started successfully");
}
```

## Output Format

# ESP32 Development Report

## Project Setup
- **Project Name**: [Project name]
- **ESP-IDF Version**: [Version]
- **Target Board**: [Board type]
- **Configuration**: [Key settings]

## Build Results
- **Build Status**: Success/Failed
- **Build Time**: [Duration]
- **Flash Size**: [Size]
- **Warnings**: [Count]

## Deployment Status
- **Flash Status**: Success/Failed
- **Serial Port**: [Port]
- **Monitor Active**: Yes/No

## Next Steps
- [ ] Test application functionality
- [ ] Verify peripheral operation
- [ ] Monitor for issues
- [ ] Deploy additional features

## Examples

### Example 1: Basic LED Blink
**Input**: "Create ESP32 project that blinks LED every second"
**Output**:
```c
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define LED_PIN GPIO_NUM_2

void led_task(void *pvParameters) {
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << LED_PIN);
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);
    
    while(1) {
        gpio_set_level(LED_PIN, 1);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        gpio_set_level(LED_PIN, 0);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

void app_main(void) {
    xTaskCreate(led_task, "led_task", 2048, NULL, 1, NULL);
}
```

### Example 3: Complex Zigbee Project Build
**Input**: "Build zigbee-co2 project with SCD40 sensor"
**Output**:
```bash
# Step 1: Set up environment
source ~/esp/esp-idf/export.sh
cd zigbee-co2

# Step 2: Verify ESP-IDF version
idf.py --version
# Output: ESP-IDF v5.5.1-dirty

# Step 3: Set target for ESP32-C6
idf.py --preview set-target esp32c6

# Step 4: Fix flash size issue (if partition table too large)
grep CONFIG_ESPTOOLPY_FLASHSIZE sdkconfig
# Edit sdkconfig: change 2MB to 4MB
# CONFIG_ESPTOOLPY_FLASHSIZE_4MB=y
# CONFIG_ESPTOOLPY_FLASHSIZE="4MB"

# Step 5: Build project
idf.py build
# Success: Build completed, firmware size 503KB

# Step 6: Flash to device
idf.py -p /dev/ttyUSB0 flash monitor
```

### Example 4: WiFi Station Setup
**Input**: "Setup ESP32 WiFi station connection"
**Output**:
```c
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"

#define WIFI_SSID "your_network"
#define WIFI_PASS "your_password"

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ESP_LOGI("wifi", "Got IP address");
    }
}

void wifi_init(void) {
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    esp_netif_create_default_wifi_sta();
    
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL);
    
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}
```

## Validation

### Environment Validation
```bash
# Verify ESP-IDF installation
idf.py --version

# Check toolchain
xtensa-esp32-elf-gcc --version

# Test serial port
ls /dev/ttyUSB*
```

### Build Validation
```bash
# Build should complete without errors
idf.py build

# Check output files
ls build/
# Should contain: firmware.bin, bootloader.bin, partition-table.bin
```

### Flash Validation
```bash
# Flash should complete successfully
idf.py flash

# Monitor should show application logs
idf.py monitor
# Should see: "ESP32 application started!"
```

## Common Issues and Solutions

### Issue 1: Flash Failed
**Symptoms**: "Failed to connect to target" error
**Solutions**:
- Check USB cable connection
- Verify serial port permissions
- Put ESP32 in bootloader mode (hold BOOT button, press RESET)
- Check baud rate settings

### Issue 2: Build Errors
**Symptoms**: Compilation errors during build
**Solutions**:
- Ensure ESP-IDF environment is sourced
- Check CMakeLists.txt syntax
- Verify component dependencies
- Clean build with `idf.py fullclean`

### Issue 3: Menuconfig Issues
**Symptoms**: Configuration menu not opening
**Solutions**:
- Install ncurses library: `sudo apt-get install libncurses5-dev`
- Delete sdkconfig and reconfigure
- Check terminal compatibility

### Issue 4: Flash Size Configuration
**Symptoms**: "Partition table occupies X.XMB of flash which does not fit in configured flash size YMB"
**Solutions**:
- Check current flash size in `sdkconfig`: `grep CONFIG_ESPTOOLPY_FLASHSIZE sdkconfig`
- Increase flash size by modifying `sdkconfig`:
  ```bash
  # Change from 2MB to 4MB
  # CONFIG_ESPTOOLPY_FLASHSIZE_2MB=y
  CONFIG_ESPTOOLPY_FLASHSIZE_4MB=y
  CONFIG_ESPTOOLPY_FLASHSIZE="4MB"
  ```
- Use menuconfig: `idf.py menuconfig` → Serial Flasher Config → Flash size
- Ensure partition table fits within available flash space
- For complex projects with OTA, recommend minimum 4MB flash

### Issue 5: Monitor No Output
**Symptoms**: No serial output in monitor
**Solutions**:
- Verify serial port and baud rate
- Check USB driver installation
- Ensure ESP32 is powered and running
- Try different terminal program

## PlatformIO Alternative

If using PlatformIO instead of native ESP-IDF:

### PlatformIO Configuration
Create `platformio.ini`:
```ini
[platformio]
src_dir = main
core_dir = ~/.platformio

[env:esp32dev]
platform = platformio/espressif32
framework = espidf
board = esp32dev

[env:esp32c6]
platform = platformio/espressif32
framework = espidf
board = esp32-c6-devkitm-1
```

### PlatformIO Commands
```bash
# Build project
pio run

# Build and upload
pio run -t upload

# Monitor serial
pio device monitor

# Configuration menu
pio run -t menuconfig

# Clean build
pio run -t clean
```

This skill provides a complete, validated workflow for ESP32 development using the official Espressif SDK.
