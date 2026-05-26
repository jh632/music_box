#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include "driver/gpio.h"
#include "driver/rmt_common.h"
#include "driver/rmt_encoder.h"
#include "driver/rmt_tx.h"
#include "esp_err.h"
#include "esp_log.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct hal_rmt_tx_s *hal_rmt_tx_handle_t;

/**
 * @brief RMT TX 字节流编码配置
 */
typedef struct {
    gpio_num_t gpio_num;              /**< 输出引脚 */
    uint32_t resolution_hz;           /**< RMT 分辨率 */
    size_t mem_block_symbols;         /**< 通道内存块大小 */
    size_t trans_queue_depth;         /**< 传输队列深度 */
    uint32_t bit0_high_ns;            /**< bit0 高电平时间 */
    uint32_t bit0_low_ns;             /**< bit0 低电平时间 */
    uint32_t bit1_high_ns;            /**< bit1 高电平时间 */
    uint32_t bit1_low_ns;             /**< bit1 低电平时间 */
    uint32_t reset_ns;                /**< 帧结束低电平时间 */
    bool msb_first;                   /**< 是否高位先发 */
} hal_rmt_tx_config_t;

esp_err_t hal_rmt_tx_init(const hal_rmt_tx_config_t *cfg, hal_rmt_tx_handle_t *out);
esp_err_t hal_rmt_tx_deinit(hal_rmt_tx_handle_t h);
esp_err_t hal_rmt_tx_enable(hal_rmt_tx_handle_t h);
esp_err_t hal_rmt_tx_disable(hal_rmt_tx_handle_t h);
esp_err_t hal_rmt_tx_transmit(hal_rmt_tx_handle_t h,
                              const void *data,
                              size_t len,
                              int loop_count);
esp_err_t hal_rmt_tx_wait_done(hal_rmt_tx_handle_t h, int timeout_ms);

#ifdef __cplusplus
}
#endif
