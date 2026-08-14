#pragma once

/* ============================================================
 *  REACTION TIMER - Configuración centralizada de hardware
 *  ESP32-S3 N16R8
 * ============================================================ */

/* ---------------- Botones ---------------- */
#define PB1_GPIO        GPIO_NUM_4
#define PB2_GPIO        GPIO_NUM_5

/* ---------------- LED de señal (WS2812) ---------------- */
#define WS2812_GPIO     GPIO_NUM_48

/* ---------------- Buzzer ---------------- */
#define BUZZER_GPIO     GPIO_NUM_17

/* ---------------- OLED SSD1306 (I2C) ---------------- */
#define OLED_SDA_GPIO    GPIO_NUM_8
#define OLED_SCL_GPIO    GPIO_NUM_9
#define OLED_I2C_PORT    I2C_NUM_0
#define OLED_I2C_FREQ_HZ 400000

/* ---------------- Timings ---------------- */
#define DEBOUNCE_MS          20
#define POLL_PERIOD_MS       10
#define RANDOM_DELAY_MIN_MS  1500
#define RANDOM_DELAY_MAX_MS  4500

/* ---------------- WiFi ---------------- */
#define WIFI_SSID       "Rapidito"
#define WIFI_PASS       "Adm1N2584km"

/* ---------------- MQTT ---------------- */
#define MQTT_BROKER_URI       "mqtt://test.mosquitto.org:1883"
#define MQTT_TOPIC_RESULTADO  "reaction/resultado"
#define MQTT_TOPIC_ESTADO     "reaction/estado"