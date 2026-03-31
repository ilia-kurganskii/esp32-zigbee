idf_component_register(
    SRCS "esp_analytics.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_http_client nvs_flash freertos esp_timer esp_wifi json
)
