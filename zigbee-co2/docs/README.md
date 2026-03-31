# Zigbee CO2 Sensor Documentation

This directory contains documentation for the Zigbee CO2 sensor project with ESP Analytics integration.

## Documentation Files

### [ANALYTICS_INTEGRATION.md](./ANALYTICS_INTEGRATION.md)
Comprehensive guide for the ESP Analytics component integration with the CO2 sensor project.

**Topics covered:**
- Integration points and implementation details
- Tracked metrics and events
- Power management and deep sleep integration
- Configuration options
- Data flow and examples
- Testing and troubleshooting
- Performance impact analysis

## Project Overview

The Zigbee CO2 sensor project combines:
- **SCD40 CO2 sensor** for environmental monitoring
- **Zigbee HA compliance** for smart home integration
- **ESP Analytics** for telemetry and monitoring
- **Power optimization** with deep sleep support

## Quick Links

- [Component Documentation](../components/esp_analytics/README.md)
- [Integration Guide](../components/esp_analytics/INTEGRATION_GUIDE.md)
- [Main Project README](../README.md)

## Getting Started

1. **Configure Analytics**: Edit `sdkconfig.defaults` with your server details
2. **Build Project**: `idf.py build`
3. **Flash Device**: `idf.py flash monitor`
4. **Monitor Analytics**: Check your telemetry service for incoming data

## Analytics Features

- **Automatic Tracking**: Sensor readings, errors, Zigbee messages
- **Power Efficient**: Deep sleep integration, batch transmission
- **Configurable**: Server URL, API key, transmission intervals
- **Robust**: Error handling, retry logic, network failure recovery

## Support

For analytics-specific issues:
1. Check the [ANALYTICS_INTEGRATION.md](./ANALYTICS_INTEGRATION.md) troubleshooting section
2. Review the component documentation in `components/esp_analytics/`
3. Use the test file to verify functionality

For general project issues:
1. Review the main [README.md](../README.md)
2. Check component-specific documentation
3. Monitor device logs for error messages
