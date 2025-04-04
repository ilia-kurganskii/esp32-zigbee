#include "esp_zb_remote.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "ha/esp_zigbee_ha_standard.h"

static const char *TAG = "ESP_ZB_REMOTE";

// Button event queue
static QueueHandle_t button_queue = NULL;

// Current mode
static remote_mode_t current_mode = MODE_COLOR;

// Button event structure
typedef struct {
    uint32_t gpio_num;
    bool pressed;
} button_event_t;

// Button ISR handler
static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    uint32_t gpio_num = (uint32_t) arg;
    button_event_t evt = {
        .gpio_num = gpio_num,
        .pressed = gpio_get_level(gpio_num) == 0  // Active low
    };
    xQueueSendFromISR(button_queue, &evt, NULL);
}

// Button task
static void button_task(void *pvParameters)
{
    button_event_t evt;
    uint32_t last_press_time = 0;
    
    while (1) {
        if (xQueueReceive(button_queue, &evt, portMAX_DELAY)) {
            uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
            
            // Debounce check
            if (now - last_press_time > BUTTON_DEBOUNCE_MS) {
                last_press_time = now;
                
                // Handle button press
                if (evt.pressed) {
                    switch (evt.gpio_num) {
                        case BUTTON_ON_OFF:
                            // Toggle On/Off
                            break;
                            
                        case BUTTON_MODE:
                            // Toggle between color and brightness modes
                            current_mode = (current_mode == MODE_COLOR) ? MODE_BRIGHTNESS : MODE_COLOR;
                            ESP_LOGI(TAG, "Mode switched to: %s", current_mode == MODE_COLOR ? "COLOR" : "BRIGHTNESS");
                            break;
                    }
                }
            }
        }
    }
}

static void bdb_start_top_level_commissioning_cb(uint8_t mode_mask)
{
    ESP_RETURN_ON_FALSE(esp_zb_bdb_start_top_level_commissioning(mode_mask) == ESP_OK, , TAG, "Failed to start Zigbee commissioning");
}

void esp_zb_app_signal_handler(esp_zb_app_signal_t *signal_struct)
{
    uint32_t *p_sg_p = signal_struct->p_app_signal;
    esp_err_t err_status = signal_struct->esp_err_status;
    esp_zb_app_signal_type_t sig_type = *p_sg_p;
    switch (sig_type) {
    case ESP_ZB_ZDO_SIGNAL_SKIP_STARTUP:
        ESP_LOGI(TAG, "Initialize Zigbee stack");
        esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_INITIALIZATION);
        break;
    case ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START:
    case ESP_ZB_BDB_SIGNAL_DEVICE_REBOOT:
        if (err_status == ESP_OK) {
            ESP_LOGI(TAG, "Device started up in%s factory-reset mode", esp_zb_bdb_is_factory_new() ? "" : " non");
            if (esp_zb_bdb_is_factory_new()) {
                ESP_LOGI(TAG, "Start network steering");
                esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_STEERING);
            } else {
                ESP_LOGI(TAG, "Device rebooted");
            }
        } else {
            ESP_LOGW(TAG, "%s failed with status: %s, retrying", esp_zb_zdo_signal_to_string(sig_type),
                     esp_err_to_name(err_status));
            esp_zb_scheduler_alarm((esp_zb_callback_t)bdb_start_top_level_commissioning_cb,
                                   ESP_ZB_BDB_MODE_INITIALIZATION, 1000);
        }
        break;
    case ESP_ZB_BDB_SIGNAL_STEERING:
        if (err_status == ESP_OK) {
            esp_zb_ieee_addr_t extended_pan_id;
            esp_zb_get_extended_pan_id(extended_pan_id);
            ESP_LOGI(TAG, "Joined network successfully (Extended PAN ID: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x, PAN ID: 0x%04hx, Channel:%d, Short Address: 0x%04hx)",
                     extended_pan_id[7], extended_pan_id[6], extended_pan_id[5], extended_pan_id[4],
                     extended_pan_id[3], extended_pan_id[2], extended_pan_id[1], extended_pan_id[0],
                     esp_zb_get_pan_id(), esp_zb_get_current_channel(), esp_zb_get_short_address());
            // Start remote control after successful network join
            remote_start();
        } else {
            ESP_LOGI(TAG, "Network steering was not successful (status: %s)", esp_err_to_name(err_status));
            esp_zb_scheduler_alarm((esp_zb_callback_t)bdb_start_top_level_commissioning_cb, ESP_ZB_BDB_MODE_NETWORK_STEERING, 1000);
        }
        break;
    case ESP_ZB_NWK_SIGNAL_PERMIT_JOIN_STATUS:
        if (err_status == ESP_OK) {
            if (*(uint8_t *)esp_zb_app_signal_get_params(p_sg_p)) {
                ESP_LOGI(TAG, "Network(0x%04hx) is open for %d seconds", esp_zb_get_pan_id(), *(uint8_t *)esp_zb_app_signal_get_params(p_sg_p));
            } else {
                ESP_LOGW(TAG, "Network(0x%04hx) closed, devices joining not allowed.", esp_zb_get_pan_id());
            }
        }
        break;
    default:
        ESP_LOGI(TAG, "ZDO signal: %s (0x%x), status: %s", esp_zb_zdo_signal_to_string(sig_type), sig_type, esp_err_to_name(err_status));
        break;
    }
}

esp_err_t remote_init(void)
{
    // Create button event queue
    button_queue = xQueueCreate(10, sizeof(button_event_t));
    if (button_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create button queue");
        return ESP_FAIL;
    }

    // Configure GPIOs
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_ANYEDGE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = BUTTON_PIN_SEL,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE
    };

    if (gpio_config(&io_conf) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure GPIOs");
        return ESP_FAIL;
    }

    // Install GPIO ISR service
    if (gpio_install_isr_service(0) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install GPIO ISR service");
        return ESP_FAIL;
    }

    // Add ISR handlers for both buttons
    if (gpio_isr_handler_add(BUTTON_ON_OFF, gpio_isr_handler, (void*)BUTTON_ON_OFF) != ESP_OK ||
        gpio_isr_handler_add(BUTTON_MODE, gpio_isr_handler, (void*)BUTTON_MODE) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add ISR handlers");
        return ESP_FAIL;
    }

    // Create button task
    if (xTaskCreate(button_task, "button_task", 2048, NULL, 5, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create button task");
        return ESP_FAIL;
    }

    return ESP_OK;
}

void remote_start(void)
{
    // Enable GPIO interrupts
    gpio_intr_enable(BUTTON_ON_OFF);
    gpio_intr_enable(BUTTON_MODE);
}

void remote_stop(void)
{
    // Disable GPIO interrupts
    gpio_intr_disable(BUTTON_ON_OFF);
    gpio_intr_disable(BUTTON_MODE);
}

static void esp_zb_task(void *pvParameters)
{
    /* initialize Zigbee stack */
    esp_zb_cfg_t zb_nwk_cfg = ESP_ZB_ZED_CONFIG();
    esp_zb_init(&zb_nwk_cfg);
    
    // Initialize remote control
    if (remote_init() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize remote control");
        return;
    }

    // Create remote control endpoint
    esp_zb_ep_list_t *ep_list = esp_zb_ep_list_create();
    
    // Register the endpoint
    esp_zb_device_register(ep_list);
    
    // Set primary channel mask
    esp_zb_set_primary_network_channel_set(ESP_ZB_PRIMARY_CHANNEL_MASK);
    
    // Start Zigbee stack
    ESP_ERROR_CHECK(esp_zb_start(false));
    
    // Main Zigbee loop
    esp_zb_stack_main_loop();
}

void app_main(void)
{
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize Zigbee platform
    esp_zb_platform_config_t config = {
        .radio_config = ESP_ZB_DEFAULT_RADIO_CONFIG(),
        .host_config = ESP_ZB_DEFAULT_HOST_CONFIG(),
    };
    ESP_ERROR_CHECK(esp_zb_platform_config(&config));

    // Create Zigbee task
    xTaskCreate(esp_zb_task, "Zigbee_main", 4096, NULL, 5, NULL);
} 