#include "driver/gpio.h"
#include "gpio_control.h"
#include "logger/logger.h"

static const char *TAG = "GPIO_CONTROL";

// Set relay outputs based on state
void set_relay_outputs(output_state_t state)
{
    const char* state_names[] = {"night", "day", "shower"};
    app_log(LOG_LEVEL_INFO, TAG, "Setting relay outputs for %s mode (state %d)", state_names[state], state);

    // Control relays based on your logic:
    // Night: Relay1=OFF, Relay2=OFF
    // Day: Relay1=ON, Relay2=OFF  
    // Shower: Relay1=ON, Relay2=ON
    switch (state) {
        case STATE_NIGHT:
            gpio_set_level(RELAY1_GPIO, 1);  // Relay 1 OFF
            gpio_set_level(RELAY2_GPIO, 1);  // Relay 2 OFF
            app_log(LOG_LEVEL_INFO, TAG, "Night mode: Both relays OFF");
            break;
        case STATE_DAY:
            gpio_set_level(RELAY1_GPIO, 0);  // Relay 1 ON
            gpio_set_level(RELAY2_GPIO, 1);  // Relay 2 OFF
            app_log(LOG_LEVEL_INFO, TAG, "Day mode: Relay1=ON, Relay2=OFF");
            break;
        case STATE_SHOWER:
            gpio_set_level(RELAY1_GPIO, 0);  // Relay 1 ON
            gpio_set_level(RELAY2_GPIO, 0);  // Relay 2 ON
            app_log(LOG_LEVEL_INFO, TAG, "Shower mode: Both relays ON");
            break;
        default:
            app_log(LOG_LEVEL_WARN, TAG, "Invalid state: %d", state);
            break;
    }
}

// Initialize relay GPIO outputs
void init_relay_outputs(void)
{
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = ((1ULL << RELAY1_GPIO) | (1ULL << RELAY2_GPIO)),
        .pull_down_en = 1,  // Enable pull-down for stable LOW when OFF
        .pull_up_en = 0,
    };
    gpio_config(&io_conf);
    
    // Set drive strength to maximum for optocouplers
    gpio_set_drive_capability(RELAY1_GPIO, GPIO_DRIVE_CAP_3);
    gpio_set_drive_capability(RELAY2_GPIO, GPIO_DRIVE_CAP_3);
    
    // Initialize to day mode (Relay1=ON, Relay2=OFF)
    set_relay_outputs(STATE_DAY);
    
    app_log(LOG_LEVEL_INFO, TAG, "CV-021 relay outputs initialized: Relay1=%d, Relay2=%d", 
             RELAY1_GPIO, RELAY2_GPIO);
}