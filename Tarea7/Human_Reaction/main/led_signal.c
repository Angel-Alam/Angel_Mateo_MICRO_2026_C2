#include "led_signal.h"
#include "config.h"
#include "led_strip.h"
#include "esp_log.h"

static led_strip_handle_t led_strip;

void led_signal_init(void)
{
    led_strip_config_t strip_config = {
        .strip_gpio_num = WS2812_GPIO,
        .max_leds = 1,
    };
    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = 10 * 1000 * 1000,
    };
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
    led_strip_clear(led_strip);
}

void led_signal_set_color(uint8_t r, uint8_t g, uint8_t b)
{
    led_strip_set_pixel(led_strip, 0, r, g, b);
    led_strip_refresh(led_strip);
}

void led_signal_off(void)
{
    led_strip_clear(led_strip);
}