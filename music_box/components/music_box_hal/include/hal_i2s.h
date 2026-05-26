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
    i2s_port_t port;                      /**< I2S 端口，可使用 I2S_NUM_AUTO */
    gpio_num_t bclk_pin;                  /**< BCLK 引脚 */
    gpio_num_t ws_pin;                    /**< WS/LRCLK 引脚 */
    gpio_num_t dout_pin;                  /**< 数据输出引脚 */
    gpio_num_t mclk_pin;                  /**< MCLK 引脚，不使用时填 GPIO_NUM_NC */
    uint32_t sample_rate_hz;              /**< 采样率 */
    i2s_data_bit_width_t data_bit_width;  /**< 采样位宽 */
    i2s_slot_mode_t slot_mode;            /**< 单声道/双声道 */
    hal_i2s_std_format_t format;          /**< standard 格式 */
    uint32_t dma_desc_num;                /**< DMA 描述符数量，0 使用 IDF 默认 */
    uint32_t dma_frame_num;               /**< 每个 DMA 描述符帧数，0 使用 IDF 默认 */
} hal_i2s_config_t;

esp_err_t hal_i2s_init(const hal_i2s_config_t *cfg);
esp_err_t hal_i2s_deinit(void);
esp_err_t hal_i2s_enable(void);
esp_err_t hal_i2s_disable(void);
esp_err_t hal_i2s_write(const void *data,
                        size_t size,
                        size_t *bytes_written,
                        uint32_t timeout_ms);
esp_err_t hal_i2s_preload(const void *data,
                          size_t size,
                          size_t *bytes_loaded);

#ifdef __cplusplus
}
#endif
