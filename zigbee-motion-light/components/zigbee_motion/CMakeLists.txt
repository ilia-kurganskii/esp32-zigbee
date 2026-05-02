idf_component_register(
    SRCS "zigbee_motion.c"
    INCLUDE_DIRS "."
    REQUIRES time_schedule
    PRIV_REQUIRES espressif__esp-zigbee-lib espressif__esp-zboss-lib
)
