# ESP32 Zigbee Projects

This repository contains a collection of Zigbee projects for the ESP32-C6 and ESP32-H2 microcontrollers, developed using the Espressif IoT Development Framework (ESP-IDF).

## Projects

This repository is organized into the following sub-projects:

*   **[Deep Sleep](./deep_sleep.md):** Demonstrates a Thread Sleepy End Device (SED) that utilizes the deep sleep mode to minimize power consumption.
*   **[Zigbee Light](./zigbee-light.md):** An implementation of a Zigbee Home Automation (HA) on/off light bulb.
*   **[Zigbee Remote](./zigbee-remote.md):** An implementation of a Zigbee Home Automation (HA) on/off switch, designed to control the Zigbee Light.
*   **[WTW 3-Channel Zigbee GPIO Controller](./zigbee-wtw.md):** A 3-channel Zigbee GPIO controller for managing a 2-relay module with three distinct states (Night, Day, Shower).

## Utilities

*   **[Zigbee WTW Converter](./zigbee-wtw-converter.md):** A JavaScript utility for converting values for the WTW 3-Channel Zigbee GPIO Controller.

## Getting Started

To get started with any of the projects, navigate to the project's directory and follow the instructions in its respective documentation. Each project is self-contained and includes its own build configurations and source code.

Before building and flashing any of the projects, ensure that you have the ESP-IDF development environment set up correctly. You can find more information on setting up the ESP-IDF in the [official Espressif documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html).
