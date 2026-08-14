#pragma once

void oled_display_init(void);
void oled_display_show_idle(void);
void oled_display_show_armado(void);
void oled_display_show_senal(void);
void oled_display_show_transicion(void);
void oled_display_show_resultado(float reaccion_ms, float movimiento_ms, float total_ms);
void oled_display_show_falso_arranque(void);