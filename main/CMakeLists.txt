idf_component_register(SRCS "oneshot_read_main.c"
                    PRIV_REQUIRES esp_adc esp_wifi esp_netif esp_event esp_http_server nvs_flash
                    INCLUDE_DIRS ".")
