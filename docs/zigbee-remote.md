# Zigbee Light Switch Example

| Supported Targets | ESP32-C6 | ESP32-H2 |
| ----------------- | -------- | -------- |

This example demonstrates how to configure a Zigbee Coordinator to function as a Home Automation (HA) on/off switch.

For more examples and tools for productization, refer to the [ESP Zigbee SDK documentation](https://docs.espressif.com/projects/esp-zigbee-sdk).

## Hardware Requirements

*   One development board with an ESP32-H2 SoC (to act as the Zigbee Coordinator).
*   A USB cable for power and programming.
*   Another ESP32-H2 board to act as the Zigbee end device (e.g., running the [HA_on_off_light example](../HA_on_off_light/)).

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

As the example runs, you will see the following log, indicating that the device has formed a network and is ready to control other devices:

```
I (781) ESP_ZB_ON_OFF_SWITCH: Formed network successfully (Extended PAN ID: 74:4d:bd:ff:fe:63:f7:30, PAN ID: 0x13af, Channel:13, Short Address: 0x0000)
I (1391) ESP_ZB_ON_OFF_SWITCH: Network steering started
I (9601) ESP_ZB_ON_OFF_SWITCH: New device commissioned or rejoined (short: 0x7c16)
I (9671) ESP_ZB_ON_OFF_SWITCH: Found light
I (9681) ESP_ZB_ON_OFF_SWITCH: Bound successfully!
I (16451) ESP_ZB_ON_OFF_SWITCH: Send 'on_off toggle' command
```

## Light Control

By toggling the switch button (BOOT) on this board, the LED on the board loaded with the `HA_on_off_light` example will turn on and off.

## Troubleshooting

If you encounter any issues, please open an [issue](https://github.com/espressif/esp-idf/issues) on GitHub.
