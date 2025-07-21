#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_system.h"
#include "ota_updater.h"
#include "logger/logger.h"
#include "metrics/metrics.h"

static const char *TAG = "OTA_UPDATER";

// OTA update task
void ota_update_task(void *pvParameter)
{
    metrics_increment(METRIC_OTA_STARTED);
    app_log(LOG_LEVEL_INFO, TAG, "Starting OTA update...");

    esp_http_client_config_t config = {
        .url = OTA_URL,
        .cert_pem = NULL,
    };

    esp_https_ota_config_t ota_config = {
        .http_config = &config,
    };

    esp_err_t ret = esp_https_ota(&ota_config);
    if (ret == ESP_OK) {
        app_log(LOG_LEVEL_INFO, TAG, "OTA update successful, rebooting...");
        esp_restart();
    } else {
        app_log(LOG_LEVEL_ERROR, TAG, "OTA update failed: %s", esp_err_to_name(ret));
    }

    vTaskDelete(NULL);
}