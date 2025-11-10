# Zigbee CO2 Sensor

A Zigbee router device that measures and reports CO2 concentration, temperature, and humidity using an SCD40/SCD41 sensor.

## Features

- **SCD40/SCD41 Sensor Integration**: High-accuracy CO2 measurement with I2C interface
- **Multi-Sensor Reporting**: Measures CO2 (ppm), temperature (°C), and humidity (%)
- **Zigbee Router**: Acts as a Zigbee router to extend network range
- **Automatic Reporting**: Updates sensor values every 5 seconds
- **Home Assistant Compatible**: Uses standard Zigbee HA clusters

## Hardware Requirements

- **ESP32-C6 Development Board**
- **SCD40 or SCD41 CO2 Sensor Module**
- **Wiring**:
  - SCD40 SDA → ESP32-C6 GPIO 6
  - SCD40 SCL → ESP32-C6 GPIO 7
  - SCD40 VDD → 3.3V
  - SCD40 GND → GND

## Sensor Specifications
Serial number 177.93.141
### SCD40
- CO2 Range: 400-2000 ppm
- Accuracy: ±(40 ppm + 5%)
- Temperature Accuracy: ±0.8°C
- Humidity Accuracy: ±6% RH
- Measurement Interval: 5 seconds
- Interface: I2C (address 0x62)

### SCD41 (Enhanced Version)
- CO2 Range: 400-5000 ppm
- Accuracy: ±(40 ppm + 5%)
- Same temperature and humidity specs as SCD40

## Zigbee Clusters

The device exposes the following Zigbee clusters on endpoint 10:

| Cluster | ID | Purpose |
|---------|-----|---------|
| Basic | 0x0000 | Device information |
| Identify | 0x0003 | Device identification |
| Temperature Measurement | 0x0402 | Temperature in 0.01°C |
| Relative Humidity Measurement | 0x0405 | Humidity in 0.01% |
| Analog Input | 0x000C | CO2 concentration in ppm |

## Building and Flashing

### Set Target
```bash
cd zigbee-co2
idf.py --preview set-target esp32c6
```

### Build
```bash
idf.py build
```

### Flash (First Time - Erase Flash Recommended)
```bash
idf.py -p /dev/ttyUSB0 erase-flash
idf.py -p /dev/ttyUSB0 flash monitor
```

### Monitor Only
```bash
idf.py -p /dev/ttyUSB0 monitor
```

## Configuration

### Change I2C Pins
Edit `main/esp_zb_co2_sensor.h`:
```c
#define I2C_SDA_GPIO    6  /* Change to your SDA pin */
#define I2C_SCL_GPIO    7  /* Change to your SCL pin */
```

### Change Sensor Reading Interval
Edit `main/esp_zb_co2_sensor.h`:
```c
#define SENSOR_READ_INTERVAL_MS  5000  /* milliseconds */
```

Note: SCD40 has a minimum measurement period of 5 seconds.

### Zigbee Channel
Default channel is 11. To change, edit `main/esp_zb_co2_sensor.h`:
```c
#define ESP_ZB_PRIMARY_CHANNEL_MASK  (1 << 11)  /* Change 11 to desired channel */
```

## Usage

1. **Power on the device** - It will automatically start Zigbee commissioning
2. **Join to network** - Use your Zigbee coordinator to permit joining
3. **Monitor values** - Sensor readings are reported every 5 seconds

### Expected Log Output
```
I (1234) ESP_ZB_CO2_SENSOR: Initializing SCD40 sensor...
I (1245) SCD40: SCD40 driver initialized on I2C port 0 (SDA: GPIO6, SCL: GPIO7)
I (1256) ESP_ZB_CO2_SENSOR: SCD40 Serial: 1234-5678-9ABC
I (1267) SCD40: Starting periodic measurement
I (6278) ESP_ZB_CO2_SENSOR: Joined network successfully
I (6289) ESP_ZB_CO2_SENSOR: Sensor values - CO2: 456 ppm, Temp: 22.34°C, Humidity: 45.67%
```

## Troubleshooting

### I2C Communication Errors
- Check wiring connections
- Verify SCD40 power supply (3.3V)
- Ensure pull-up resistors on SDA/SCL (often built into sensor module)
- Try lower I2C speed (100kHz is default)

### Sensor Not Responding
- Power cycle the sensor
- Check sensor serial number reads successfully
- Verify I2C address is 0x62

### Zigbee Network Issues
- Ensure coordinator is in pairing mode
- Check Zigbee channel matches coordinator
- Verify device is within range
- Reset device with factory reset if needed

### Factory Reset
To perform a factory reset:
1. Press and hold the reset button
2. Or erase flash: `idf.py -p PORT erase-flash`

## Sensor Calibration

The SCD40 has automatic self-calibration (ASC) enabled by default. For best results:

- Expose sensor to fresh air (400 ppm) for at least 1 hour every 7 days
- Avoid enclosed spaces for long periods
- Consider manual calibration in controlled environments

### Set Altitude Compensation
If you're at significant altitude:
```c
scd40_set_altitude(1000); // 1000 meters above sea level
```

### Temperature Offset
If sensor is affected by self-heating:
```c
scd40_set_temperature_offset(2.5); // 2.5°C offset
```

## Integration Examples

### Home Assistant (via Zigbee2MQTT)
The device will appear as a multi-sensor with entities for:
- CO2 (sensor.co2_sensor_co2)
- Temperature (sensor.co2_sensor_temperature)
- Humidity (sensor.co2_sensor_humidity)

### Z2M Configuration
The device uses standard HA clusters and should work automatically with:
- Zigbee2MQTT
- ZHA (Zigbee Home Automation)
- deCONZ
- Other Zigbee coordinators

## Directory Structure

```
zigbee-co2/
├── CMakeLists.txt              # Project build config
├── sdkconfig.defaults          # Default SDK configuration
├── partitions.csv              # Flash partition table
├── README.md                   # This file
└── main/
    ├── CMakeLists.txt          # Main component config
    ├── idf_component.yml       # Component dependencies
    ├── esp_zb_co2_sensor.h     # Main header file
    ├── esp_zb_co2_sensor.c     # Zigbee application
    ├── scd40_driver.h          # SCD40 driver header
    └── scd40_driver.c          # SCD40 driver implementation
```

## Technical Details

### Power Consumption
- **Active Mode**: ~50mA (ESP32-C6) + 12mA (SCD40) = ~62mA
- **Router Role**: Device stays powered (not a sleepy end device)

### Memory Usage
- Flash: ~700KB
- RAM: ~120KB

### Communication
- I2C Speed: 100 kHz
- Zigbee: 802.15.4 (2.4 GHz)
- Transmission Power: Configurable via menuconfig

## License

This example is provided as-is under the CC0 license.

## References

- [SCD40 Datasheet](https://sensirion.com/products/catalog/SCD40/)
- [ESP-IDF Documentation](https://docs.espressif.com/projects/esp-idf/)
- [ESP-Zigbee SDK](https://github.com/espressif/esp-zigbee-sdk)
- [Zigbee HA Specification](https://zigbeealliance.org/)
