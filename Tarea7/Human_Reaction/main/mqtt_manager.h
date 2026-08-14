#pragma once
#include <stdbool.h>

void mqtt_manager_init(void);
bool mqtt_manager_is_connected(void);
void mqtt_manager_publish_estado(const char *estado);
void mqtt_manager_publish_resultado(float reaccion_ms, float movimiento_ms, float total_ms, const char *estado);