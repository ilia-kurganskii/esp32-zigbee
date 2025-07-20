#pragma once
#ifndef LOGGER_H
#define LOGGER_H

// Enum for log levels
typedef enum {
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERROR
} log_level_t;

// Function declaration for the centralized logger
void app_log(log_level_t level, const char *tag, const char *format, ...);

#endif // LOGGER_H
