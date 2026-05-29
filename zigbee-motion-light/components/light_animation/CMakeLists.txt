idf_component_register(
    SRCS "light_animation.c"
    INCLUDE_DIRS "."
    REQUIRES light_driver motion_driver motion_status_led zigbee_motion esp_timer
)
