#pragma once

#include <stdint.h>
#include "esp_err.h"
#include <stdbool.h>
#include "hal_gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct dev_light_sensor_s *dev_light_sensor_handle_t;

typedef struct {
    gpio_num_t do_pin;
} dev_light_sensor_config_t;

typedef struct {
    esp_err_t (*init)(const dev_light_sensor_config_t *cfg, dev_light_sensor_handle_t *out);
    esp_err_t (*deinit)(dev_light_sensor_handle_t h);
    esp_err_t (*get_status)(dev_light_sensor_handle_t h, bool *is_dark);
} dev_light_sensor_ops_t;

const dev_light_sensor_ops_t *dev_light_sensor_get_ops(void);

#ifdef __cplusplus
}
#endif
