const {postfixWithEndpointName} = require('zigbee-herdsman-converters/lib/utils');
const exposes = require('zigbee-herdsman-converters/lib/exposes');
const fz = require('zigbee-herdsman-converters/converters/fromZigbee');
const tz = require('zigbee-herdsman-converters/converters/toZigbee');
const ea = exposes.access;
const reporting = require('zigbee-herdsman-converters/lib/reporting');

const definition = {
    zigbeeModel: ['ESP32-WTW-Switch'],
    model: 'ESP32-WTW-Switch',
    vendor: 'COSMIKIN',
    description: 'ESP32-WTW mode control device',
    fromZigbee: [fz.ignore_basic_report, fz.multistate_value],
    toZigbee: [tz.multistate_value],
    exposes: [
        // Mode control using multistate value (1=night, 2=daily, 3=full)
        exposes.numeric('multistate_value', ea.ALL).withValueMin(1).withValueMax(3).withDescription('Mode: 1=night, 2=daily, 3=full'),
    ],
    endpoint: (device) => {
        return {'default': 1};
    },
    configure: async (device, coordinatorEndpoint, logger) => {
        const endpoint = device.getEndpoint(1);
        await reporting.bind(endpoint, coordinatorEndpoint, ['msMultistateValue']);
    },
};

module.exports = definition;