import {battery, diyruz_freepad_config} from "zigbee-herdsman-converters/converters/fromZigbee";
import {factory_reset} from "zigbee-herdsman-converters/converters/toZigbee";
import {access, presets} from "zigbee-herdsman-converters/lib/exposes";
import {bind} from "zigbee-herdsman-converters/lib/reporting";
import {getFromLookup, getKey} from "zigbee-herdsman-converters/lib/utils";

/** @type{Record<string, import('zigbee-herdsman-converters/lib/types').Fz.Converter>} */
const fzLocal = {
    diyruz_freepad_clicks: {
        cluster: "genMultistateInput",
        type: ["readResponse", "attributeReport"],
        convert: (model, msg, publish, options, meta) => {
            const ep = model.endpoint?.(msg.device);

            if (ep) {
                const button = getKey(ep, msg.endpoint.ID);
                const lookup = {0: "night", 1: "day", 2: "shower"};
                const clicks = msg.data.presentValue;
                const action = lookup[clicks] ? lookup[clicks] : `non_found_${clicks}`;
                return {action: `${button}_${action}`};
            }
        },
    },
};

/** @type{Record<string, import('zigbee-herdsman-converters/lib/types').Tz.Converter>} */
const tzLocal = {
    diyruz_freepad_on_off_config: {
        key: ["switch_actions"],
        convertGet: async (entity, key, meta) => {
            await entity.read("genOnOffSwitchCfg", ["switchActions"]);
        },
        convertSet: async (entity, key, value, meta) => {
            const switchActionsLookup = {
                night: 0x00,
                day: 0x01,
                shower: 0x02,
            };
            const intVal = Number(value);
            const switchActions = getFromLookup(value, switchActionsLookup, intVal);

            const payloads = {
                switch_actions: {switchActions},
            };

            await entity.write("genOnOffSwitchCfg", payloads[key]);

            return {state: {[`${key}`]: value}};
        },
    },
};

/** @type{import('zigbee-herdsman-converters/lib/types').DefinitionWithExtend | import('zigbee-herdsman-converters/lib/types').DefinitionWithExtend[]} */
export default {
    zigbeeModel: ['WTW'],
    model: 'WTW',
    vendor: 'ESP-32',
    description: "[DiY 8/12/20 button keypad](http://modkam.ru/?p=1114)",
    fromZigbee: [fzLocal.diyruz_freepad_clicks, diyruz_freepad_config, battery],
    toZigbee: [tzLocal.diyruz_freepad_on_off_config, factory_reset],
    exposes: [presets.enum("switch_actions", access.ALL, ["day", "night", "shower"]).withEndpoint("button")],
    configure: async (device, coordinatorEndpoint, definition) => {
        for (const ep of device.endpoints) {
            if (ep.outputClusters.includes(18)) {
                await bind(ep, coordinatorEndpoint, ["genMultistateInput"]);
            }
        }
    },
    endpoint: (device) => {
        return {
            button: 1,
        };
    },
};