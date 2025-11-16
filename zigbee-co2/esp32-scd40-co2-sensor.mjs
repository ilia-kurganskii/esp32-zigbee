/**
 * Zigbee2MQTT External Converter for ESP32 SCD40 CO2 Sensor
 *
 * This converter enables Zigbee2MQTT to properly recognize and handle
 * the ESP32-based CO2 sensor with SCD40.
 *
 * Installation:
 * 1. Copy this file to your Zigbee2MQTT data directory
 * 2. Add to configuration.yaml:
 *    external_converters:
 *      - esp32-scd40-co2-sensor.mjs
 * 3. Restart Zigbee2MQTT
 *
 * Device Features:
 * - Temperature measurement (°C)
 * - Humidity measurement (%RH)
 * - CO2 measurement (ppm)
 */

import {
  temperature as fzTemperature,
  humidity as fzHumidity,
  co2 as fzCo2,
} from "zigbee-herdsman-converters/converters/fromZigbee.js";

import * as exposes from "zigbee-herdsman-converters/lib/exposes.js";
import * as reporting from "zigbee-herdsman-converters/lib/reporting.js";

const { presets: e, access: ea } = exposes;

const definition = {
  zigbeeModel: ["CO2-Sensor"], // Must match ESP_MODEL_IDENTIFIER in firmware
  model: "ESP32-SCD40-CO2",
  vendor: "Espressif", // Must match ESP_MANUFACTURER_NAME in firmware
  description: "ESP32-based CO2, temperature and humidity sensor with SCD40",

  fromZigbee: [fzTemperature, fzHumidity, fzCo2],

  toZigbee: [],

  exposes: [e.temperature(), e.humidity(), e.co2()],

  configure: async (device, coordinatorEndpoint, logger) => {
    const endpoint = device.getEndpoint(1);

    // Bind clusters
    await reporting.bind(endpoint, coordinatorEndpoint, [
      "msTemperatureMeasurement",
      "msRelativeHumidity",
      "msCO2",
    ]);

    // Configure reporting for temperature (in 0.01°C)
    await reporting.temperature(endpoint, {
      min: 10, // Minimum 10 seconds
      max: 300, // Maximum 5 minutes
      change: 50, // Report on 0.5°C change
    });

    // Configure reporting for humidity (in 0.01%)
    await reporting.humidity(endpoint, {
      min: 10, // Minimum 10 seconds
      max: 300, // Maximum 5 minutes
      change: 100, // Report on 1% change
    });

    // Configure reporting for CO2 (as fraction, needs custom config)
    await endpoint.configureReporting("msCO2", [
      {
        attribute: "measuredValue",
        minimumReportInterval: 10, // Minimum 10 seconds
        maximumReportInterval: 300, // Maximum 5 minutes
        reportableChange: 0.00005, // Report on 50 ppm change (50/1000000)
      },
    ]);
  },
};

export default definition;
