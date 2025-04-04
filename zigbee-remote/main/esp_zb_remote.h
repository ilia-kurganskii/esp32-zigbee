#ifndef ESP_ZB_REMOTE_H
#define ESP_ZB_REMOTE_H

#include "esp_err.h"
#include "esp_zigbee_core.h"
#include "zcl_utility.h"
#include "driver/gpio.h"

// Remote control configuration
#define REMOTE_ENDPOINT 0x0001
#define REMOTE_PROFILE_ID ESP_ZB_AF_HA_PROFILE_ID
#define REMOTE_DEVICE_ID ESP_ZB_HA_ON_OFF_OUTPUT_DEVICE_ID

// Button GPIO configuration
#define BUTTON_ON_OFF GPIO_NUM_0
#define BUTTON_MODE GPIO_NUM_1
#define BUTTON_PIN_SEL ((1ULL<<BUTTON_ON_OFF) | (1ULL<<BUTTON_MODE))

// Button debounce time in ms
#define BUTTON_DEBOUNCE_MS 50

// Mode states
typedef enum {
    MODE_COLOR = 0,
    MODE_BRIGHTNESS = 1
} remote_mode_t;

// Function declarations
esp_err_t remote_init(void);
void remote_start(void);
void remote_stop(void);


/* Zigbee configuration */
#define INSTALLCODE_POLICY_ENABLE       false                                /* enable the install code policy for security */
#define ED_AGING_TIMEOUT                ESP_ZB_ED_AGING_TIMEOUT_64MIN        /* aging timeout of device */
#define ED_KEEP_ALIVE                   3000                                 /* 3000 millisecond */
#define HA_ESP_LIGHT_ENDPOINT           10                                   /* esp light bulb device endpoint, used to process light controlling commands */
#define ESP_ZB_PRIMARY_CHANNEL_MASK     ESP_ZB_TRANSCEIVER_ALL_CHANNELS_MASK /* Zigbee primary channel mask use in the example */

/* Basic manufacturer information */
#define ESP_MANUFACTURER_NAME "\x09""ESPRESSIF"      /* Customized manufacturer name */
#define ESP_MODEL_IDENTIFIER "\x07"CONFIG_IDF_TARGET /* Customized model identifier */

#define ESP_ZB_ZED_CONFIG()                                         \
    {                                                               \
        .esp_zb_role = ESP_ZB_DEVICE_TYPE_ED,                       \
        .install_code_policy = INSTALLCODE_POLICY_ENABLE,           \
        .nwk_cfg.zed_cfg = {                                        \
            .ed_timeout = ED_AGING_TIMEOUT,                         \
            .keep_alive = ED_KEEP_ALIVE,                            \
        },                                                          \
    }

#define ESP_ZB_DEFAULT_RADIO_CONFIG()                           \
    {                                                           \
        .radio_mode = ZB_RADIO_MODE_NATIVE,                     \
    }

#define ESP_ZB_DEFAULT_HOST_CONFIG()                            \
    {                                                           \
        .host_connection_mode = ZB_HOST_CONNECTION_MODE_NONE,   \
    }



#endif // ESP_ZB_REMOTE_H 