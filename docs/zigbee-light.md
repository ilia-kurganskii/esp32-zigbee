# Zigbee Light Bulb Example

| Supported Targets | ESP32-C6 | ESP32-H2 |
| ----------------- | -------- | -------- |

This example demonstrates how to configure a Zigbee end device to function as a Home Automation (HA) on/off light bulb.

For more examples and tools for productization, refer to the [ESP Zigbee SDK documentation](https://docs.espressif.com/projects/esp-zigbee-sdk).

## Hardware Requirements

*   One development board with an ESP32-H2 SoC (to act as the Zigbee end device).
*   A USB cable for power and programming.
*   Another ESP32-H2 board to act as the Zigbee coordinator (e.g., running the [HA_on_off_switch example](../HA_on_off_switch)).

## How to Use

### 1. Set Chip Target

Before building, set the correct chip target using the following command:

```bash
idf.py --preview set-target TARGET
```

### 2. Erase NVRAM

It is recommended to erase the NVRAM before flashing to avoid conflicts with previous projects:

```bash
idf.py -p PORT erase-flash
```

### 3. Build and Flash

Build, flash, and monitor the project with this command:

```bash
idf.py -p PORT flash monitor
```

To exit the serial monitor, press `Ctrl-]`.

## Example Output

As the example runs, you will see the following log, indicating that the device has joined the network and is responding to commands:

```
I (578) ESP_ZB_ON_OFF_LIGHT: Start network steering
I (3558) ESP_ZB_ON_OFF_LIGHT: Joined network successfully (Extended PAN ID: 74:4d:bd:ff:fe:63:f7:30, PAN ID: 0x13af, Channel:13, Short Address: 0x7c16)
I (10238) ESP_ZB_ON_OFF_LIGHT: Received message: endpoint(10), cluster(0x6), attribute(0x0), data size(1)
I (10238) ESP_ZB_ON_OFF_LIGHT: Light sets to On
I (10798) ESP_ZB_ON_OFF_LIGHT: Received message: endpoint(10), cluster(0x6), attribute(0x0), data size(1)
I (10798) ESP_ZB_ON_OFF_LIGHT: Light sets to Off
```

## Light Control

By toggling the switch button (BOOT) on the coordinator device (running the `HA_on_off_switch` example), the LED on the light bulb device will turn on and off.

## Troubleshooting

If you encounter any issues, please open an [issue](https://github.com/espressif/esp-idf/issues) on GitHub.
