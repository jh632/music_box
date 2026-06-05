#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "hal/adc_types.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    adc_unit_t unit;              /**< ADC 单元 (ADC_UNIT_1 / ADC_UNIT_2) */
    adc_channel_t channel;        /**< ADC 通道 */
    adc_atten_t atten;            /**< 衰减倍数 */
    adc_bitwidth_t bitwidth;      /**< 采样位宽 */
    bool enable_calibration;      /**< 是否启用内部校准 */
} hal_adc_config_t;

typedef struct {
    esp_err_t (*init)(const hal_adc_config_t *cfg);
    esp_err_t (*deinit)(void);
    esp_err_t (*read_raw)(int *out_raw);
    esp_err_t (*read_voltage)(int *out_mv);
} hal_adc_ops_t;

const hal_adc_ops_t *hal_adc_get_ops(void);

#ifdef __cplusplus
}
#endif