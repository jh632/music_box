#include "hal_timer.h"

static const char *TAG = "HAL_TIMER";

struct hal_timer_s {
    esp_timer_handle_t timer;
};

esp_err_t hal_timer_create(const hal_timer_config_t *cfg, hal_timer_handle_t *out)
{
    if (cfg == NULL || out == NULL || cfg->callback == NULL) {
        ESP_LOGE(TAG, "invalid create arg");
        return ESP_ERR_INVALID_ARG;
    }

    hal_timer_handle_t h = calloc(1, sizeof(*h));
    if (h == NULL) {
        ESP_LOGE(TAG, "OOM");
        return ESP_ERR_NO_MEM;
    }

    esp_timer_create_args_t args = {
        .callback = cfg->callback,
        .arg = cfg->arg,
        .dispatch_method = cfg->dispatch_method,
        .name = cfg->name,
        .skip_unhandled_events = cfg->skip_unhandled_events,
    };

    esp_err_t ret = esp_timer_create(&args, &h->timer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "create failed: %s", esp_err_to_name(ret));
        free(h);
        return ret;
    }

    *out = h;
    return ESP_OK;
}

esp_err_t hal_timer_delete(hal_timer_handle_t h)
{
    if (h == NULL) {
        ESP_LOGE(TAG, "handle is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = esp_timer_delete(h->timer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "delete failed: %s", esp_err_to_name(ret));
        return ret;
    }

    free(h);
    return ESP_OK;
}

esp_err_t hal_timer_start_once(hal_timer_handle_t h, uint64_t timeout_us)
{
    if (h == NULL) {
        ESP_LOGE(TAG, "handle is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = esp_timer_start_once(h->timer, timeout_us);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "start once failed: %s", esp_err_to_name(ret));
    }
    return ret;
}

esp_err_t hal_timer_start_periodic(hal_timer_handle_t h, uint64_t period_us)
{
    if (h == NULL) {
        ESP_LOGE(TAG, "handle is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = esp_timer_start_periodic(h->timer, period_us);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "start periodic failed: %s", esp_err_to_name(ret));
    }
    return ret;
}

esp_err_t hal_timer_stop(hal_timer_handle_t h)
{
    if (h == NULL) {
        ESP_LOGE(TAG, "handle is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = esp_timer_stop(h->timer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "stop failed: %s", esp_err_to_name(ret));
    }
    return ret;
}
