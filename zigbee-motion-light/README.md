# Zigbee Motion Light

Motion sensor with LED controller - currently implemented as basic GPIO control.

## Features

- **Motion Detection**: PIR motion sensor on GPIO 4 with interrupt-driven detection
- **LED Control**: Simple LED on GPIO 5 (on/off control)
- **Auto Control**: LED turns on for 10 seconds when motion detected
- **Build Ready**: Successfully compiles with ESP-IDF

## Hardware Connections

```
Motion Sensor (PIR)    -> GPIO 4
LED (simple)           -> GPIO 5
Power                  -> 3.3V/5V
Ground                 -> GND
```

## Build Instructions

1. Setup ESP-IDF environment:
   ```bash
   source /Users/ilia-kurganskii-air/esp/esp-idf/export.sh
   ```

2. Build and flash:
   ```bash
   cd zigbee-motion-light
   idf.py build
   idf.py -p <PORT> flash monitor
   ```

## Operation

- Motion detected → LED turns on for 10 seconds
- Interrupt-driven motion detection for immediate response
- Basic GPIO implementation ready for Zigbee integration

## Next Steps

This basic version builds successfully. To add Zigbee functionality:

1. Add Zigbee component dependencies back to `idf_component.yml`
2. Restore Zigbee includes and cluster configuration
3. Configure Zigbee settings in `sdkconfig.defaults`
4. Implement Zigbee occupancy and on/off clusters

## Configuration

Edit `sdkconfig.defaults` for custom settings or use `idf.py menuconfig`.
