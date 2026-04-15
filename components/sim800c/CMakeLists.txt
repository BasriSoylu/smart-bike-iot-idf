idf_component_register(
    SRCS "sim800c.c"
    INCLUDE_DIRS "include"
    REQUIRES uart freertos config
)