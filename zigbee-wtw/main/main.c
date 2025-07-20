/**
 * WTW 3-Channel GPIO Controller
 * Receives multistate commands from Zigbee2MQTT to control GPIO outputs
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "esp_zigbee_core.h"
#include "zigbee_handler/zigbee_handler.h"
#include "logger/logger.h"
#include "metrics/metrics.h"

static const char *TAG = "MAIN";

void app_main(void)
{
    // Initialize NVS
    ESP_ERROR_CHECK(nvs_flash_erase());
    ESP_ERROR_CHECK(nvs_flash_init());

    // Initialize metrics
    metrics_init();

    // Initialize Zigbee platform
    esp_zb_platform_config_t config = {
        .radio_config = {.radio_mode = ZB_RADIO_MODE_NATIVE},
        .host_config = {.host_connection_mode = ZB_HOST_CONNECTION_MODE_NONE},
    };
    ESP_ERROR_CHECK(esp_zb_platform_config(&config));
    
    app_log(LOG_LEVEL_INFO, TAG, "Starting WTW 2-relay Zigbee controller");
    
    // Start Zigbee task
    xTaskCreate(zigbee_main_task, "Zigbee_main", 4096, NULL, 5, NULL);
}