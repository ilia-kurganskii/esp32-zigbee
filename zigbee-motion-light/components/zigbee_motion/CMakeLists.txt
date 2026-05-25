idf_component_register(
    SRCS "zigbee_motion.c"
    INCLUDE_DIRS "."
    REQUIRES link_status_led motion_driver nvs_flash
    PRIV_REQUIRES espressif__esp-zigbee-lib espressif__esp-zboss-lib
)
