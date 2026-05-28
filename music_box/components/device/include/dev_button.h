#pragma once

#include <stdint.h>
#include <stddef.h>
#include "driver/gpio.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct dev_button_s *dev_button_handle_t;

typedef enum {
    DEV_BTN_EVT_SHORT_PRESS = 0,
    DEV_BTN_EVT_LONG_PRESS,
} dev_button_event_t;

typedef void (*dev_button_cb_t)(gpio_num_t pin, dev_button_event_t event, void *arg);

typedef struct {
    gpio_num_t pin;
    bool pull_up;
    uint32_t long_press_ms;
    uint32_t debounce_ms;
} dev_button_pin_config_t;

typedef struct {
    dev_button_pin_config_t *pins;
    size_t num_pins;
} dev_button_config_t;

typedef struct {
    esp_err_t (*init)(const dev_button_config_t *cfg, dev_button_handle_t *out);
    esp_err_t (*deinit)(dev_button_handle_t h);
    esp_err_t (*register_callback)(dev_button_handle_t h, dev_button_cb_t cb, void *arg);
} dev_button_ops_t;

const dev_button_ops_t *dev_button_get_ops(void);

#ifdef __cplusplus
}
#endif
