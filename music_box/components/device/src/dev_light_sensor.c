#include "dev_light_sensor.h"

#include <stdlib.h>

#include "esp_log.h"

static const char *TAG = "DEV_LIGHT_SENSOR";

struct dev_light_sensor_s {
    const hal_gpio_ops_t *gpio_ops;
    gpio_num_t            do_pin;
};

/* 普通光敏模块的 DO 一般是低电平表示触发，这里按“低电平=暗”处理 */
static bool s_is_dark_level(int level)
{
    return level == 0;
}

static esp_err_t dev_light_sensor_init(const dev_light_sensor_config_t *cfg,
                                       dev_light_sensor_handle_t       *out)
{
    if (cfg == NULL || out == NULL || cfg->do_pin < 0 || cfg->do_pin >= GPIO_NUM_MAX) {
        return ESP_ERR_INVALID_ARG;
    }

    const hal_gpio_ops_t *gpio_ops = hal_gpio_get_ops();
    if (gpio_ops == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    dev_light_sensor_handle_t h = calloc(1, sizeof(*h));
    if (h == NULL) {
        ESP_LOGE(TAG, "Out of memory");
        return ESP_ERR_NO_MEM;
    }

    hal_gpio_config_t gpio_cfg = {
        .pin       = cfg->do_pin,
        .dir       = HAL_GPIO_DIR_INPUT,
        .pull_up   = false,
        .pull_down = false,
        .intr_type = HAL_GPIO_INTR_DISABLE,
    };

    esp_err_t ret = gpio_ops->init(&gpio_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "GPIO %d init failed", cfg->do_pin);
        free(h);
        return ret;
    }

    h->gpio_ops = gpio_ops;
    h->do_pin   = cfg->do_pin;

    *out = h;
    ESP_LOGI(TAG, "Light sensor created on GPIO %d", h->do_pin);
    return ESP_OK;
}

static esp_err_t dev_light_sensor_deinit(dev_light_sensor_handle_t h)
{
    if (h == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    free(h);
    return ESP_OK;
}

static esp_err_t dev_light_sensor_get_status(dev_light_sensor_handle_t h, bool *is_dark)
{
    if (h == NULL || is_dark == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    int level = h->gpio_ops->get_level(h->do_pin);
    *is_dark = s_is_dark_level(level);
    return ESP_OK;
}

static const dev_light_sensor_ops_t light_sensor_ops = {
    .init       = dev_light_sensor_init,
    .deinit     = dev_light_sensor_deinit,
    .get_status = dev_light_sensor_get_status,
};

const dev_light_sensor_ops_t *dev_light_sensor_get_ops(void)
{
    return &light_sensor_ops;
}
