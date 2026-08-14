#pragma once

/**
 * Inicializa el GPIO del buzzer como salida digital.
 */
void buzzer_init(void);

/**
 * Hace sonar el buzzer de forma bloqueante durante duration_ms.
 */
void buzzer_beep(int duration_ms);