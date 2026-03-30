idf_component_register(
    SRC_DIRS  "." "logger" "metrics" "gpio_control" "ota_updater" "zigbee_handler" "$ENV{IDF_PATH}/examples/zigbee/common/zcl_utility/src"
    INCLUDE_DIRS "." "logger" "metrics" "gpio_control" "ota_updater" "zigbee_handler" "$ENV{IDF_PATH}/examples/zigbee/common/zcl_utility/include"
    PRIV_REQUIRES led_signal esp_http_client nvs_flash esp_https_ota
)
