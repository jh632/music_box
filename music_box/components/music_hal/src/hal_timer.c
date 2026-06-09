#include "hal_timer.h"

#include "esp_log.h"

static const char *TAG = "HAL_TIMER";

typedef struct hal_timer_s {
    esp_timer_handle_t timer;
} hal_timer_t;

static esp_err_t s_hal_timer_create(const hal_timer_config_t *cfg, hal_timer_handle_t *out_handle)
{
    if (cfg == NULL || cfg->callback == NULL || out_handle == NULL) {
        ESP_LOGE(TAG, "invalid create arg");
        return ESP_ERR_INVALID_ARG;
    }

    hal_timer_t *handle = calloc(1, sizeof(*handle));
    if (handle == NULL) {
        ESP_LOGE(TAG, "out of memory");
        return ESP_ERR_NO_MEM;
    }

    esp_timer_create_args_t args = {
        .callback = cfg->callback,
        .arg = cfg->arg,
        .dispatch_method = cfg->dispatch_method,
        .name = cfg->name,
        .skip_unhandled_events = cfg->skip_unhandled_events,
    };

    esp_err_t ret = esp_timer_create(&args, &handle->timer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "create failed: %s", esp_err_to_name(ret));
        free(handle);
        return ret;
    }

    *out_handle = handle;
    return ESP_OK;
}

static esp_err_t s_hal_timer_delete(hal_timer_handle_t handle)
{
    if (handle == NULL || handle->timer == NULL) {
        ESP_LOGE(TAG, "timer is not created");
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = esp_timer_delete(handle->timer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "delete failed: %s", esp_err_to_name(ret));
        return ret;
    }

    handle->timer = NULL;
    free(handle);
    return ESP_OK;
}

static esp_err_t s_hal_timer_start_once(hal_timer_handle_t handle, uint64_t timeout_us)
{
    if (handle == NULL || handle->timer == NULL) {
        ESP_LOGE(TAG, "timer is not created");
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = esp_timer_start_once(handle->timer, timeout_us);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "start once failed: %s", esp_err_to_name(ret));
    }
    return ret;
}

static esp_err_t s_hal_timer_start_periodic(hal_timer_handle_t handle, uint64_t period_us)
{
    if (handle == NULL || handle->timer == NULL) {
        ESP_LOGE(TAG, "timer is not created");
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = esp_timer_start_periodic(handle->timer, period_us);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "start periodic failed: %s", esp_err_to_name(ret));
    }
    return ret;
}

static esp_err_t s_hal_timer_stop(hal_timer_handle_t handle)
{
    if (handle == NULL || handle->timer == NULL) {
        ESP_LOGE(TAG, "timer is not created");
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = esp_timer_stop(handle->timer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "stop failed: %s", esp_err_to_name(ret));
    }
    return ret;
}

static const hal_timer_ops_t s_hal_timer_ops = {
    .create = s_hal_timer_create,
    .del = s_hal_timer_delete,
    .start_once = s_hal_timer_start_once,
    .start_periodic = s_hal_timer_start_periodic,
    .stop = s_hal_timer_stop,
};

const hal_timer_ops_t *hal_timer_get_ops(void)
{
    return &s_hal_timer_ops;
}