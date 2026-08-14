#include "button_manager.h"
#include "config.h"
#include "driver/gpio.h"
#include "freertos/task.h"
#include "esp_timer.h"

typedef struct {
    gpio_num_t gpio;
    bool stable_level;
    bool last_raw_level;
    int64_t last_change_ms;
} debounce_t;

static debounce_t pb1_db = { .gpio = PB1_GPIO, .stable_level = true, .last_raw_level = true };
static debounce_t pb2_db = { .gpio = PB2_GPIO, .stable_level = true, .last_raw_level = true };

static QueueHandle_t s_event_queue;

static inline int64_t millis(void)
{
    return esp_timer_get_time() / 1000;
}

static void update_debounce(debounce_t *db, button_event_t press_event, button_event_t release_event)
{
    bool raw = gpio_get_level(db->gpio);

    if (raw != db->last_raw_level) {
        db->last_raw_level = raw;
        db->last_change_ms = millis();
    }

    if ((millis() - db->last_change_ms) >= DEBOUNCE_MS && raw != db->stable_level) {
        db->stable_level = raw;
        button_event_t ev = (raw == false) ? press_event : release_event;
        if (ev != EV_NONE) {
            xQueueSend(s_event_queue, &ev, 0);
        }
    }
}

static void debounce_task(void *arg)
{
    while (1) {
        update_debounce(&pb1_db, EV_PB1_PRESSED, EV_PB1_RELEASED);
        update_debounce(&pb2_db, EV_PB2_PRESSED, EV_NONE);
        vTaskDelay(pdMS_TO_TICKS(POLL_PERIOD_MS));
    }
}

void button_manager_init(QueueHandle_t event_queue)
{
    s_event_queue = event_queue;

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << PB1_GPIO) | (1ULL << PB2_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
}

void button_manager_start_task(void)
{
    xTaskCreate(debounce_task, "debounce_task", 2048, NULL, 10, NULL);
}