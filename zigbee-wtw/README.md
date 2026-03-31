# WTW 3-Channel Zigbee GPIO Controller

This project implements a 3-channel Zigbee GPIO controller for the ESP32-C6, designed to work with a 2-relay module. It receives multistate commands from a Zigbee coordinator (like Zigbee2MQTT) to control the GPIO outputs, allowing for three distinct states: Night, Day, and Shower.

## Features

*   **3-State Control:** Manages three states (Night, Day, Shower) using a 2-relay module.
*   **Zigbee Integration:** Acts as a Zigbee End Device (ZED) and communicates using the Zigbee protocol.
*   **Zigbee2MQTT Compatible:** Designed to be easily integrated with Zigbee2MQTT using the multistate value cluster.
*   **ESP32-C6 Based:** Built on the Espressif IoT Development Framework (ESP-IDF) for the ESP32-C6.

## Hardware

*   **ESP32-C6 Development Board:** The microcontroller running the Zigbee application.
*   **2-Relay Module:** The module controlled by the ESP32-C6's GPIO pins.
    *   `RELAY1_GPIO` is connected to `GPIO_NUM_2`
    *   `RELAY2_GPIO` is connected to `GPIO_NUM_3`

## States

The controller operates in one of three states, with the following relay configurations:

| State   | `RELAY1` | `RELAY2` |
| :------ | :------- | :------- |
| Night   | OFF      | OFF      |
| Day     | ON       | OFF      |
| Shower  | ON       | ON       |

The default state is **Day**.

## Building and Flashing

This project is built using the ESP-IDF. To build and flash the project to your ESP32-C6, follow these steps:

1.  **Set up the ESP-IDF:**
    Follow the official Espressif documentation to set up the ESP-IDF development environment.

2.  **Configure the Project:**
    ```bash
    idf.py set-target esp32c6
    idf.py menuconfig
    ```
    In `menuconfig`, you can adjust the GPIO pin assignments and other project settings.

3.  **Build the Project:**
    ```bash
    idf.py build
    ```

4.  **Flash the Project:**
    Connect your ESP32-C6 board and run the following command:
    ```bash
    idf.py -p (PORT) flash
    ```
    Replace `(PORT)` with your device's serial port (e.g., `/dev/ttyUSB0`).

## Zigbee2MQTT Integration

Once the device is flashed and has joined your Zigbee network, it will appear in Zigbee2MQTT as a simple sensor. You can control its state by sending a command to the `present_value` attribute of the `multistate_value` cluster.

*   **Set State to Night:** `0`
*   **Set State to Day:** `1`
*   **Set State to Shower:** `2`

You can use the Zigbee2MQTT frontend or an MQTT client to send these commands.