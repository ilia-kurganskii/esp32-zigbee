# Refactoring `main.c` into Sub-domains: A Prompt-Driven Plan

This document outlines a step-by-step plan to refactor the existing `main.c` file into five distinct sub-domains: `logger`, `metrics`, `gpio_control`, `ota_updater`, and `zigbee_handler`. Each step is presented as a prompt for a code-generation LLM, ensuring an incremental and testable approach, following C best practices.

## Step 1: Create the Logger Module

This module will provide a centralized logging system.

---

### Prompt 1.1: Create `logger.h`

```text
Create a header file named `main/logger/logger.h`.

It should include:
- `#pragma once` and header guards.
- An enum `log_level_t` with values `LOG_LEVEL_INFO`, `LOG_LEVEL_WARN`, and `LOG_LEVEL_ERROR`.
- A function declaration for `void app_log(log_level_t level, const char *tag, const char *format, ...);`.
```

---

### Prompt 1.2: Create `logger.c`

```text
Create the implementation file `main/logger/logger.c`.

It should include:
- The necessary headers: `esp_log.h`, `stdarg.h`, and `"logger.h"`.
- The implementation of the `app_log` function. This function will use a `switch` statement on the `log_level_t` to call the appropriate `esp_log_write` function (e.g., `ESP_LOGI`, `ESP_LOGW`, `ESP_LOGE`). It will use `va_list` to handle the variable arguments.
```

## Step 2: Create the Metrics Module

This module will handle application-defined counters.

---

### Prompt 2.1: Create `metrics.h`

```text
Create a header file named `main/metrics.h`.

It should include:
- `#pragma once` and header guards.
- An enum `metric_id_t` for counter IDs. For now, include `METRIC_ZIGBEE_CMD_RECEIVED` and `METRIC_OTA_STARTED`.
- Function declarations for `void metrics_init(void);`, `void metrics_increment(metric_id_t metric_id);`, and `uint32_t metrics_get(metric_id_t metric_id);`.
```

---

### Prompt 2.2: Create `metrics.c`

```text
Create the implementation file `main/metrics.c`.

It should include:
- The necessary headers: `stdint.h` and `"metrics.h"`.
- A static array to hold the counter values.
- The implementation of `metrics_init` to zero out all counters.
- The implementation of `metrics_increment` to increase a specific counter.
- The implementation of `metrics_get` to retrieve a counter's value.
```

## Step 3: Create the GPIO Control Module

This module will encapsulate all logic related to GPIO pin management and relay control.

---

### Prompt 3.1: Create `gpio_control.h`

```text
Create a header file named `main/gpio_control.h`.

It should include:
- `#pragma once` and header guards.
- `#define` constants for `RELAY1_GPIO` (GPIO_NUM_2) and `RELAY2_GPIO` (GPIO_NUM_3).
- An enum `output_state_t` with the states `STATE_NIGHT`, `STATE_DAY`, and `STATE_SHOWER`.
- Function declarations for `void init_relay_outputs(void);` and `void set_relay_outputs(output_state_t state);`.
```

---

### Prompt 3.2: Create `gpio_control.c`

```text
Create the implementation file `main/gpio_control.c`.

It should include:
- The necessary headers: `driver/gpio.h`, `"gpio_control.h"`, and `"logger.h"`.
- A `static const char *TAG = "GPIO_CONTROL";` for logging.
- The implementation of `init_relay_outputs` and `set_relay_outputs`, replacing all `ESP_LOGI` calls with `app_log(LOG_LEVEL_INFO, ...)`.
```

## Step 4: Create the OTA Updater Module

---

### Prompt 4.1: Create `ota_updater.h`

```text
Create a header file named `main/ota_updater.h`.

It should include:
- `#pragma once` and header guards.
- The necessary include: `esp_err.h`.
- `#define` constants for `OTA_URL`, `OTA_CLUSTER_ID`, and `OTA_ATTR_ID`.
- A function declaration for `void ota_update_task(void *pvParameter)`.
```

---

### Prompt 4.2: Create `ota_updater.c`

```text
Create the implementation file `main/ota_updater.c`.

It should include:
- The necessary headers: `esp_http_client.h`, `esp_https_ota.h`, `"ota_updater.h"`, `"logger.h"`, and `"metrics.h"`.
- A `static const char *TAG = "OTA_UPDATER";` for logging.
- The implementation of `ota_update_task`. It should call `metrics_increment(METRIC_OTA_STARTED)` at the beginning and use `app_log` for all logging.
```

## Step 5: Create the Zigbee Handler Module

---

### Prompt 5.1: Create `zigbee_handler.h`

```text
Create a header file named `main/zigbee_handler.h`.

It should include:
- `#pragma once` and header guards.
- The necessary includes: `esp_zigbee_core.h`.
- `extern` declarations for the device info constants: `extern const uint8_t manufacturer[];` and `extern const uint8_t model[];`.
- Function declarations for `void zigbee_main_task(void *pvParameters);`.
```

---

### Prompt 5.2: Create `zigbee_handler.c`

```text
Create the implementation file `main/zigbee_handler.c`.

It should include:
- All necessary Zigbee headers.
- Includes for the new modules: `"gpio_control.h"`, `"ota_updater.h"`, `"zigbee_handler.h"`, `"logger.h"`, and `"metrics.h"`.
- A `static const char *TAG = "ZIGBEE_HANDLER";`.
- Definitions for the `manufacturer` and `model` constants.
- All Zigbee-related functions from the original `main.c`.
- In `zb_attribute_handler`, replace logging with `app_log` and increment `METRIC_ZIGBEE_CMD_RECEIVED` when a command is processed.
- In `zigbee_main_task`, call `init_relay_outputs()`.
```

## Step 6: Refactor `main.c`

---

### Prompt 6.1: Update `main.c`

```text
Refactor `main/main.c`.

It should now only contain:
- The necessary includes: `freertos/FreeRTOS.h`, `freertos/task.h`, `nvs_flash.h`, `"zigbee_handler.h"`, `"logger.h"`, and `"metrics.h"`.
- A `static const char *TAG = "MAIN";`.
- The `app_main` function, which initializes NVS, metrics, the Zigbee platform, and then creates the `zigbee_main_task`.
- Use `app_log` for any logging.
```

## Step 7: Update the Build System

---

### Prompt 7.1: Update `main/CMakeLists.txt`

```text
Update the `main/CMakeLists.txt` file to include the new source files.

Modify the `SRCS` variable to include:
- `main.c`
- `logger.c`
- `metrics.c`
- `gpio_control.c`
- `ota_updater.c`
- `zigbee_handler.c`
```
