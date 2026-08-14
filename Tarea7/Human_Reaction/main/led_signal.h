#pragma once
#include <stdint.h>

/**
 * Inicializa el WS2812 (GPIO48, 1 LED).
 */
void led_signal_init(void);

/**
 * Fija el color del LED de señal (RGB, 0-255 cada canal).
 */
void led_signal_set_color(uint8_t r, uint8_t g, uint8_t b);

/**
 * Apaga el LED de señal.
 */
void led_signal_off(void);