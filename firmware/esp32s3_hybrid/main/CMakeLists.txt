idf_component_register(
    SRCS "main.c" "wav_pcm.c"
    INCLUDE_DIRS "."
    REQUIRES esp_wifi esp_event esp_netif nvs_flash esp_http_client esp_driver_i2s esp_timer json
)
