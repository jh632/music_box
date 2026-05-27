#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "driver/gpio.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief GPIO 方向
 */
typedef enum {
    HAL_GPIO_DIR_OUTPUT = 0,
    HAL_GPIO_DIR_INPUT,
    HAL_GPIO_DIR_INPUT_INTR,  // 输入模式（带中断）
} hal_gpio_dir_t;

/**
 * @brief 中断触发类型（仅 HAL_GPIO_DIR_INPUT_INTR 时有效）
 */
typedef enum {
    HAL_GPIO_INTR_DISABLE = GPIO_INTR_DISABLE,
    HAL_GPIO_INTR_POSEDGE = GPIO_INTR_POSEDGE,
    HAL_GPIO_INTR_NEGEDGE = GPIO_INTR_NEGEDGE,
    HAL_GPIO_INTR_ANYEDGE = GPIO_INTR_ANYEDGE,
    HAL_GPIO_INTR_LOW_LEVEL = GPIO_INTR_LOW_LEVEL,
    HAL_GPIO_INTR_HIGH_LEVEL = GPIO_INTR_HIGH_LEVEL,
} hal_gpio_intr_type_t;

/**
 * @brief 单引脚配置结构体
 */
typedef struct {
    gpio_num_t pin;
    hal_gpio_dir_t dir;
    bool pull_up;
    bool pull_down;
    hal_gpio_intr_type_t intr_type;
} hal_gpio_config_t;

/**
 * @brief ISR 回调函数类型
 */
typedef void (*hal_gpio_isr_cb_t)(void *arg);

/**
 * @brief GPIO HAL 操作表
 */
typedef struct {
    esp_err_t (*init)(const hal_gpio_config_t *cfg);
    esp_err_t (*set_level)(gpio_num_t pin, uint32_t level);
    int (*get_level)(gpio_num_t pin);
    esp_err_t (*isr_service_install)(void);
    esp_err_t (*isr_handler_add)(gpio_num_t pin, hal_gpio_isr_cb_t cb, void *arg);
    esp_err_t (*isr_handler_remove)(gpio_num_t pin);
} hal_gpio_ops_t;

const hal_gpio_ops_t *hal_gpio_get_ops(void);

#ifdef __cplusplus
}
#endif
