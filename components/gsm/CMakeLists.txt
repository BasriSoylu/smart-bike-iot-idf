idf_component_register(
    SRCS "src/gsm.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_driver_uart log
)