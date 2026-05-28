#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "driver/gpio.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief RMT TX 字节流编码配置
 */
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
    bool msb_first;
} hal_rmt_tx_config_t;

/**
 * @brief RMT HAL 操作表
 */
typedef struct {
    esp_err_t (*tx_init)(const hal_rmt_tx_config_t *cfg);
    esp_err_t (*tx_deinit)(void);
    esp_err_t (*tx_enable)(void);
    esp_err_t (*tx_disable)(void);
    esp_err_t (*tx_transmit)(const void *data, size_t len, int loop_count);
    esp_err_t (*tx_wait_done)(int timeout_ms);
} hal_rmt_ops_t;

const hal_rmt_ops_t *hal_rmt_get_ops(void);

#ifdef __cplusplus
}
#endif
