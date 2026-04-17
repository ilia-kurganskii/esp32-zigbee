# Zigbee Motion Light

Motion sensor with LED controller - currently implemented as basic GPIO control.

## Features

- **Motion Detection**: AM312 PIR motion sensor on GPIO 4 with interrupt-driven detection
- **LED Control**: Simple LED on GPIO 5 (on/off control)
- **Auto Control**: LED turns on for 10 seconds when motion detected
- **Build Ready**: Successfully compiles with ESP-IDF

## Hardware Connections

```
AM312 PIR Sensor:
- VCC   -> 3.3V (or 2.7-12V)
- GND   -> GND
- OUT   -> GPIO 4

LED:
- Anode  -> GPIO 5
- Cathode -> GND (with appropriate resistor)
```

## AM312 PIR Sensor Specifications

- **Operating Voltage**: 2.7V - 12V DC
- **Idle Current**: <0.1mA (100µA)
- **Output Delay**: 2 seconds
- **Blocking Time**: 2 seconds
- **Detection Range**: 3-5 meters
- **Field of View**: 100° cone angle
- **Trigger Mode**: Repeatable
- **Operating Temperature**: -20°C to +60°C

The AM312 is an ultra-miniature, low-power PIR sensor ideal for battery-powered applications. It provides a simple digital output (HIGH when motion detected, LOW when no motion).

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
