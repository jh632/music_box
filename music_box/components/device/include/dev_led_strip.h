#pragma once

#include <stdint.h>
#include "driver/gpio.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct dev_led_strip_s *dev_led_strip_handle_t;

typedef struct {
    gpio_num_t gpio_num;
    uint32_t resolution_hz;
    size_t mem_block_symbols;
    size_t trans_queue_depth;
    uint32_t bit0_high_ns;
    uint32_t bit0_low_ns;
    uint32_t bit1_high_ns;
    uint32_t bit1_low_ns;
    uint32_t reset_ns;
    uint32_t num_pixels;
} dev_led_strip_config_t;

typedef struct {
    esp_err_t (*init)(const dev_led_strip_config_t *cfg, dev_led_strip_handle_t *out);
    esp_err_t (*deinit)(dev_led_strip_handle_t h);
    esp_err_t (*set_pixel)(dev_led_strip_handle_t h, uint32_t index, uint8_t r, uint8_t g, uint8_t b);
    esp_err_t (*refresh)(dev_led_strip_handle_t h);
    esp_err_t (*clear)(dev_led_strip_handle_t h);
    esp_err_t (*set_brightness)(dev_led_strip_handle_t h, uint8_t brightness);
} dev_led_strip_ops_t;

const dev_led_strip_ops_t *dev_led_strip_get_ops(void);

#ifdef __cplusplus
}
#endif
