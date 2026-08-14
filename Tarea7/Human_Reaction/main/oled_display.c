#include "oled_display.h"
#include "config.h"
#include "u8g2.h"
#include "driver/i2c.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "OLED";
static u8g2_t u8g2;

#define I2C_TIMEOUT_MS 1000

static uint8_t u8x8_byte_i2c(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
    static uint8_t buffer[32];
    static uint8_t buf_idx;

    switch (msg) {
    case U8X8_MSG_BYTE_SEND:
        memcpy(&buffer[buf_idx], arg_ptr, arg_int);
        buf_idx += arg_int;
        break;
    case U8X8_MSG_BYTE_INIT:
        break;
    case U8X8_MSG_BYTE_SET_DC:
        break;
    case U8X8_MSG_BYTE_START_TRANSFER:
        buf_idx = 0;
        break;
    case U8X8_MSG_BYTE_END_TRANSFER: {
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (u8x8_GetI2CAddress(u8x8) << 1) | I2C_MASTER_WRITE, true);
        i2c_master_write(cmd, buffer, buf_idx, true);
        i2c_master_stop(cmd);
        i2c_master_cmd_begin(OLED_I2C_PORT, cmd, pdMS_TO_TICKS(I2C_TIMEOUT_MS));
        i2c_cmd_link_delete(cmd);
        break;
    }
    default:
        return 0;
    }
    return 1;
}

static uint8_t u8x8_gpio_and_delay(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
    if (msg == U8X8_MSG_DELAY_MILLI) {
        vTaskDelay(pdMS_TO_TICKS(arg_int));
    }
    return 1;
}

static void i2c_bus_init(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = OLED_SDA_GPIO,
        .scl_io_num = OLED_SCL_GPIO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = OLED_I2C_FREQ_HZ,
    };
    i2c_param_config(OLED_I2C_PORT, &conf);
    i2c_driver_install(OLED_I2C_PORT, conf.mode, 0, 0, 0);
}

void oled_display_init(void)
{
    i2c_bus_init();

    u8g2_Setup_ssd1306_i2c_128x64_noname_f(&u8g2, U8G2_R0, u8x8_byte_i2c, u8x8_gpio_and_delay);
    u8g2_SetI2CAddress(&u8g2, 0x3C << 1);
    u8g2_InitDisplay(&u8g2);
    u8g2_SetPowerSave(&u8g2, 0);
    u8g2_ClearBuffer(&u8g2);
    u8g2_SendBuffer(&u8g2);

    ESP_LOGI(TAG, "OLED inicializado");
}

static void draw_header(const char *title)
{
    u8g2_ClearBuffer(&u8g2);
    u8g2_SetFont(&u8g2, u8g2_font_ncenB08_tr);
    u8g2_DrawStr(&u8g2, 0, 12, title);
    u8g2_DrawHLine(&u8g2, 0, 16, 128);
}

void oled_display_show_idle(void)
{
    draw_header("Reaction Timer");
    u8g2_SetFont(&u8g2, u8g2_font_6x10_tr);
    u8g2_DrawStr(&u8g2, 0, 35, "Presiona PB1");
    u8g2_DrawStr(&u8g2, 0, 50, "para empezar");
    u8g2_SendBuffer(&u8g2);
}

void oled_display_show_armado(void)
{
    draw_header("Preparando...");
    u8g2_SetFont(&u8g2, u8g2_font_6x10_tr);
    u8g2_DrawStr(&u8g2, 0, 35, "Espera la senal");
    u8g2_DrawStr(&u8g2, 0, 50, "No sueltes PB1");
    u8g2_SendBuffer(&u8g2);
}

void oled_display_show_senal(void)
{
    draw_header("YA!");
    u8g2_SetFont(&u8g2, u8g2_font_6x10_tr);
    u8g2_DrawStr(&u8g2, 0, 35, "Suelta PB1");
    u8g2_SendBuffer(&u8g2);
}

void oled_display_show_transicion(void)
{
    draw_header("Ahora...");
    u8g2_SetFont(&u8g2, u8g2_font_6x10_tr);
    u8g2_DrawStr(&u8g2, 0, 35, "Presiona PB2");
    u8g2_SendBuffer(&u8g2);
}

void oled_display_show_resultado(float reaccion_ms, float movimiento_ms, float total_ms)
{
    char line[32];

    draw_header("Resultado");
    u8g2_SetFont(&u8g2, u8g2_font_6x10_tr);

    snprintf(line, sizeof(line), "Reaccion: %.1f ms", reaccion_ms);
    u8g2_DrawStr(&u8g2, 0, 30, line);

    snprintf(line, sizeof(line), "Mov: %.1f ms", movimiento_ms);
    u8g2_DrawStr(&u8g2, 0, 43, line);

    snprintf(line, sizeof(line), "Total: %.1f ms", total_ms);
    u8g2_DrawStr(&u8g2, 0, 56, line);

    u8g2_SendBuffer(&u8g2);
}

void oled_display_show_falso_arranque(void)
{
    draw_header("Falso arranque!");
    u8g2_SetFont(&u8g2, u8g2_font_6x10_tr);
    u8g2_DrawStr(&u8g2, 0, 35, "Soltaste antes");
    u8g2_DrawStr(&u8g2, 0, 50, "de la senal");
    u8g2_SendBuffer(&u8g2);
}