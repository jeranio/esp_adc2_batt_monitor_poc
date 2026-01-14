idf_component_register(SRCS "oneshot_read_main.c"
                            "sht41_sensor.c"
                            "adc_handler.c"
                            "http_handlers.c"
                    PRIV_REQUIRES esp_adc esp_wifi esp_netif esp_event esp_http_server nvs_flash esp_driver_i2c
                    INCLUDE_DIRS ".")
