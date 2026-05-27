#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief I2C master 总线配置
 */
typedef struct {
    i2c_port_num_t port;
    gpio_num_t sda_pin;
    gpio_num_t scl_pin;
    i2c_clock_source_t clk_source;
    uint8_t glitch_ignore_cnt;
    int intr_priority;
    size_t trans_queue_depth;
    bool enable_internal_pullup;
} hal_i2c_bus_config_t;

/**
 * @brief I2C 设备配置
 */
typedef struct {
    uint16_t device_address;
    i2c_addr_bit_len_t address_bit_len;
    uint32_t scl_speed_hz;
    uint32_t scl_wait_us;
    bool disable_ack_check;
} hal_i2c_dev_config_t;

/**
 * @brief I2C HAL 操作表
 */
typedef struct {
    esp_err_t (*bus_init)(const hal_i2c_bus_config_t *cfg);
    esp_err_t (*bus_deinit)(void);
    esp_err_t (*device_init)(const hal_i2c_dev_config_t *cfg);
    esp_err_t (*device_deinit)(void);
    esp_err_t (*transmit)(const uint8_t *data, size_t len, int timeout_ms);
    esp_err_t (*transmit_receive)(const uint8_t *write_data,
                                  size_t write_len,
                                  uint8_t *read_data,
                                  size_t read_len,
                                  int timeout_ms);
    esp_err_t (*read_reg)(uint8_t reg_addr, uint8_t *data, size_t len, int timeout_ms);
    esp_err_t (*write_reg)(uint8_t reg_addr, const uint8_t *data, size_t len, int timeout_ms);
} hal_i2c_ops_t;

const hal_i2c_ops_t *hal_i2c_get_ops(void);

#ifdef __cplusplus
}
#endif
