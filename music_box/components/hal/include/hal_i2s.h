#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "driver/gpio.h"
#include "driver/i2s_std.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief I2S standard 数据格式
 */
typedef enum {
    HAL_I2S_STD_FMT_PHILIPS = 0,
    HAL_I2S_STD_FMT_MSB,
} hal_i2s_std_format_t;

/**
 * @brief I2S standard TX 配置
 */
typedef struct {
    i2s_port_t port;
    gpio_num_t bclk_pin;
    gpio_num_t ws_pin;
    gpio_num_t dout_pin;
    gpio_num_t mclk_pin;
    uint32_t sample_rate_hz;
    i2s_data_bit_width_t data_bit_width;
    i2s_slot_mode_t slot_mode;
    hal_i2s_std_format_t format;
    uint32_t dma_desc_num;
    uint32_t dma_frame_num;
} hal_i2s_config_t;

/**
 * @brief I2S HAL 操作表
 */
typedef struct {
    esp_err_t (*init)(const hal_i2s_config_t *cfg);
    esp_err_t (*deinit)(void);
    esp_err_t (*enable)(void);
    esp_err_t (*disable)(void);
    esp_err_t (*write)(const void *data,
                       size_t size,
                       size_t *bytes_written,
                       uint32_t timeout_ms);
    esp_err_t (*preload)(const void *data, size_t size, size_t *bytes_loaded);
} hal_i2s_ops_t;

const hal_i2s_ops_t *hal_i2s_get_ops(void);

#ifdef __cplusplus
}
#endif
