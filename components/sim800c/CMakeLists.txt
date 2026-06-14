idf_component_register(
    SRCS "src/sim800c.c"
    INCLUDE_DIRS "include"
    REQUIRES uart freertos config
)