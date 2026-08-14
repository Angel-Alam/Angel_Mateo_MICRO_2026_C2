#pragma once
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

typedef enum {
    ST_IDLE = 0,
    ST_ARMADO,
    ST_SENAL_ACTIVA,
    ST_TRANSICION,
    ST_RESULTADO,
    ST_FALSO_ARRANQUE,
    ST_COUNT
} fsm_state_t;

/**
 * Inicializa la FSM con la cola de eventos de botones ya creada.
 */
void fsm_reaction_init(QueueHandle_t event_queue);

/**
 * Lanza la tarea de la FSM en background.
 */
void fsm_reaction_start_task(void);