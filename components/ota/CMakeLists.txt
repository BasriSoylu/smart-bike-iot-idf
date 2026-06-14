idf_component_register(
    SRCS "src/ota.c"
    INCLUDE_DIRS "include"
    REQUIRES sim800c config app_update esp_partition log freertos json mbedtls 
)
