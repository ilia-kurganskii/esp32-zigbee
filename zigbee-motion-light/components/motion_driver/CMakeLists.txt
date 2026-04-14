idf_component_register(
    SRCS "motion_driver.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES driver esp_timer
)
