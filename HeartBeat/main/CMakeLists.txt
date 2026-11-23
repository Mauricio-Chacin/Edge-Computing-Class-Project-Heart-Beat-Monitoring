idf_component_register(SRCS "Functions.c" "Helpers.c" "main.c"
                    INCLUDE_DIRS "."
                    REQUIRES esp_driver_i2c esp_driver_gpio bt nvs_flash)
