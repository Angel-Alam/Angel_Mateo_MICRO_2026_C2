#include "mqtt_manager.h"
#include "config.h"
#include "mqtt_client.h"
#include "esp_log.h"
#include <stdio.h>

static const char *TAG = "MQTT";
static esp_mqtt_client_handle_t s_client;
static bool s_connected = false;

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT conectado a %s", MQTT_BROKER_URI);
        s_connected = true;
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "MQTT desconectado");
        s_connected = false;
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGE(TAG, "Error MQTT");
        break;
    default:
        break;
    }
}

void mqtt_manager_init(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = MQTT_BROKER_URI,
    };

    s_client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(s_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(s_client);
}

bool mqtt_manager_is_connected(void)
{
    return s_connected;
}

void mqtt_manager_publish_estado(const char *estado)
{
    if (!s_connected) return;
    esp_mqtt_client_publish(s_client, MQTT_TOPIC_ESTADO, estado, 0, 1, 0);
}

void mqtt_manager_publish_resultado(float reaccion_ms, float movimiento_ms, float total_ms, const char *estado)
{
    if (!s_connected) {
        ESP_LOGW(TAG, "MQTT no conectado, no se publica resultado");
        return;
    }

    char payload[160];
    snprintf(payload, sizeof(payload),
             "{\"reaccion_ms\":%.1f,\"movimiento_ms\":%.1f,\"total_ms\":%.1f,\"estado\":\"%s\"}",
             reaccion_ms, movimiento_ms, total_ms, estado);

    esp_mqtt_client_publish(s_client, MQTT_TOPIC_RESULTADO, payload, 0, 1, 0);
    ESP_LOGI(TAG, "Publicado: %s", payload);
}