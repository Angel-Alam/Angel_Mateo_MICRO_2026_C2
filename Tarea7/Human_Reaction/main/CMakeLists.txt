idf_component_register(SRCS "Human_Reaction.c" "button_manager.c" "led_signal.c" "buzzer.c" "fsm_reaction.c" "oled_display.c" "wifi_manager.c" "mqtt_manager.c"
                    INCLUDE_DIRS "."
                    REQUIRES driver esp_wifi esp_event esp_netif mqtt nvs_flash u8g2)
