#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "led_signal.h"
#include "esp_log.h"

static const char *TAG = "LED_SIGNAL";

// Current LED state
static led_signal_state_t current_state = LED_STATE_OFF;
static TaskHandle_t led_task_handle = NULL;
static volatile bool task_running = true;

// LED blink task
static void led_blink_task(void *pvParameters)
{
    uint32_t on_time_ms = 0;
    uint32_t off_time_ms = 0;

    while (task_running) {
        // Determine blink pattern based on current state
        switch (current_state) {
            case LED_STATE_INITIALIZING:
                // Fast blink - 200ms on/off
                on_time_ms = 200;
                off_time_ms = 200;
                break;

            case LED_STATE_JOINING:
                // Medium blink - 500ms on/off
                on_time_ms = 500;
                off_time_ms = 500;
                break;

            case LED_STATE_CONNECTED:
                // Slow blink - 2000ms on/off
                on_time_ms = 2000;
                off_time_ms = 2000;
                break;

            case LED_STATE_ERROR:
                for (int i = 0; i < 5 && task_running; i++) {
                    gpio_set_level(LED_SIGNAL_GPIO, 1);
                    vTaskDelay(pdMS_TO_TICKS(100));
                    gpio_set_level(LED_SIGNAL_GPIO, 0);
                    vTaskDelay(pdMS_TO_TICKS(200));
                }
                break;

            case LED_STATE_SENSOR_READING:
                // Quick triple blink - 100ms on/100ms off (3x) then 500ms off
                for (int i = 0; i < 3 && task_running; i++) {
                    gpio_set_level(LED_SIGNAL_GPIO, 1);
                    vTaskDelay(pdMS_TO_TICKS(100));
                    gpio_set_level(LED_SIGNAL_GPIO, 0);
                    vTaskDelay(pdMS_TO_TICKS(100));
                }
                vTaskDelay(pdMS_TO_TICKS(500));
                continue; // Skip normal pattern

            case LED_STATE_COMMAND_RECEIVED:
                // Quick double blink - 100ms on/100ms off/100ms on/500ms off
                gpio_set_level(LED_SIGNAL_GPIO, 1);
                vTaskDelay(pdMS_TO_TICKS(100));
                gpio_set_level(LED_SIGNAL_GPIO, 0);
                vTaskDelay(pdMS_TO_TICKS(100));
                gpio_set_level(LED_SIGNAL_GPIO, 1);
                vTaskDelay(pdMS_TO_TICKS(100));
                gpio_set_level(LED_SIGNAL_GPIO, 0);
                vTaskDelay(pdMS_TO_TICKS(500));
                continue; // Skip normal pattern

            case LED_STATE_DEEP_SLEEP_PREPARE:
                // 3 quick blinks then off - 150ms on/150ms off (3x)
                for (int i = 0; i < 3 && task_running; i++) {
                    gpio_set_level(LED_SIGNAL_GPIO, 1);
                    vTaskDelay(pdMS_TO_TICKS(150));
                    gpio_set_level(LED_SIGNAL_GPIO, 0);
                    vTaskDelay(pdMS_TO_TICKS(150));
                }
                vTaskDelay(pdMS_TO_TICKS(1000));
                continue; // Skip normal pattern

            case LED_STATE_OTA_UPDATE:
                // Alternating pattern - 300ms on/100ms off
                on_time_ms = 300;
                off_time_ms = 100;
                break;

            case LED_STATE_OFF:
            default:
                // LED off
                gpio_set_level(LED_SIGNAL_GPIO, 0);
                vTaskDelay(pdMS_TO_TICKS(1000));
                continue;
        }

        // Execute blink pattern
        if (task_running) {
            gpio_set_level(LED_SIGNAL_GPIO, 1);
            vTaskDelay(pdMS_TO_TICKS(on_time_ms));
        }
        if (task_running) {
            gpio_set_level(LED_SIGNAL_GPIO, 0);
            vTaskDelay(pdMS_TO_TICKS(off_time_ms));
        }
    }

    // Ensure LED is off when task exits
    gpio_set_level(LED_SIGNAL_GPIO, 0);
    vTaskDelete(NULL);
}

void led_signal_init(void)
{
    // Configure LED GPIO
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << LED_SIGNAL_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = 0,
        .pull_down_en = 0,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);

    // Initialize LED to OFF
    gpio_set_level(LED_SIGNAL_GPIO, 0);

    // Create LED blink task
    task_running = true;
    xTaskCreate(led_blink_task, "led_blink", 2048, NULL, 3, &led_task_handle);

    ESP_LOGI(TAG, "LED signal initialized on GPIO %d", LED_SIGNAL_GPIO);
}

void led_signal_set_state(led_signal_state_t state)
{
    const char *state_names[] = {
        "INITIALIZING",
        "JOINING",
        "CONNECTED",
        "ERROR",
        "SENSOR_READING",
        "COMMAND_RECEIVED",
        "DEEP_SLEEP_PREPARE",
        "OTA_UPDATE",
        "OFF"
    };

    if (state < 0 || state > LED_STATE_OFF) {
        ESP_LOGW(TAG, "Invalid LED state: %d", state);
        return;
    }

    ESP_LOGI(TAG, "LED state changed: %s -> %s",
            state_names[current_state], state_names[state]);
    current_state = state;
}

void led_signal_blink_once(uint32_t duration_ms)
{
    led_signal_state_t previous_state = current_state;

    // Set to sensor reading state
    led_signal_set_state(LED_STATE_SENSOR_READING);

    // Wait for specified duration
    vTaskDelay(pdMS_TO_TICKS(duration_ms));

    // Restore previous state
    led_signal_set_state(previous_state);
}

void led_signal_stop(void)
{
    ESP_LOGI(TAG, "Stopping LED signal task");
    task_running = false;

    // Wait for task to finish
    vTaskDelay(pdMS_TO_TICKS(100));

    // Ensure LED is off
    gpio_set_level(LED_SIGNAL_GPIO, 0);

    ESP_LOGI(TAG, "LED signal stopped");
}
