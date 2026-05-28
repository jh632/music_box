#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "hal/adc_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct dev_light_sensor_s *dev_light_sensor_handle_t;

typedef struct {
    adc_unit_t adc_unit;
    adc_channel_t adc_channel;
    adc_atten_t atten;
    adc_bitwidth_t bitwidth;
    uint32_t filter_window;
} dev_light_sensor_config_t;

typedef struct {
    esp_err_t (*init)(const dev_light_sensor_config_t *cfg, dev_light_sensor_handle_t *out);
    esp_err_t (*deinit)(dev_light_sensor_handle_t h);
    esp_err_t (*read_raw)(dev_light_sensor_handle_t h, int *raw);
    esp_err_t (*read_lux)(dev_light_sensor_handle_t h, float *lux);
} dev_light_sensor_ops_t;

const dev_light_sensor_ops_t *dev_light_sensor_get_ops(void);

#ifdef __cplusplus
}
#endif
