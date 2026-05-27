#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "esp_timer.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*hal_timer_cb_t)(void *arg);

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
    esp_err_t (*create)(const hal_timer_config_t *cfg);
    esp_err_t (*del)(void);
    esp_err_t (*start_once)(uint64_t timeout_us);
    esp_err_t (*start_periodic)(uint64_t period_us);
    esp_err_t (*stop)(void);
} hal_timer_ops_t;

const hal_timer_ops_t *hal_timer_get_ops(void);

#ifdef __cplusplus
}
#endif
