#pragma once
#ifndef LED_SIGNAL_H
#define LED_SIGNAL_H

#include "driver/gpio.h"

// LED GPIO pin definition
#define LED_SIGNAL_GPIO   GPIO_NUM_15  // Status LED pin

// LED signal states for different Zigbee events
typedef enum {
    LED_STATE_INITIALIZING,      // Fast blink (200ms on/off) - Stack initializing
    LED_STATE_JOINING,           // Medium blink (500ms on/off) - Joining network
    LED_STATE_CONNECTED,         // Slow blink (2000ms on/off) - Connected to network
    LED_STATE_ERROR,             // Very fast blink (100ms on/off) - Error occurred
    LED_STATE_SENSOR_READING,    // Quick triple blink - Reading sensor data
    LED_STATE_COMMAND_RECEIVED,  // Quick double blink - Command received
    LED_STATE_DEEP_SLEEP_PREPARE,// Fade pattern (3 quick blinks) - Preparing for deep sleep
    LED_STATE_OTA_UPDATE,        // Alternating pattern (300ms/100ms) - OTA update in progress
    LED_STATE_OFF                // LED off - Idle state
} led_signal_state_t;

/**
 * @brief Initialize LED signal GPIO
 */
void led_signal_init(void);

/**
 * @brief Set LED signal state
 * @param state The desired LED state
 */
void led_signal_set_state(led_signal_state_t state);

/**
 * @brief Temporarily blink LED (for sensor reading or command received indication)
 * @param duration_ms Duration to show the blink pattern
 */
void led_signal_blink_once(uint32_t duration_ms);

/**
 * @brief Stop LED task (before deep sleep)
 */
void led_signal_stop(void);

#endif // LED_SIGNAL_H
