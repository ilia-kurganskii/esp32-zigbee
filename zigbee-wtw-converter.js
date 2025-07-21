import {battery, diyruz_freepad_config} from "zigbee-herdsman-converters/converters/fromZigbee";
import {factory_reset} from "zigbee-herdsman-converters/converters/toZigbee";
import {access, presets} from "zigbee-herdsman-converters/lib/exposes";
import {bind} from "zigbee-herdsman-converters/lib/reporting";
import {getFromLookup, getKey} from "zigbee-herdsman-converters/lib/utils";

/** @type{Record<string, import('zigbee-herdsman-converters/lib/types').Fz.Converter>} */
const fzLocal = {
    wtw_multistate_report: {
        cluster: "genMultistateValue",
        type: ["readResponse", "attributeReport"],
        convert: (model, msg, publish, options, meta) => {
            if (msg.data.hasOwnProperty('presentValue')) {
                const lookup = {0: "night", 1: "day", 2: "shower"};
                const state = msg.data.presentValue;
                const mode = lookup[state] !== undefined ? lookup[state] : `unknown_${state}`;
                return {switch_actions: mode};
            }
        },
    },
};

/** @type{Record<string, import('zigbee-herdsman-converters/lib/types').Tz.Converter>} */
const tzLocal = {
    wtw_multistate_control: {
        key: ["switch_actions"],
        convertGet: async (entity, key, meta) => {
            await entity.read("genMultistateValue", ["presentValue"]);
        },
        convertSet: async (entity, key, value, meta) => {
            const switchActionsLookup = {
                night: 0,
                day: 1,
                shower: 2,
            };
            const intVal = Number(value);
            const presentValue = getFromLookup(value, switchActionsLookup, intVal);

            await entity.write("genMultistateValue", {presentValue});

            return {state: {[key]: value}};
        },
    },
};

/** @type{import('zigbee-herdsman-converters/lib/types').DefinitionWithExtend | import('zigbee-herdsman-converters/lib/types').DefinitionWithExtend[]} */
export default {
    zigbeeModel: ['WTW'],
    model: 'WTW',
    vendor: 'ESP-32',
    description: "WTW 3-channel GPIO controller for night/day/shower modes",
    fromZigbee: [fzLocal.wtw_multistate_report, battery],
    toZigbee: [tzLocal.wtw_multistate_control, factory_reset],
    exposes: [presets.enum("switch_actions", access.ALL, ["day", "night", "shower"]).withEndpoint("button")],
    configure: async (device, coordinatorEndpoint, definition) => {
        for (const ep of device.endpoints) {
            if (ep.inputClusters.includes(21)) {
                await bind(ep, coordinatorEndpoint, ["genMultistateValue"]);
            }
        }
    },
    endpoint: (device) => {
        return {
            button: 1,
        };
    },
};