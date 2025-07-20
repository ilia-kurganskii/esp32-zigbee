#pragma once
#ifndef GPIO_CONTROL_H
#define GPIO_CONTROL_H

#include "driver/gpio.h"

// GPIO pin definitions
#define RELAY1_GPIO   GPIO_NUM_2  // IN1 - Used for Day and Shower modes
#define RELAY2_GPIO   GPIO_NUM_3  // IN2 - Used for Shower mode only

// Output states
typedef enum {
    STATE_NIGHT = 0,
    STATE_DAY = 1,
    STATE_SHOWER = 2
} output_state_t;

// Function declarations
void init_relay_outputs(void);
void set_relay_outputs(output_state_t state);

#endif // GPIO_CONTROL_H