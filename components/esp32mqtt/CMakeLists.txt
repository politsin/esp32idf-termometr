idf_component_register(
    SRCS esp32mqtt.cpp
    INCLUDE_DIRS .
    REQUIRES freertos log mqtt esp_netif nvs_flash
)
