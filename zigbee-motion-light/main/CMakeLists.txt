idf_component_register(
    SRC_DIRS  "."
    INCLUDE_DIRS "."
    PRIV_REQUIRES nvs_flash esp_timer driver espressif__esp-zigbee-lib espressif__esp-zboss-lib espressif__led_strip motion_driver light_driver time_schedule
)
