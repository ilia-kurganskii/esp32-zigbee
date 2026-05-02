/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "time_schedule.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <sys/time.h>
#include <time.h>

static const char *TAG = "TIME_SCHEDULE";

#define NVS_NAMESPACE "time_sched"

/* RTC memory - persists through deep sleep, resets on power cycle */
static RTC_DATA_ATTR bool s_time_synced = false;
/* tz_offset stored in NVS for power cycle persistence */
static int32_t s_tz_offset = TIME_SCHEDULE_DEFAULT_TZ_OFFSET;

/* Schedule config (loaded from NVS on init) */
static uint8_t s_start_hour = TIME_SCHEDULE_DEFAULT_START_HOUR;
static uint8_t s_start_min  = TIME_SCHEDULE_DEFAULT_START_MIN;
static uint8_t s_end_hour   = TIME_SCHEDULE_DEFAULT_END_HOUR;
static uint8_t s_end_min    = TIME_SCHEDULE_DEFAULT_END_MIN;
static bool    s_enabled    = true;

esp_err_t time_schedule_init(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err == ESP_OK) {
        uint8_t val;
        if (nvs_get_u8(handle, "start_h", &val) == ESP_OK) s_start_hour = val;
        if (nvs_get_u8(handle, "start_m", &val) == ESP_OK) s_start_min = val;
        if (nvs_get_u8(handle, "end_h", &val) == ESP_OK) s_end_hour = val;
        if (nvs_get_u8(handle, "end_m", &val) == ESP_OK) s_end_min = val;
        if (nvs_get_u8(handle, "enabled", &val) == ESP_OK) s_enabled = (val != 0);

        int32_t tz;
        if (nvs_get_i32(handle, "tz_offset", &tz) == ESP_OK) s_tz_offset = tz;

        uint8_t synced;
        if (nvs_get_u8(handle, "synced", &synced) == ESP_OK) s_time_synced = (synced != 0);

        nvs_close(handle);
    } else if (err == ESP_ERR_NVS_NOT_FOUND) {
        /* First boot - no config saved yet, use defaults */
        ESP_LOGI(TAG, "No saved schedule, using defaults");
    } else {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "Schedule: %s, active %02d:%02d - %02d:%02d, tz_offset=%ld, synced=%d",
             s_enabled ? "ON" : "OFF",
             s_start_hour, s_start_min,
             s_end_hour, s_end_min,
             (long)s_tz_offset, s_time_synced);
    return ESP_OK;
}

bool time_schedule_is_time_synced(void)
{
    return s_time_synced;
}

esp_err_t time_schedule_sync_time(uint32_t zigbee_utc_seconds)
{
    /* Convert Zigbee epoch (2000-01-01) to Unix epoch (1970-01-01) */
    time_t unix_time = (time_t)zigbee_utc_seconds + ZIGBEE_EPOCH_OFFSET;

    struct timeval tv = {
        .tv_sec = unix_time,
        .tv_usec = 0,
    };

    if (settimeofday(&tv, NULL) != 0) {
        ESP_LOGE(TAG, "Failed to set system time");
        return ESP_FAIL;
    }

    s_time_synced = true;

    /* Save sync status to NVS for power cycle persistence */
    nvs_handle_t nvs_handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle) == ESP_OK) {
        nvs_set_u8(nvs_handle, "synced", 1);
        nvs_set_i32(nvs_handle, "tz_offset", s_tz_offset);
        nvs_commit(nvs_handle);
        nvs_close(nvs_handle);
    }

    /* Log the synced time in local time */
    time_t local_time = unix_time + s_tz_offset;
    struct tm *tm_info = gmtime(&local_time);
    ESP_LOGI(TAG, "Time synced: %04d-%02d-%02d %02d:%02d:%02d (UTC%+ld)",
             tm_info->tm_year + 1900, tm_info->tm_mon + 1, tm_info->tm_mday,
             tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec,
             (long)(s_tz_offset / 3600));

    return ESP_OK;
}

bool time_schedule_is_active(void)
{
    if (!s_enabled) {
        return true; /* Schedule disabled = always active */
    }

    if (!s_time_synced) {
        ESP_LOGW(TAG, "Time not synced, defaulting to active (fail-open)");
        return true;
    }

    struct timeval tv;
    gettimeofday(&tv, NULL);
    time_t local_time = tv.tv_sec + s_tz_offset;
    struct tm *tm_info = gmtime(&local_time);

    int current_minutes = tm_info->tm_hour * 60 + tm_info->tm_min;
    int start_minutes   = s_start_hour * 60 + s_start_min;
    int end_minutes     = s_end_hour * 60 + s_end_min;

    bool active;
    if (start_minutes <= end_minutes) {
        /* Same-day window (e.g., 08:00 - 18:00) */
        active = (current_minutes >= start_minutes && current_minutes < end_minutes);
    } else {
        /* Overnight window (e.g., 22:00 - 06:00) */
        active = (current_minutes >= start_minutes || current_minutes < end_minutes);
    }

    ESP_LOGI(TAG, "Local time: %02d:%02d, schedule %02d:%02d-%02d:%02d -> %s",
             tm_info->tm_hour, tm_info->tm_min,
             s_start_hour, s_start_min, s_end_hour, s_end_min,
             active ? "ACTIVE" : "INACTIVE");

    return active;
}

esp_err_t time_schedule_set_hours(uint8_t start_hour, uint8_t start_min,
                                   uint8_t end_hour, uint8_t end_min)
{
    if (start_hour > 23 || end_hour > 23 || start_min > 59 || end_min > 59) {
        return ESP_ERR_INVALID_ARG;
    }

    s_start_hour = start_hour;
    s_start_min  = start_min;
    s_end_hour   = end_hour;
    s_end_min    = end_min;

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err == ESP_OK) {
        nvs_set_u8(handle, "start_h", start_hour);
        nvs_set_u8(handle, "start_m", start_min);
        nvs_set_u8(handle, "end_h", end_hour);
        nvs_set_u8(handle, "end_m", end_min);
        nvs_commit(handle);
        nvs_close(handle);
    }

    ESP_LOGI(TAG, "Schedule updated: %02d:%02d - %02d:%02d",
             start_hour, start_min, end_hour, end_min);
    return err;
}

esp_err_t time_schedule_set_enabled(bool enabled)
{
    s_enabled = enabled;

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err == ESP_OK) {
        nvs_set_u8(handle, "enabled", enabled ? 1 : 0);
        nvs_commit(handle);
        nvs_close(handle);
    }

    ESP_LOGI(TAG, "Schedule %s", enabled ? "enabled" : "disabled");
    return err;
}

int time_schedule_get_current_hour(void)
{
    if (!s_time_synced) return -1;

    struct timeval tv;
    gettimeofday(&tv, NULL);
    time_t local_time = tv.tv_sec + s_tz_offset;
    struct tm *tm_info = gmtime(&local_time);
    return tm_info->tm_hour;
}
