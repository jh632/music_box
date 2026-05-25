/**
 * @file hal_timer.h
 * @brief HAL 层 esp_timer 封装接口
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct hal_timer_s *hal_timer_handle_t;
typedef void (*hal_timer_cb_t)(void *arg);

/**
 * @brief 定时器配置
 */
typedef struct {
    hal_timer_cb_t callback;                 /**< 超时回调 */
    void *arg;                               /**< 回调参数 */
    const char *name;                        /**< 定时器名称，可为 NULL */
    esp_timer_dispatch_t dispatch_method;    /**< 回调派发方式 */
    bool skip_unhandled_events;              /**< 轻睡眠后是否跳过未处理事件 */
} hal_timer_config_t;

esp_err_t hal_timer_create(const hal_timer_config_t *cfg, hal_timer_handle_t *out);
esp_err_t hal_timer_delete(hal_timer_handle_t h);
esp_err_t hal_timer_start_once(hal_timer_handle_t h, uint64_t timeout_us);
esp_err_t hal_timer_start_periodic(hal_timer_handle_t h, uint64_t period_us);
esp_err_t hal_timer_stop(hal_timer_handle_t h);

/**
 * @brief Timer HAL 函数表
 */
typedef struct {
    esp_err_t (*create)(const hal_timer_config_t *cfg, hal_timer_handle_t *out);
    esp_err_t (*del)(hal_timer_handle_t h);
    esp_err_t (*start_once)(hal_timer_handle_t h, uint64_t timeout_us);
    esp_err_t (*start_periodic)(hal_timer_handle_t h, uint64_t period_us);
    esp_err_t (*stop)(hal_timer_handle_t h);
} hal_timer_ops_t;

const hal_timer_ops_t *hal_timer_get_ops(void);

#ifdef __cplusplus
}
#endif
