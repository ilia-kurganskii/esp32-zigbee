#pragma once
#ifndef OTA_UPDATER_H
#define OTA_UPDATER_H

#include "esp_err.h"

// OTA update settings
#define OTA_URL "http://192.168.1.100:8070/zigbee_wtw.bin"
#define OTA_CLUSTER_ID 0xFC01
#define OTA_ATTR_ID 0x0001

// Function declaration
void ota_update_task(void *pvParameter);

#endif // OTA_UPDATER_H