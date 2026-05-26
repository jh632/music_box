#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief GPIO 方向
 */
typedef enum {
    HAL_GPIO_DIR_OUTPUT = 0,   
    HAL_GPIO_DIR_INPUT,       
    HAL_GPIO_DIR_INPUT_INTR,   //输入模式（带中断）
} hal_gpio_dir_t;

/**
 * @brief 中断触发类型（仅 HAL_GPIO_DIR_INPUT_INTR 时有效）
 */
typedef enum {
    HAL_GPIO_INTR_DISABLE   = GPIO_INTR_DISABLE,   
    HAL_GPIO_INTR_POSEDGE   = GPIO_INTR_POSEDGE,    
    HAL_GPIO_INTR_NEGEDGE   = GPIO_INTR_NEGEDGE,   
    HAL_GPIO_INTR_ANYEDGE   = GPIO_INTR_ANYEDGE,    /**< 任意沿 */
    HAL_GPIO_INTR_LOW_LEVEL = GPIO_INTR_LOW_LEVEL,  /**< 低电平 */
    HAL_GPIO_INTR_HIGH_LEVEL= GPIO_INTR_HIGH_LEVEL, /**< 高电平 */
} hal_gpio_intr_type_t;

/**
 * @brief 单引脚配置结构体
 */
typedef struct {
    gpio_num_t          pin;        
    hal_gpio_dir_t      dir;       
    bool                pull_up;   
    bool                pull_down;  
    hal_gpio_intr_type_t intr_type; /**< 中断类型（dir=INPUT_INTR 时有效） */
} hal_gpio_config_t;

/**
 * @brief ISR 回调函数类型
 *
 * @note 运行在中断上下文，禁止调用非 ISR 安全的函数
 * @param arg 注册时传入的用户参数
 */
typedef void (*hal_gpio_isr_cb_t)(void *arg);


/* ------------------------------------------------------------------ */
/*  接口声明                                                           */
/* ------------------------------------------------------------------ */

esp_err_t hal_gpio_init(const hal_gpio_config_t *cfg); //@param cfg  引脚配置，不可为 NULL
esp_err_t hal_gpio_set_level(gpio_num_t pin, uint32_t level);
int hal_gpio_get_level(gpio_num_t pin);
esp_err_t hal_gpio_isr_service_install(void); //安装 GPIO ISR 服务
esp_err_t hal_gpio_isr_handler_add(gpio_num_t pin, hal_gpio_isr_cb_t cb, void *arg);
esp_err_t hal_gpio_isr_handler_remove(gpio_num_t pin);

#ifdef __cplusplus
}
#endif
