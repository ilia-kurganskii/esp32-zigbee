# ESP-IDF Commands Reference

## Essential Commands
```bash
# Environment setup
. $IDF_PATH/export.sh
# OR if you have the alias configured:
get_idf

# Project configuration
idf.py menuconfig

# Build commands
idf.py build          # Build project
idf.py clean          # Clean build files
idf.py fullclean      # Clean everything including sdkconfig
idf.py -v build       # Verbose build

# Flash commands
idf.py flash          # Flash firmware
idf.py flash monitor  # Flash and monitor
idf.py -p PORT flash  # Flash with specific port

# Monitor commands
idf.py monitor        # Start serial monitor
idf.py -p PORT monitor -b BAUD monitor  # With port/baud

# Build system
idf.py build bootloader    # Build bootloader only
idf.py build app           # Build app only
idf.py partition-table     # Show partition table
```

## Common Aliases
```bash
# Add to ~/.zshrc for convenience
alias get_idf='. $HOME/esp/esp-idf/export.sh'

# Then use simply:
get_idf
```

## Common ESP32 Boards
- ESP32 DevKit
- ESP32-C6 DevKit
- ESP32-S3 DevKit
- ESP32-C3 DevKit

## Pin Definitions
```c
// Common GPIO pins
#define LED_PIN GPIO_NUM_2
#define BUTTON_PIN GPIO_NUM_0
#define I2C_SDA GPIO_NUM_21
#define I2C_SCL GPIO_NUM_22
```

## Error Codes
- `ESP_OK` - Success
- `ESP_ERR_*` - Various error conditions
- Always check return values with `ESP_ERROR_CHECK()`
