#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include "esp_err.h"
#include "esp_log.h"
#include "hal_gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct dev_led_strip_s *dev_led_strip_handle_t;

typedef struct {
    const gpio_num_t *gpio_nums;
    uint32_t          led_count;
    uint8_t           active_level;
} dev_led_strip_config_t;

typedef struct {
    esp_err_t (*init)(const dev_led_strip_config_t *cfg, dev_led_strip_handle_t *out);
    esp_err_t (*deinit)(dev_led_strip_handle_t h);
    esp_err_t (*on)(dev_led_strip_handle_t h, uint32_t index);
    esp_err_t (*off)(dev_led_strip_handle_t h, uint32_t index);
    esp_err_t (*set)(dev_led_strip_handle_t h, uint32_t index, bool on);
    esp_err_t (*all_on)(dev_led_strip_handle_t h);
    esp_err_t (*all_off)(dev_led_strip_handle_t h);
    esp_err_t (*toggle)(dev_led_strip_handle_t h, uint32_t index);
    esp_err_t (*chase_step)(dev_led_strip_handle_t h);
} dev_led_strip_ops_t;

const dev_led_strip_ops_t *dev_led_strip_get_ops(void);

#ifdef __cplusplus
}
#endif
