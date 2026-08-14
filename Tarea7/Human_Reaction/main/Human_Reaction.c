#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "button_manager.h"
#include "led_signal.h"
#include "buzzer.h"
#include "fsm_reaction.h"
#include "oled_display.h"
#include "wifi_manager.h"
#include "mqtt_manager.h"

static const char *TAG = "MAIN";

void app_main(void)
{
    ESP_LOGI(TAG, "=== Reaction Timer - Fases 1-5 ===");

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    QueueHandle_t event_queue = xQueueCreate(10, sizeof(button_event_t));

    buzzer_init();
    led_signal_init();
    led_signal_off();
    oled_display_init();

    button_manager_init(event_queue);
    fsm_reaction_init(event_queue);

    wifi_manager_init_sta();
    mqtt_manager_init();

    oled_display_show_idle();

    button_manager_start_task();
    fsm_reaction_start_task();
}