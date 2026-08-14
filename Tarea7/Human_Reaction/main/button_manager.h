#pragma once
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

typedef enum {
    EV_NONE = 0,
    EV_PB1_PRESSED,
    EV_PB1_RELEASED,
    EV_PB2_PRESSED,
    EV_TIMER_SIGNAL,   // disparado por el temporizador de delay aleatorio (Fase 2)
} button_event_t;

/**
 * Inicializa los GPIO de PB1/PB2 y guarda la cola donde se publicarán
 * los eventos ya "debounced".
 */
void button_manager_init(QueueHandle_t event_queue);

/**
 * Lanza la tarea de polling/debounce (10ms) en background.
 */
void button_manager_start_task(void);