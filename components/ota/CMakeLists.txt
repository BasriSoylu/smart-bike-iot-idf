idf_component_register(
    SRCS "ota.c"
    INCLUDE_DIRS "include"
    REQUIRES gsm config app_update esp_partition log freertos
)
