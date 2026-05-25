/**
 * @file hal_i2c.h
 * @brief HAL 层 I2C master 封装接口
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "driver/i2c_master.h"
#include "esp_err.h"
#include "esp_log.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct hal_i2c_bus_s *hal_i2c_bus_handle_t;
typedef struct hal_i2c_dev_s *hal_i2c_dev_handle_t;

/**
 * @brief I2C master 总线配置
 */
typedef struct {
    i2c_port_num_t port;             /**< I2C 端口 */
    gpio_num_t sda_pin;              /**< SDA 引脚，由上层传入 */
    gpio_num_t scl_pin;              /**< SCL 引脚，由上层传入 */
    i2c_clock_source_t clk_source;   /**< 时钟源，通常使用 I2C_CLK_SRC_DEFAULT */
    uint8_t glitch_ignore_cnt;       /**< 毛刺过滤周期，常用 7 */
    int intr_priority;               /**< 中断优先级，0 表示默认 */
    size_t trans_queue_depth;        /**< 异步队列深度，不使用异步可为 0 */
    bool enable_internal_pullup;     /**< 是否使能内部上拉 */
} hal_i2c_bus_config_t;

/**
 * @brief I2C 设备配置
 */
typedef struct {
    uint16_t device_address;             /**< 设备地址，不包含读写位 */
    i2c_addr_bit_len_t address_bit_len;  /**< 地址位宽 */
    uint32_t scl_speed_hz;               /**< SCL 频率 */
    uint32_t scl_wait_us;                /**< 时钟拉伸等待时间，0 表示默认 */
    bool disable_ack_check;              /**< 是否关闭 ACK 检查 */
} hal_i2c_dev_config_t;

esp_err_t hal_i2c_bus_init(const hal_i2c_bus_config_t *cfg,
                           hal_i2c_bus_handle_t *out_bus);
esp_err_t hal_i2c_bus_deinit(hal_i2c_bus_handle_t bus);
esp_err_t hal_i2c_bus_add_device(hal_i2c_bus_handle_t bus,
                                 const hal_i2c_dev_config_t *cfg,
                                 hal_i2c_dev_handle_t *out_dev);
esp_err_t hal_i2c_device_remove(hal_i2c_dev_handle_t dev);
esp_err_t hal_i2c_transmit(hal_i2c_dev_handle_t dev,
                           const uint8_t *data,
                           size_t len,
                           int timeout_ms);
esp_err_t hal_i2c_transmit_receive(hal_i2c_dev_handle_t dev,
                                   const uint8_t *write_data,
                                   size_t write_len,
                                   uint8_t *read_data,
                                   size_t read_len,
                                   int timeout_ms);
esp_err_t hal_i2c_read_reg(hal_i2c_dev_handle_t dev,
                           uint8_t reg_addr,
                           uint8_t *data,
                           size_t len,
                           int timeout_ms);
esp_err_t hal_i2c_write_reg(hal_i2c_dev_handle_t dev,
                            uint8_t reg_addr,
                            const uint8_t *data,
                            size_t len,
                            int timeout_ms);

/**
 * @brief I2C HAL 函数表
 */
typedef struct {
    esp_err_t (*bus_init)(const hal_i2c_bus_config_t *cfg, hal_i2c_bus_handle_t *out_bus);
    esp_err_t (*bus_deinit)(hal_i2c_bus_handle_t bus);
    esp_err_t (*bus_add_device)(hal_i2c_bus_handle_t bus,
                                const hal_i2c_dev_config_t *cfg,
                                hal_i2c_dev_handle_t *out_dev);
    esp_err_t (*device_remove)(hal_i2c_dev_handle_t dev);
    esp_err_t (*transmit)(hal_i2c_dev_handle_t dev, const uint8_t *data, size_t len, int timeout_ms);
    esp_err_t (*transmit_receive)(hal_i2c_dev_handle_t dev,
                                  const uint8_t *write_data,
                                  size_t write_len,
                                  uint8_t *read_data,
                                  size_t read_len,
                                  int timeout_ms);
    esp_err_t (*read_reg)(hal_i2c_dev_handle_t dev, uint8_t reg_addr, uint8_t *data, size_t len, int timeout_ms);
    esp_err_t (*write_reg)(hal_i2c_dev_handle_t dev,
                           uint8_t reg_addr,
                           const uint8_t *data,
                           size_t len,
                           int timeout_ms);
} hal_i2c_ops_t;

const hal_i2c_ops_t *hal_i2c_get_ops(void);

#ifdef __cplusplus
}
#endif
