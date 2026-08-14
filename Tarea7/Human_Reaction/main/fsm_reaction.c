#include "fsm_reaction.h"
#include "button_manager.h"
#include "led_signal.h"
#include "buzzer.h"
#include "oled_display.h"
#include "mqtt_manager.h"
#include "config.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_random.h"

static const char *TAG = "FSM_REACTION";

static const char *state_names[ST_COUNT] = {
    "IDLE", "ARMADO", "SENAL_ACTIVA", "TRANSICION", "RESULTADO", "FALSO_ARRANQUE"
};

static fsm_state_t current_state = ST_IDLE;
static QueueHandle_t s_event_queue;
static esp_timer_handle_t s_delay_timer;

static int64_t s_signal_on_time_us = 0;
static int64_t s_pb1_release_time_us = 0;

static float s_reaccion_ms = 0.0f;
static float s_movimiento_ms = 0.0f;
static float s_total_ms = 0.0f;

static void goto_state(fsm_state_t new_state)
{
    ESP_LOGI(TAG, "Transicion: %s -> %s", state_names[current_state], state_names[new_state]);
    current_state = new_state;

    mqtt_manager_publish_estado(state_names[new_state]);

    switch (new_state) {
    case ST_IDLE:            oled_display_show_idle();           break;
    case ST_ARMADO:          oled_display_show_armado();         break;
    case ST_SENAL_ACTIVA:    oled_display_show_senal();          break;
    case ST_TRANSICION:      oled_display_show_transicion();     break;
    case ST_FALSO_ARRANQUE:  oled_display_show_falso_arranque(); break;
    default: break;
    }
}

static void delay_timer_callback(void *arg)
{
    button_event_t ev = EV_TIMER_SIGNAL;
    xQueueSend(s_event_queue, &ev, 0);
}

static void start_random_delay_timer(void)
{
    uint32_t range_ms = RANDOM_DELAY_MAX_MS - RANDOM_DELAY_MIN_MS;
    uint32_t delay_ms = RANDOM_DELAY_MIN_MS + (esp_random() % range_ms);

    ESP_LOGI(TAG, "Delay aleatorio: %u ms", (unsigned int)delay_ms);
    esp_timer_start_once(s_delay_timer, (uint64_t)delay_ms * 1000ULL);
}

static void fsm_task(void *arg)
{
    button_event_t ev;

    while (1) {
        if (xQueueReceive(s_event_queue, &ev, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        switch (current_state) {

        case ST_IDLE:
            if (ev == EV_PB1_PRESSED) {
                ESP_LOGI(TAG, "PB1 presionado. Preparando ronda...");
                led_signal_set_color(20, 15, 0);
                goto_state(ST_ARMADO);
                start_random_delay_timer();
            }
            break;

        case ST_ARMADO:
            if (ev == EV_PB1_RELEASED) {
                esp_timer_stop(s_delay_timer);
                ESP_LOGW(TAG, "Falso arranque detectado!");
                buzzer_beep(300);
                led_signal_set_color(40, 0, 0);
                mqtt_manager_publish_resultado(0.0f, 0.0f, 0.0f, "FALSO_ARRANQUE");
                goto_state(ST_FALSO_ARRANQUE);
            } else if (ev == EV_TIMER_SIGNAL) {
                s_signal_on_time_us = esp_timer_get_time();
                led_signal_set_color(0, 40, 0);
                ESP_LOGI(TAG, "SEÑAL ACTIVADA");
                goto_state(ST_SENAL_ACTIVA);
            }
            break;

        case ST_SENAL_ACTIVA:
            if (ev == EV_PB1_RELEASED) {
                s_pb1_release_time_us = esp_timer_get_time();
                s_reaccion_ms = (s_pb1_release_time_us - s_signal_on_time_us) / 1000.0f;
                ESP_LOGI(TAG, "PB1 soltado. Tiempo de reaccion: %.1f ms", s_reaccion_ms);
                led_signal_set_color(0, 0, 40);
                goto_state(ST_TRANSICION);
            }
            break;

        case ST_TRANSICION:
            if (ev == EV_PB2_PRESSED) {
                int64_t pb2_time_us = esp_timer_get_time();
                s_movimiento_ms = (pb2_time_us - s_pb1_release_time_us) / 1000.0f;
                s_total_ms = s_reaccion_ms + s_movimiento_ms;

                ESP_LOGI(TAG, "PB2 presionado. Tiempo de movimiento: %.1f ms", s_movimiento_ms);
                ESP_LOGI(TAG, "===> RESULTADO: reaccion=%.1fms movimiento=%.1fms total=%.1fms",
                         s_reaccion_ms, s_movimiento_ms, s_total_ms);

                buzzer_beep(100);
                led_signal_set_color(0, 40, 40);

                oled_display_show_resultado(s_reaccion_ms, s_movimiento_ms, s_total_ms);
                mqtt_manager_publish_resultado(s_reaccion_ms, s_movimiento_ms, s_total_ms, "OK");

                current_state = ST_RESULTADO;
                ESP_LOGI(TAG, "Transicion: TRANSICION -> RESULTADO");
                mqtt_manager_publish_estado(state_names[ST_RESULTADO]);
            }
            break;

        case ST_RESULTADO:
        case ST_FALSO_ARRANQUE:
            vTaskDelay(pdMS_TO_TICKS(1500));
            led_signal_off();
            goto_state(ST_IDLE);
            break;

        default:
            break;
        }
    }
}

void fsm_reaction_init(QueueHandle_t event_queue)
{
    s_event_queue = event_queue;

    const esp_timer_create_args_t timer_args = {
        .callback = &delay_timer_callback,
        .name = "random_delay_timer",
    };
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &s_delay_timer));
}

void fsm_reaction_start_task(void)
{
    xTaskCreate(fsm_task, "fsm_task", 4096, NULL, 5, NULL);
}