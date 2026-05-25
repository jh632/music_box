/**
 * @file hal_gpio.h
 * @brief HAL 层 GPIO 封装接口
 *
 * 职责：
 *   - 单引脚初始化（输出 / 输入 / 输入+中断）
 *   - 电平读写
 *   - ISR 服务安装与回调注册/注销
 *
 * 规范：
 *   - 不出现具体芯片型号
 *   - 引脚号通过 hal_gpio_config_t 传入，不硬编码
 *   - 调用方只 #include "hal_gpio.h"，不直接使用 driver/gpio.h
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/*  类型定义                                                           */
/* ------------------------------------------------------------------ */

/**
 * @brief GPIO 方向
 */
typedef enum {
    HAL_GPIO_DIR_OUTPUT = 0,   /**< 输出模式 */
    HAL_GPIO_DIR_INPUT,        /**< 输入模式（无中断） */
    HAL_GPIO_DIR_INPUT_INTR,   /**< 输入模式（带中断） */
} hal_gpio_dir_t;

/**
 * @brief 中断触发类型（仅 HAL_GPIO_DIR_INPUT_INTR 时有效）
 */
typedef enum {
    HAL_GPIO_INTR_DISABLE   = GPIO_INTR_DISABLE,    /**< 禁用中断 */
    HAL_GPIO_INTR_POSEDGE   = GPIO_INTR_POSEDGE,    /**< 上升沿 */
    HAL_GPIO_INTR_NEGEDGE   = GPIO_INTR_NEGEDGE,    /**< 下降沿 */
    HAL_GPIO_INTR_ANYEDGE   = GPIO_INTR_ANYEDGE,    /**< 任意沿 */
    HAL_GPIO_INTR_LOW_LEVEL = GPIO_INTR_LOW_LEVEL,  /**< 低电平 */
    HAL_GPIO_INTR_HIGH_LEVEL= GPIO_INTR_HIGH_LEVEL, /**< 高电平 */
} hal_gpio_intr_type_t;

/**
 * @brief 单引脚配置结构体
 */
typedef struct {
    gpio_num_t          pin;        /**< GPIO 引脚号，由调用方传入 */
    hal_gpio_dir_t      dir;        /**< 方向 */
    bool                pull_up;    /**< 使能内部上拉 */
    bool                pull_down;  /**< 使能内部下拉 */
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

/**
 * @brief 初始化单个 GPIO 引脚
 *
 * @param cfg  引脚配置，不可为 NULL
 * @return ESP_OK 成功，ESP_ERR_INVALID_ARG 参数错误
 */
esp_err_t hal_gpio_init(const hal_gpio_config_t *cfg);

/**
 * @brief 设置输出电平
 *
 * @param pin   GPIO 引脚号
 * @param level 0 = 低，非 0 = 高
 * @return ESP_OK 成功
 */
esp_err_t hal_gpio_set_level(gpio_num_t pin, uint32_t level);

/**
 * @brief 读取引脚电平
 *
 * @param pin  GPIO 引脚号
 * @return 0 = 低，1 = 高
 */
int hal_gpio_get_level(gpio_num_t pin);

/**
 * @brief 安装 GPIO ISR 服务（全局只需调用一次）
 *
 * @return ESP_OK 成功，ESP_ERR_INVALID_STATE 已安装
 */
esp_err_t hal_gpio_isr_service_install(void);

/**
 * @brief 为指定引脚注册 ISR 回调
 *
 * @param pin  GPIO 引脚号（需已配置为 INPUT_INTR 模式）
 * @param cb   回调函数，运行在 ISR 上下文
 * @param arg  传递给回调的用户参数
 * @return ESP_OK 成功
 */
esp_err_t hal_gpio_isr_handler_add(gpio_num_t pin, hal_gpio_isr_cb_t cb, void *arg);

/**
 * @brief 注销指定引脚的 ISR 回调
 *
 * @param pin  GPIO 引脚号
 * @return ESP_OK 成功
 */
esp_err_t hal_gpio_isr_handler_remove(gpio_num_t pin);

/**
 * @brief GPIO HAL 函数表，供上层按 ops 模式调用
 */
typedef struct {
    esp_err_t (*init)(const hal_gpio_config_t *cfg);
    esp_err_t (*set_level)(gpio_num_t pin, uint32_t level);
    int (*get_level)(gpio_num_t pin);
    esp_err_t (*isr_service_install)(void);
    esp_err_t (*isr_handler_add)(gpio_num_t pin, hal_gpio_isr_cb_t cb, void *arg);
    esp_err_t (*isr_handler_remove)(gpio_num_t pin);
} hal_gpio_ops_t;

/**
 * @brief 获取 GPIO HAL 函数表
 *
 * @return GPIO HAL 函数表单例
 */
const hal_gpio_ops_t *hal_gpio_get_ops(void);

#ifdef __cplusplus
}
#endif
