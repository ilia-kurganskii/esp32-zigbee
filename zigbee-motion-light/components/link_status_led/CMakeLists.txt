idf_component_register(
    SRCS "link_status_led.c"
    INCLUDE_DIRS "."
    REQUIRES light_driver esp_timer
)
