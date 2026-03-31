# Zigbee WTW Converter

This file, `zigbee-wtw-converter.js`, is a custom converter for Zigbee2MQTT that enables communication with the **WTW 3-Channel Zigbee GPIO Controller**. It acts as a bridge between the raw Zigbee messages sent by the device and the user-friendly entities that appear in home automation platforms like Home Assistant.

## How it Works

The converter is designed to handle the `multistate_value` cluster, which is used to represent the three states of the WTW controller: **Night**, **Day**, and **Shower**.

### `fromZigbee` (Device to Zigbee2MQTT)

The `fzLocal.wtw_multistate_report` converter handles incoming messages from the device. When the device reports a change in its `presentValue` attribute, this converter translates the numeric value into a human-readable state:

*   `0` is converted to `night`
*   `1` is converted to `day`
*   `2` is converted to `shower`

This allows you to see the current state of the device in Zigbee2MQTT and Home Assistant.

### `toZigbee` (Zigbee2MQTT to Device)

The `tzLocal.wtw_multistate_control` converter handles outgoing messages to the device. When you change the state of the device from your home automation platform, this converter translates the human-readable state back into a numeric value that the device can understand:

*   `night` is converted to `0`
*   `day` is converted to `1`
*   `shower` is converted to `2`

This allows you to control the device by selecting the desired state.

## Zigbee2MQTT Integration

To use this converter, you need to place the `zigbee-wtw-converter.js` file in your Zigbee2MQTT configuration directory and add it to your `configuration.yaml` file. Once loaded, Zigbee2MQTT will automatically recognize the WTW controller and expose it as a device with a `switch_actions` entity.

This entity will be an enum that allows you to select one of the three states: `day`, `night`, or `shower`.

## Configuration

The `configure` function in the converter is responsible for binding the device's `multistate_value` cluster to the coordinator. This ensures that the device knows where to send its state updates, allowing for proper communication within the Zigbee network.
