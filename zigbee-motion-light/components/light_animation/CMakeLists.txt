idf_component_register(
    SRCS "light_animation.c"
    INCLUDE_DIRS "."
    REQUIRES light_driver motion_driver zigbee_motion
)
