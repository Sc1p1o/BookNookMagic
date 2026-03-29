idf_component_register(
        SRCS "BookNookMagic.c"
        INCLUDE_DIRS "." "../components/Assets"

        REQUIRES esp_wifi esp_event nvs_flash esp_netif esp_http_client json
)
