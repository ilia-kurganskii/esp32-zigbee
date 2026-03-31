#include "logger.h"
#include <esp_log.h>
#include <stdarg.h>

// Centralized logging function
void app_log(log_level_t level, const char *tag, const char *format, ...) {
    va_list args;
    va_start(args, format);

    switch (level) {
        case LOG_LEVEL_INFO:
            esp_log_writev(ESP_LOG_INFO, tag, format, args);
            break;
        case LOG_LEVEL_WARN:
            esp_log_writev(ESP_LOG_WARN, tag, format, args);
            break;
        case LOG_LEVEL_ERROR:
            esp_log_writev(ESP_LOG_ERROR, tag, format, args);
            break;
    }

    va_end(args);
}
