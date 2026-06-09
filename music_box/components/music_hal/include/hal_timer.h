#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "esp_timer.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*hal_timer_cb_t)(void *arg);
typedef struct hal_timer_s *hal_timer_handle_t;

typedef struct {
    hal_timer_cb_t callback;
    void *arg;
    const char *name;
    esp_timer_dispatch_t dispatch_method;
    bool skip_unhandled_events;
} hal_timer_config_t;

/**
 * @brief Timer HAL 操作表
 */
typedef struct {
    esp_err_t (*create)(const hal_timer_config_t *cfg, hal_timer_handle_t *out_handle);
    esp_err_t (*del)(hal_timer_handle_t handle);
    esp_err_t (*start_once)(hal_timer_handle_t handle, uint64_t timeout_us);
    esp_err_t (*start_periodic)(hal_timer_handle_t handle, uint64_t period_us);
    esp_err_t (*stop)(hal_timer_handle_t handle);
} hal_timer_ops_t;

const hal_timer_ops_t *hal_timer_get_ops(void);

#ifdef __cplusplus
}
#endif
