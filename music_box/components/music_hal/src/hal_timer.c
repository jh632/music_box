#include "hal_timer.h"

#include "esp_log.h"

static const char *TAG = "HAL_TIMER";

static esp_timer_handle_t s_timer;

static esp_err_t s_hal_timer_create(const hal_timer_config_t *cfg)
{
    if (cfg == NULL || cfg->callback == NULL) {
        ESP_LOGE(TAG, "invalid create arg");
        return ESP_ERR_INVALID_ARG;
    }
    if (s_timer != NULL) {
        ESP_LOGE(TAG, "timer already created");
        return ESP_ERR_INVALID_STATE;
    }

    esp_timer_create_args_t args = {
        .callback = cfg->callback,
        .arg = cfg->arg,
        .dispatch_method = cfg->dispatch_method,
        .name = cfg->name,
        .skip_unhandled_events = cfg->skip_unhandled_events,
    };

    esp_err_t ret = esp_timer_create(&args, &s_timer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "create failed: %s", esp_err_to_name(ret));
    }
    return ret;
}

static esp_err_t s_hal_timer_delete(void)
{
    if (s_timer == NULL) {
        ESP_LOGE(TAG, "timer is not created");
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = esp_timer_delete(s_timer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "delete failed: %s", esp_err_to_name(ret));
        return ret;
    }

    s_timer = NULL;
    return ESP_OK;
}

static esp_err_t s_hal_timer_start_once(uint64_t timeout_us)
{
    if (s_timer == NULL) {
        ESP_LOGE(TAG, "timer is not created");
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = esp_timer_start_once(s_timer, timeout_us);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "start once failed: %s", esp_err_to_name(ret));
    }
    return ret;
}

static esp_err_t s_hal_timer_start_periodic(uint64_t period_us)
{
    if (s_timer == NULL) {
        ESP_LOGE(TAG, "timer is not created");
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = esp_timer_start_periodic(s_timer, period_us);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "start periodic failed: %s", esp_err_to_name(ret));
    }
    return ret;
}

static esp_err_t s_hal_timer_stop(void)
{
    if (s_timer == NULL) {
        ESP_LOGE(TAG, "timer is not created");
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = esp_timer_stop(s_timer);
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
